#include <algorithm/base64.h>

#include <common/string_oprs.h>
#include <log/log_wrapper.h>


#include "etcd_cluster.h"
#include "etcd_watcher.h"


namespace atframe {
    namespace component {
        etcd_watcher::etcd_watcher(etcd_cluster &owner, const std::string &path, const std::string &range_end, constrict_helper_t &)
            : owner_(&owner), path_(path), range_end_(range_end) {
            rpc_.retry_interval = std::chrono::seconds(15); // 重试间隔15秒
            rpc_.request_timeout = std::chrono::hours(1);   // 一小时超时时间，相当于每小时重新拉取数据
            rpc_.watcher_next_request_time = std::chrono::system_clock::from_time_t(0);
            rpc_.enable_progress_notify = true;
            rpc_.enable_prev_kv = false;
            rpc_.is_actived = false;
            rpc_.last_revision = 0;
        }

        etcd_watcher::ptr_t etcd_watcher::create(etcd_cluster &owner, const std::string &path, const std::string &range_end) {
            constrict_helper_t h;
            return std::make_shared<etcd_watcher>(owner, path, range_end, h);
        }

        void etcd_watcher::close() {
            if (rpc_.rpc_opr_) {
                WLOGDEBUG("Etcd watcher %p cancel http request.", this);
                rpc_.rpc_opr_->set_on_complete(NULL);
                rpc_.rpc_opr_->set_on_write(NULL);
                rpc_.rpc_opr_->stop();
                rpc_.rpc_opr_.reset();
            }
            rpc_.is_actived = false;
            rpc_.last_revision = 0;
        }

        const std::string &etcd_watcher::get_path() const { return path_; }

        void etcd_watcher::active() {
            rpc_.is_actived = true;
            process();
        }

        void etcd_watcher::process() {
            if (rpc_.rpc_opr_) {
                return;
            }

            rpc_.is_actived = false;

            if (rpc_.watcher_next_request_time > util::time::time_utility::now()) {
                return;
            }

            // ask for revision first
            if (0 == rpc_.last_revision) {
                // create range request
                rpc_.rpc_opr_ = owner_->create_request_kv_get(path_, range_end_);
                if (!rpc_.rpc_opr_) {
                    WLOGERROR("Etcd watcher %p create range request to %s failed", this, path_.c_str());
                    rpc_.watcher_next_request_time = util::time::time_utility::now() + rpc_.retry_interval;
                    return;
                }

                rpc_.rpc_opr_->set_priv_data(this);
                rpc_.rpc_opr_->set_on_complete(libcurl_callback_on_range_completed);

                int res = rpc_.rpc_opr_->start(util::network::http_request::method_t::EN_MT_POST, false);
                if (res != 0) {
                    rpc_.rpc_opr_->set_on_complete(NULL);
                    WLOGERROR("Etcd watcher %p start request to %s failed, res: %d", this, rpc_.rpc_opr_->get_url().c_str(), res);
                    rpc_.rpc_opr_.reset();
                } else {
                    WLOGDEBUG("Etcd watcher %p start request to %s success.", this, rpc_.rpc_opr_->get_url().c_str());
                }

                return;
            }

            // create watcher request
            rpc_.rpc_opr_ = owner_->create_request_watch(path_, range_end_, rpc_.last_revision, rpc_.enable_prev_kv, rpc_.enable_progress_notify);
            if (!rpc_.rpc_opr_) {
                WLOGERROR("Etcd watcher %p create watch request to %s failed", this, path_.c_str());
                rpc_.watcher_next_request_time = util::time::time_utility::now() + rpc_.retry_interval;
                return;
            }

            rpc_.rpc_opr_->set_priv_data(this);
            rpc_.rpc_opr_->set_on_complete(libcurl_callback_on_watch_completed);
            rpc_.rpc_opr_->set_on_write(libcurl_callback_on_watch_write);
            rpc_.rpc_opr_->set_opt_timeout(static_cast<time_t>(std::chrono::duration_cast<std::chrono::milliseconds>(rpc_.request_timeout).count()));

            rpc_data_stream_.str("");

            int res = rpc_.rpc_opr_->start(util::network::http_request::method_t::EN_MT_POST, false);
            if (res != 0) {
                rpc_.rpc_opr_->set_on_complete(NULL);
                rpc_.rpc_opr_->set_on_write(NULL);
                WLOGERROR("Etcd watcher %p start request to %s failed, res: %d", this, rpc_.rpc_opr_->get_url().c_str(), res);
                rpc_.rpc_opr_.reset();
            } else {
                WLOGDEBUG("Etcd watcher %p start request to %s success.", this, rpc_.rpc_opr_->get_url().c_str());
            }

            return;
        }

