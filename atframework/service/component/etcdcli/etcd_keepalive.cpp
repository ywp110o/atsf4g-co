#include <algorithm/base64.h>

#include <log/log_wrapper.h>

#include "etcd_cluster.h"
#include "etcd_keepalive.h"


namespace atframe {
    namespace component {
        etcd_keepalive::default_checker_t::default_checker_t(const std::string &checked) : data(checked) {}

        bool etcd_keepalive::default_checker_t::operator()(const std::string &checked) const { return checked.empty() || data == checked; }

        etcd_keepalive::etcd_keepalive(etcd_cluster &owner, const std::string &path, constrict_helper_t &) : owner_(&owner) {
            checker_.is_check_run = false;
            checker_.is_check_passed = false;
            rpc_.is_actived = false;

            util::base64_encode(path_, path);
        }

        etcd_keepalive::ptr_t etcd_keepalive::create(etcd_cluster &owner, const std::string &path) {
            constrict_helper_t h;
            return std::make_shared<etcd_keepalive>(owner, path, h);
        }

        void etcd_keepalive::close() {
            if (rpc_.rpc_opr_) {
                rpc_.rpc_opr_->set_on_complete(NULL);
                rpc_.rpc_opr_->stop();
                rpc_.rpc_opr_.reset();
            }
            rpc_.is_actived = false;

            checker_.is_check_run = false;
            checker_.is_check_passed = false;
            checker_.fn = NULL;
        }

        void etcd_keepalive::set_checker(const std::string &checked_str) { checker_.fn = default_checker_t(checked_str); }

        void etcd_keepalive::set_checker(checker_fn_t fn) { checker_.fn = fn; }

        std::string etcd_keepalive::get_path() const {
            std::string ret;
            util::base64_decode(ret, path_);

            // these should be optimization by NRVO
            return ret;
        }

        void etcd_keepalive::active() {
            rpc_.is_actived = true;
            process();
        }

        void etcd_keepalive::process() {
            if (rpc_.rpc_opr_) {
                return;
            }

            rpc_.is_actived = false;

            // if has checker and has not check date yet, send a check request
            if (!checker_.fn) {
                checker_.is_check_run = true;
                checker_.is_check_passed = true;
            }

            if (false == checker_.is_check_run) {
                // create a check rpc
                rpc_.rpc_opr_ = owner_->create_request_kv_get(path_);
                if (!rpc_.rpc_opr_) {
                    WLOGERROR("Etcd keepalive create get data request to %s failed", path_.c_str());
                    return;
                }

                rpc_.rpc_opr_->set_priv_data(this);
                rpc_.rpc_opr_->set_on_complete(libcurl_callback_on_get_data);
                return;
            }

            // if check passed, set data
            if (checker_.is_check_run && checker_.is_check_passed) {
                // create set data rpc
                rpc_.rpc_opr_ = owner_->create_request_kv_set(path_, value_);
                if (!rpc_.rpc_opr_) {
                    WLOGERROR("Etcd keepalive create get data request to %s failed", path_.c_str());
                    return;
                }

                rpc_.rpc_opr_->set_priv_data(this);
                rpc_.rpc_opr_->set_on_complete(libcurl_callback_on_set_data);
            }
        }

        int etcd_keepalive::libcurl_callback_on_get_data(util::network::http_request &req) {
            etcd_keepalive *self = reinterpret_cast<etcd_keepalive *>(req.get_priv_data());
            if (NULL == self) {
                WLOGERROR("Etcd keepalive get request shouldn't has request without private data");
                return 0;
            }

            self->rpc_.rpc_opr_.reset();

            // 服务器错误则忽略
            if (0 != req.get_error_code() ||
                util::network::http_request::status_code_t::EN_ECG_SUCCESS != util::network::http_request::get_status_code_group(req.get_response_code())) {
                WLOGERROR("Etcd keepalive get request failed, error code: %d, http code: %d\n%s", req.get_error_code(), req.get_response_code(),
                          req.get_error_msg());
                return 0;
            }

            std::string http_content;
            req.get_response_stream().str().swap(http_content);
            WLOGDEBUG("Etcd keepalive %p got http response: %s", self, http_content.c_str());

            // 如果lease不存在（没有TTL）则启动创建流程
            rapidjson::Document doc;
            doc.Parse(http_content.c_str());

            rapidjson::Value root = doc.GetObject();

            self->checker_.is_check_run = true;
            // Run check function
            int64_t count = 0;
            std::string content;
            etcd_packer::unpack_int(root, "count", count);
            if (count > 0) {
                rapidjson::Document::MemberIterator kvs = root.FindMember("kvs");
                if (root.MemberEnd() == kvs) {
                    WLOGERROR("Etcd keepalive get data count=%lld, but kvs not found", static_cast<long long>(count));
                    return 0;
                }

                rapidjson::Document::Array all_kvs = kvs->value.GetArray();
                for (rapidjson::Document::Array::ValueIterator iter = all_kvs.Begin(); iter != all_kvs.End(); ++iter) {
                    etcd_key_value kv;
                    etcd_packer::unpack(kv, *iter);
                    content.swap(kv.value);
                    break;
                }
            }

            if (!self->checker_.fn) {
                self->checker_.is_check_passed = true;
            } else {
                self->checker_.is_check_passed = self->checker_.fn(content);
            }
            WLOGDEBUG("Etcd keepalive %p check data %s", self, self->checker_.is_check_passed ? "passed" : "failed");

            self->active();
            return 0;
        }

        int etcd_keepalive::libcurl_callback_on_set_data(util::network::http_request &req) {
            etcd_keepalive *self = reinterpret_cast<etcd_keepalive *>(req.get_priv_data());
            if (NULL == self) {
                WLOGERROR("Etcd keepalive set request shouldn't has request without private data");
                return 0;
            }

            self->rpc_.rpc_opr_.reset();

            // 服务器错误则忽略
            if (0 != req.get_error_code() ||
                util::network::http_request::status_code_t::EN_ECG_SUCCESS != util::network::http_request::get_status_code_group(req.get_response_code())) {
                WLOGERROR("Etcd keepalive get request failed, error code: %d, http code: %d\n%s", req.get_error_code(), req.get_response_code(),
                          req.get_error_msg());
                return 0;
            }

            WLOGDEBUG("Etcd keepalive %p set data http response: %s", self, req.get_response_stream().str().c_str());
            return 0;
        }

    } // namespace component
} // namespace atframe
