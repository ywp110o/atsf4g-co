#include <algorithm/base64.h>

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
            rpc_.is_actived = false;
        }

        etcd_watcher::ptr_t etcd_watcher::create(etcd_cluster &owner, const std::string &path, const std::string &range_end) {
            constrict_helper_t h;
            return std::make_shared<etcd_watcher>(owner, path, range_end, h);
        }

        void etcd_watcher::close() {
            if (rpc_.rpc_opr_) {
                rpc_.rpc_opr_->set_on_complete(NULL);
                rpc_.rpc_opr_->stop();
                rpc_.rpc_opr_.reset();
            }
            rpc_.is_actived = false;
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

            // create watcher request
            rpc_.rpc_opr_ = owner_->create_request_watch(path_, range_end_, true, rpc_.enable_progress_notify);
            if (!rpc_.rpc_opr_) {
                WLOGERROR("Etcd keepalive create get data request to %s failed", path_.c_str());
                rpc_.watcher_next_request_time = util::time::time_utility::now() + rpc_.retry_interval;
                return;
            }

            rpc_.rpc_opr_->set_priv_data(this);
            rpc_.rpc_opr_->set_on_complete(libcurl_callback_on_changed);
            rpc_.rpc_opr_->set_opt_timeout(static_cast<time_t>(std::chrono::duration_cast<std::chrono::milliseconds>(rpc_.request_timeout).count()));
            return;
        }

        int etcd_watcher::libcurl_callback_on_changed(util::network::http_request &req) {
            etcd_watcher *self = reinterpret_cast<etcd_watcher *>(req.get_priv_data());
            if (NULL == self) {
                WLOGERROR("Etcd watcher get request shouldn't has request without private data");
                return 0;
            }

            self->rpc_.rpc_opr_.reset();

            // 服务器错误则过一段时间后重试
            if (0 != req.get_error_code() ||
                util::network::http_request::status_code_t::EN_ECG_SUCCESS != util::network::http_request::get_status_code_group(req.get_response_code())) {
                WLOGERROR("Etcd watcher get request failed, error code: %d, http code: %d\n%s", req.get_error_code(), req.get_response_code(),
                          req.get_error_msg());

                self->rpc_.watcher_next_request_time = util::time::time_utility::now() + self->rpc_.retry_interval;
                // 立刻开启下一次watch
                self->active();
                return 0;
            }

            std::string http_content;
            req.get_response_stream().str().swap(http_content);
            WLOGDEBUG("Etcd watcher %p got http response: %s", self, http_content.c_str());

            // 如果lease不存在（没有TTL）则启动创建流程
            rapidjson::Document doc;
            doc.Parse(http_content.c_str());

            rapidjson::Value root = doc.GetObject();

            response_t response;
            // TODO decode
            etcd_packer::unpack_int(root, "watch_id", response.watch_id);
            etcd_packer::unpack_int(root, "compact_revision", response.compact_revision);
            etcd_packer::unpack_bool(root, "created", response.created);
            etcd_packer::unpack_bool(root, "canceled", response.canceled);

            rapidjson::Document::MemberIterator events = root.FindMember("events");
            if (root.MemberEnd() != events) {
                rapidjson::Document::Array all_events = events->value.GetArray();
                for (rapidjson::Document::Array::ValueIterator iter = all_events.Begin(); iter != all_events.End(); ++iter) {
                    rapidjson::Document::MemberIterator type = iter->FindMember("type");
                    if (type == iter->MemberEnd()) {
                        continue;
                    }

                    response.events.push_back(event_t());
                    event_t &evt = response.events.back();

                    uint64_t type_int = 0;
                    etcd_packer::unpack_int(*iter, "type", type_int);
                    if (0 == type_int) {
                        evt.evt_type = etcd_watch_event::PUT;
                    } else {
                        evt.evt_type = etcd_watch_event::DELETE;
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
                    etcd_key_value *kv;
                    const char *name;
                    if (etcd_watch_event::PUT == response.events[i].evt_type) {
                        kv = &response.events[i].kv;
                        name = "PUT";
                    } else {
                        kv = &response.events[i].prev_kv;
                        name = "DELETE";
                    }
                    WLOGDEBUG("    Evt => type: %s, key: %s, value %s", name, kv->key.c_str(), kv->value.c_str());
                }
            }

            // TODO trigger event

            // 立刻开启下一次watch
            self->active();
            return 0;
        }
    } // namespace component
} // namespace atframe