        int etcd_watcher::libcurl_callback_on_range_completed(util::network::http_request &req) {
            etcd_watcher *self = reinterpret_cast<etcd_watcher *>(req.get_priv_data());
            if (NULL == self) {
                WLOGERROR("Etcd watcher range request shouldn't has request without private data");
                return 0;
            }
            self->rpc_.rpc_opr_.reset();

            // 服务器错误则过一段时间后重试
            if (0 != req.get_error_code() ||
                util::network::http_request::status_code_t::EN_ECG_SUCCESS != util::network::http_request::get_status_code_group(req.get_response_code())) {

                WLOGERROR("Etcd watcher %p range request failed, error code: %d, http code: %d\n%s", self, req.get_error_code(), req.get_response_code(),
                          req.get_error_msg());

                self->rpc_.watcher_next_request_time = util::time::time_utility::now() + self->rpc_.retry_interval;

                // 立刻开启下一次watch
                self->active();
                return 0;
            }

            std::string http_content;
            req.get_response_stream().str().swap(http_content);
            WLOGDEBUG("Etcd watcher %p got range http response: %s", self, http_content.c_str());

            rapidjson::Document doc;
            doc.Parse(http_content.c_str(), http_content.size());

            // unpack header
            etcd_response_header header;
            {
                header.revision = 0;
                rapidjson::Document::MemberIterator res = doc.FindMember("header");
                if (res != doc.MemberEnd()) {
                    etcd_packer::unpack(header, res->value);
                }
            }

            if (0 == header.revision) {
                WLOGERROR("Etcd watcher %p got range response without header", self);

                self->rpc_.watcher_next_request_time = util::time::time_utility::now() + self->rpc_.retry_interval;
                self->active();
                return 0;
            }

            // save revision
            self->rpc_.last_revision = header.revision;

            // first event
            response_t response;
            response.watch_id = 0;
            response.created = false;
            response.canceled = false;
            response.compact_revision = 0;
            {
                rapidjson::Document::MemberIterator res = doc.FindMember("kvs");

                if (doc.MemberEnd() != res) {
                    size_t reverse_sz = 0;
                    etcd_packer::unpack_int(res->value, "count", reverse_sz);
                    if (0 == reverse_sz) {
                        reverse_sz = 64;
                    }
                    response.events.reserve(reverse_sz);

                    rapidjson::Document::Array all_events = res->value.GetArray();
                    for (rapidjson::Document::Array::ValueIterator iter = all_events.Begin(); iter != all_events.End(); ++iter) {
                        response.events.push_back(event_t());
                        event_t &evt = response.events.back();

                        evt.evt_type = etcd_watch_event::PUT; // 查询的结果都认为是PUT
                        etcd_packer::unpack(evt.kv, *iter);
                    }
                }
            }

            // trigger event
            if (self->evt_handle_) {
                self->evt_handle_(header, response);
            }

            // reset request time to invoke watch request immediately
            self->rpc_.watcher_next_request_time = util::time::time_utility::now();

            // 立刻开启下一次watch
            self->active();
            return 0;
        }

        int etcd_watcher::libcurl_callback_on_watch_completed(util::network::http_request &req) {
            etcd_watcher *self = reinterpret_cast<etcd_watcher *>(req.get_priv_data());
            if (NULL == self) {
                WLOGERROR("Etcd watcher watch request shouldn't has request without private data");
                return 0;
            }
            self->rpc_.rpc_opr_.reset();

            // 服务器错误则过一段时间后重试
            if (0 != req.get_error_code() ||
                util::network::http_request::status_code_t::EN_ECG_SUCCESS != util::network::http_request::get_status_code_group(req.get_response_code())) {

                // timeout是正常的保活流程
                if (CURLE_OPERATION_TIMEDOUT != req.get_error_code()) {
                    WLOGERROR("Etcd watcher %p watch request failed, error code: %d, http code: %d\n%s", self, req.get_error_code(), req.get_response_code(),
                              req.get_error_msg());

                    self->rpc_.watcher_next_request_time = util::time::time_utility::now() + self->rpc_.retry_interval;

                } else {
                    WLOGDEBUG("Etcd watcher %p watch request finished, start another request later, msg: %s.", self, req.get_error_msg());
                    self->rpc_.watcher_next_request_time = util::time::time_utility::now();
                }

                // 立刻开启下一次watch
                self->active();
                return 0;
            }

            WLOGDEBUG("Etcd watcher %p got watch http response", self);

            // 立刻开启下一次watch
            self->active();
            return 0;
        }

        int etcd_watcher::libcurl_callback_on_watch_write(util::network::http_request &req, const char *inbuf, size_t inbufsz, const char *&outbuf,
                                                          size_t &outbufsz) {
            // etcd_watcher 模块内消耗掉缓冲区，不需要写出到通用缓冲区了
            outbuf = NULL;
            outbufsz = 0;

            etcd_watcher *self = reinterpret_cast<etcd_watcher *>(req.get_priv_data());
            if (NULL == self) {
                WLOGERROR("Etcd watcher watch request shouldn't has request without private data");
                return 0;
            }

            if (inbuf == NULL || 0 == inbufsz) {
                WLOGDEBUG("Etcd watcher %p got http trunk without data", self);
                return 0;
            }

            while (inbufsz > 0) {
                bool need_process = false;
                for (size_t i = 0; i < inbufsz; ++i) {
                    if (inbuf[i] == 0 || inbuf[i] == '\n') {
                        self->rpc_data_stream_.write(inbuf, i);
                        inbuf += i + 1;
                        inbufsz -= i + 1;
                        need_process = true;
                        break;
                    }
                }

                if (!need_process) {
                    self->rpc_data_stream_.write(inbuf, inbufsz);
                    break;
                }

                // 如果lease不存在（没有TTL）则启动创建流程
                rapidjson::Document doc;
                std::string value_json;
                self->rpc_data_stream_.str().swap(value_json);
                self->rpc_data_stream_.str("");

                WLOGDEBUG("Etcd watcher %p got http trunk: %s", self, value_json.c_str());
                doc.Parse(value_json.c_str(), value_json.size());

                // 忽略空数据
                if (!doc.IsObject()) {
                    continue;
                }

                rapidjson::Value root = doc.GetObject();
                rapidjson::Value *result = &root;
                {
                    rapidjson::Document::MemberIterator res = root.FindMember("result");
                    if (res != root.MemberEnd()) {
                        result = &res->value;
                    }
                }

                // unpack header
                etcd_response_header header;
                {
                    rapidjson::Document::MemberIterator res = result->FindMember("header");
                    if (res != result->MemberEnd()) {
                        etcd_packer::unpack(header, res->value);
                        // save revision
                        if (0 != header.revision) {
                            self->rpc_.last_revision = header.revision;
                        }
                    } else {
                        WLOGERROR("Etcd watcher %p got http trunk without header", self);
                    }
                }

                response_t response;
                // decode basic info
                etcd_packer::unpack_int(*result, "watch_id", response.watch_id);
                etcd_packer::unpack_int(*result, "compact_revision", response.compact_revision);
                etcd_packer::unpack_bool(*result, "created", response.created);
                etcd_packer::unpack_bool(*result, "canceled", response.canceled);

                rapidjson::Document::MemberIterator events = result->FindMember("events");
                if (result->MemberEnd() != events) {
                    rapidjson::Document::Array all_events = events->value.GetArray();
                    for (rapidjson::Document::Array::ValueIterator iter = all_events.Begin(); iter != all_events.End(); ++iter) {
                        response.events.push_back(event_t());
                        event_t &evt = response.events.back();

                        rapidjson::Document::MemberIterator type = iter->FindMember("type");
                        if (type == iter->MemberEnd()) {
                            evt.evt_type = etcd_watch_event::PUT; // etcd可能不会下发默认值
                        } else {
                            if (type->value.IsString()) {
                                if (0 == UTIL_STRFUNC_STRCASE_CMP("DELETE", type->value.GetString())) {
                                    evt.evt_type = etcd_watch_event::DELETE;
                                } else {
                                    evt.evt_type = etcd_watch_event::PUT;
                                }
                            } else if (type->value.IsNumber()) {
                                uint64_t type_int = 0;
                                etcd_packer::unpack_int(*iter, "type", type_int);
                                if (0 == type_int) {
                                    evt.evt_type = etcd_watch_event::PUT;
                                } else {
                                    evt.evt_type = etcd_watch_event::DELETE;
                                }
                            } else {
                                WLOGERROR("Etcd watcher %p got unknown event type. msg: %s", self, value_json.c_str());
                            }
                        }


                        rapidjson::Document::MemberIterator kv = iter->FindMember("kv");
                        if (kv != iter->MemberEnd()) {
                            etcd_packer::unpack(evt.kv, kv->value);
                        }

                        rapidjson::Document::MemberIterator prev_kv = iter->FindMember("prev_kv");
                        if (prev_kv != iter->MemberEnd()) {
                            etcd_packer::unpack(evt.prev_kv, prev_kv->value);
                        }
                    }
                }

                if (util::log::log_wrapper::check(WDTLOGGETCAT(util::log::log_wrapper::categorize_t::DEFAULT), util::log::log_wrapper::level_t::LOG_LW_DEBUG)) {
                    WLOGDEBUG("Etcd watcher %p got response: watch_id: %lld, compact_revision: %lld, created: %s, canceled: %s", self,
                              static_cast<long long>(response.watch_id), static_cast<long long>(response.compact_revision), response.created ? "Yes" : "No",
                              response.canceled ? "Yes" : "No");
                    for (size_t i = 0; i < response.events.size(); ++i) {
                        etcd_key_value *kv = &response.events[i].kv;
                        const char *name;
                        if (etcd_watch_event::PUT == response.events[i].evt_type) {
                            name = "PUT";
                        } else {
                            name = "DELETE";
                        }
                        WLOGDEBUG("    Evt => type: %s, key: %s, value: %s", name, kv->key.c_str(), kv->value.c_str());
                    }
                }

                // trigger event
                if (self->evt_handle_) {
                    self->evt_handle_(header, response);
                }
            }

            return 0;
        }
    } // namespace component
} // namespace atframe
