#include <assert.h>

#include <common/string_oprs.h>

#include <log/log_wrapper.h>

#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

#include "etcd_keepalive.h"
#include "etcd_watcher.h"

#include "etcd_cluster.h"

namespace atframe {
    namespace component {
    /**
     * @note APIs just like this
     * @see https://coreos.com/etcd/docs/latest/dev-guide/api_reference_v3.html
     * @see https://coreos.com/etcd/docs/latest/dev-guide/apispec/swagger/rpc.swagger.json
     * @note KeyValue: { "key": "KEY", "create_revision": "number", "mod_revision": "number", "version": "number", "value": "", "lease": "number" }
     *   Get data => curl http://localhost:2379/v3alpha/range -X POST -d '{"key": "KEY", "range_end": ""}'
     *       # Response {"kvs": [{...}], "more": "bool", "count": "COUNT"}
     *   Set data => curl http://localhost:2379/v3alpha/kv/put -X POST -d '{"key": "KEY", "value": "", "lease": "number", "prev_kv": "bool"}'
     *   Renew data => curl http://localhost:2379/v3alpha/kv/put -X POST -d '{"key": "KEY", "value": "", "prev_kv": "bool", "ignore_lease": true}'
     *       # Response {"header":{...}, "prev_kv": {...}}
     *   Delete data => curl http://localhost:2379/v3alpha/kv/deleterange -X POST -d '{"key": "KEY", "range_end": "", "prev_kv": "bool"}'
     *       # Response {"header":{...}, "deleted": "number", "prev_kvs": [{...}]}
     *
     *   Watch => curl http://localhost:2379/v3alpha/watch -XPOST -d '{"create_request":  {"key": "WATCH KEY", "range_end": "", "prev_kv": true} }'
     *       # Response {"header":{...},"watch_id":"ID","created":"bool", "canceled": "bool", "compact_revision": "REVISION", "events": [{"type":
     *                  "PUT=0|DELETE=1", "kv": {...}, prev_kv": {...}"}]}
     *
     *   Allocate Lease => curl http://localhost:2379/v3alpha/lease/grant -XPOST -d '{"TTL": 5, "ID": 0}'
     *       # Response {"header":{...},"ID":"ID","TTL":"5"}
     *   Keepalive Lease => curl http://localhost:2379/v3alpha/lease/keepalive -XPOST -d '{"ID": 0}'
     *       # Response {"header":{...},"ID":"ID","TTL":"5"}
     *   Revoke Lease => curl http://localhost:2379/v3alpha/kv/lease/revoke -XPOST -d '{"ID": 0}'
     *       # Response {"header":{...}}
     *
     *   List members => curl http://localhost:2379/v3alpha/cluster/member/list
     *       # Response {"header":{...},"members":[{"ID":"ID","name":"NAME","peerURLs":["peer url"],"clientURLs":["client url"]}]}
     */

#define ETCD_API_V3_MEMBER_LIST "/v3alpha/cluster/member/list"

#define ETCD_API_V3_KV_GET "/v3alpha/kv/range"
#define ETCD_API_V3_KV_SET "/v3alpha/kv/put"
#define ETCD_API_V3_KV_DELETE "/v3alpha/kv/deleterange"

#define ETCD_API_V3_WATCH "/v3alpha/watch"

#define ETCD_API_V3_LEASE_GRANT "/v3alpha/lease/grant"
#define ETCD_API_V3_LEASE_KEEPALIVE "/v3alpha/lease/keepalive"
#define ETCD_API_V3_LEASE_REVOKE "/v3alpha/kv/lease/revoke"

        namespace details {
            const std::string &get_default_user_agent() {
                static std::string ret;
                if (!ret.empty()) {
                    return ret;
                }

                char buffer[256] = {0};
                const char *prefix = "Mozilla/5.0";
                const char *suffix = "Atframework Etcdcli/1.0";
#if defined(_WIN32) || defined(__WIN32__)
#if (defined(__MINGW32__) && __MINGW32__)
                const char *sys_env = "Win32; MinGW32";
#elif (defined(__MINGW64__) || __MINGW64__)
                const char *sys_env = "Win64; x64; MinGW64";
#elif defined(__CYGWIN__) || defined(__MSYS__)
#if defined(_WIN64) || defined(__amd64) || defined(__x86_64)
                const char *sys_env = "Win64; x64; POSIX";
#else
                const char *sys_env = "Win32; POSIX";
#endif
#elif defined(_WIN64) || defined(__amd64) || defined(__x86_64)
                const char *sys_env = "Win64; x64";
#else
                const char *sys_env = "Win32";
#endif
#elif defined(__linux__) || defined(__linux)
                const char *sys_env = "Linux";
#elif defined(__APPLE__)
                const char *sys_env = "Darwin";
#elif defined(__DragonFly__) || defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || defined(__OpenBSD__) || defined(__NetBSD__)
                const char *sys_env = "BSD";
#elif defined(__unix__) || defined(__unix)
                const char *sys_env = "Unix";
#else
                const char *sys_env = "Unknown";
#endif

                UTIL_STRFUNC_SNPRINTF(buffer, sizeof(buffer) - 1, "%s (%s) %s", prefix, sys_env, suffix);
                ret = &buffer[0];

                return ret;
            }
        } // namespace details

        etcd_cluster::etcd_cluster() : flags_(0) {
            conf_.http_cmd_timeout = std::chrono::seconds(10);
            conf_.etcd_members_next_update_time = std::chrono::system_clock::from_time_t(0);
            conf_.etcd_members_update_interval = std::chrono::minutes(5);

            conf_.lease = 0;
            conf_.keepalive_next_update_time = std::chrono::system_clock::from_time_t(0);
            conf_.keepalive_timeout = std::chrono::seconds(16);
            conf_.keepalive_interval = std::chrono::seconds(5);
        }

        etcd_cluster::~etcd_cluster() { reset(); }

        void etcd_cluster::init(const util::network::http_request::curl_m_bind_ptr_t &curl_mgr) {
            curl_multi_ = curl_mgr;
            random_generator_.init_seed(static_cast<util::random::mt19937::result_type>(util::time::time_utility::get_now()));

            set_flag(flag_t::CLOSING, false);
        }

        util::network::http_request::ptr_t etcd_cluster::close(bool wait) {
            set_flag(flag_t::CLOSING, true);

            if (rpc_keepalive_) {
                rpc_keepalive_->set_on_complete(NULL);
                rpc_keepalive_->stop();
                rpc_keepalive_.reset();
            }

            if (rpc_update_members_) {
                rpc_update_members_->set_on_complete(NULL);
                rpc_update_members_->stop();
                rpc_update_members_.reset();
            }

            for (size_t i = 0; i < keepalive_actors_.size(); ++i) {
                if (keepalive_actors_[i]) {
                    keepalive_actors_[i]->close();
                }
            }
            keepalive_actors_.clear();

            for (size_t i = 0; i < watcher_actors_.size(); ++i) {
                if (watcher_actors_[i]) {
                    watcher_actors_[i]->close();
                }
            }
            watcher_actors_.clear();

            util::network::http_request::ptr_t ret;
            if (curl_multi_) {
                if (0 != conf_.lease) {
                    ret = create_request_lease_revoke();

                    // wait to delete content
                    if (ret) {
                        ret->start(util::network::http_request::method_t::EN_MT_POST, wait);
                    }

                    conf_.lease = 0;
                }
            }

            if (ret && false == ret->is_running()) {
                ret.reset();
            }

            return ret;
        }

        void etcd_cluster::reset() {
            close(true);

            curl_multi_.reset();
            flags_ = 0;

            conf_.http_cmd_timeout = std::chrono::seconds(10);

            conf_.path_node.clear();
            conf_.etcd_members_next_update_time = std::chrono::system_clock::from_time_t(0);
            conf_.etcd_members_update_interval = std::chrono::minutes(5);

            conf_.lease = 0;
            conf_.keepalive_next_update_time = std::chrono::system_clock::from_time_t(0);
            conf_.keepalive_timeout = std::chrono::seconds(16);
            conf_.keepalive_interval = std::chrono::seconds(5);
        }

        int etcd_cluster::tick() {
            int ret = 0;

            if (!curl_multi_) {
                return ret;
            }

            if (check_flag(flag_t::CLOSING)) {
                return 0;
            }

            // update members
            if (util::time::time_utility::now() > conf_.etcd_members_next_update_time) {
                ret += create_request_member_update() ? 1 : 0;
            }

            // empty other actions will be delayed
            if (conf_.path_node.empty()) {
                return ret;
            }

            // keepalive lease
            if (check_flag(flag_t::ENABLE_LEASE)) {
                if (0 == get_lease()) {
                    ret += create_request_lease_grant() ? 1 : 0;
                } else if (util::time::time_utility::now() > conf_.keepalive_next_update_time) {
                    ret += create_request_lease_keepalive() ? 1 : 0;
                }
            }

            // reactive watcher
            for (size_t i = 0; i < watcher_actors_.size(); ++i) {
                if (watcher_actors_[i]) {
                    watcher_actors_[i]->active();
                }
            }

            return ret;
        }

        void etcd_cluster::set_flag(flag_t::type f, bool v) {
            assert(0 == (f & (f - 1)));
            if (v == check_flag(f)) {
                return;
            }

            if (v) {
                flags_ |= f;
            } else {
                flags_ &= ~f;
            }

            switch (f) {
            case flag_t::ENABLE_LEASE: {
                if (v) {
                    create_request_lease_grant();
                } else if (rpc_keepalive_) {
                    rpc_keepalive_->set_on_complete(NULL);
                    rpc_keepalive_->stop();
                    rpc_keepalive_.reset();
                }
                break;
            }
            default: { break; }
            }
        }

        time_t etcd_cluster::get_http_timeout_ms() const {
            time_t ret = static_cast<time_t>(std::chrono::duration_cast<std::chrono::milliseconds>(get_conf_http_timeout()).count());
            if (ret <= 0) {
                ret = 30000; // 30s
            }

            return ret;
        }

        bool etcd_cluster::add_keepalive(const std::shared_ptr<etcd_keepalive> &keepalive) {
            if (!keepalive) {
                return false;
            }

            if (check_flag(flag_t::CLOSING)) {
                return false;
            }

            if (keepalive_actors_.end() != std::find(keepalive_actors_.begin(), keepalive_actors_.end(), keepalive)) {
                return false;
            }

            if (this != &keepalive->get_owner()) {
                return false;
            }

            set_flag(flag_t::ENABLE_LEASE, true);
            keepalive_actors_.push_back(keepalive);
            return true;
        }

        bool etcd_cluster::add_retry_keepalive(const std::shared_ptr<etcd_keepalive> &keepalive) {
            if (!keepalive) {
                return false;
            }

            if (check_flag(flag_t::CLOSING)) {
                return false;
            }

            if (keepalive_retry_actors_.end() != std::find(keepalive_retry_actors_.begin(), keepalive_retry_actors_.end(), keepalive)) {
                return false;
            }

            if (this != &keepalive->get_owner()) {
                return false;
            }

            set_flag(flag_t::ENABLE_LEASE, true);
            keepalive_retry_actors_.push_back(keepalive);
            return true;
        }

        bool etcd_cluster::add_watcher(const std::shared_ptr<etcd_watcher> &watcher) {
            if (!watcher) {
                return false;
            }

            if (check_flag(flag_t::CLOSING)) {
                return false;
            }

            if (watcher_actors_.end() != std::find(watcher_actors_.begin(), watcher_actors_.end(), watcher)) {
                return false;
            }

            if (this != &watcher->get_owner()) {
                return false;
            }

            watcher_actors_.push_back(watcher);
            return true;
        }

        void etcd_cluster::set_lease(int64_t v, bool force_active_keepalives) {
            int64_t old_v = get_lease();
            conf_.lease = v;

            if (old_v == v && false == force_active_keepalives) {
                // 仅重试失败项目
                for (size_t i = 0; i < keepalive_retry_actors_.size(); ++i) {
                    if (keepalive_retry_actors_[i]) {
                        keepalive_retry_actors_[i]->active();
                    }
                }

                keepalive_retry_actors_.clear();
                return;
            }

            if (0 == old_v && 0 != v) {
                // all keepalive object start a set request
                for (size_t i = 0; i < keepalive_actors_.size(); ++i) {
                    if (keepalive_actors_[i]) {
                        keepalive_actors_[i]->active();
                    }
                }
            } else if (0 != old_v && 0 != v) {
                // all keepalive object start a update request
                for (size_t i = 0; i < keepalive_actors_.size(); ++i) {
                    if (keepalive_actors_[i]) {
                        keepalive_actors_[i]->active();
                    }
                }
            }

            keepalive_retry_actors_.clear();
        }

        bool etcd_cluster::create_request_member_update() {
            if (!curl_multi_) {
                return false;
            }

            if (check_flag(flag_t::CLOSING)) {
                return false;
            }

            if (rpc_update_members_) {
                return false;
            }

            if (conf_.conf_hosts.empty() && conf_.hosts.empty()) {
                return false;
            }

            if (std::chrono::system_clock::duration::zero() >= conf_.etcd_members_update_interval) {
                conf_.etcd_members_next_update_time = util::time::time_utility::now() + std::chrono::seconds(1);
            } else {
                conf_.etcd_members_next_update_time = util::time::time_utility::now() + conf_.etcd_members_update_interval;
            }

            std::string *selected_host;
            if (!conf_.hosts.empty()) {
                selected_host = &conf_.hosts[random_generator_.random_between<size_t>(0, conf_.hosts.size())];
            } else {
                selected_host = &conf_.conf_hosts[random_generator_.random_between<size_t>(0, conf_.conf_hosts.size())];
            }

            std::stringstream ss;
            ss << (*selected_host) << ETCD_API_V3_MEMBER_LIST;
            util::network::http_request::ptr_t req = util::network::http_request::create(curl_multi_.get(), ss.str());

            if (req) {
                rapidjson::Document doc;
                doc.SetObject();

                setup_http_request(req, doc, get_http_timeout_ms());
                req->set_priv_data(this);
                req->set_on_complete(libcurl_callback_on_member_update);

                int res = req->start(util::network::http_request::method_t::EN_MT_POST, false);
                if (res != 0) {
                    req->set_on_complete(NULL);
                    WLOGERROR("Etcd start keepalive lease %lld request to %s failed, res: %d", static_cast<long long>(get_lease()), req->get_url().c_str(),
                              res);
                    return false;
                }

                WLOGDEBUG("Etcd start keepalive lease %lld request to %s", static_cast<long long>(get_lease()), req->get_url().c_str());
                rpc_update_members_ = req;
            }

            return true;
        }

        int etcd_cluster::libcurl_callback_on_member_update(util::network::http_request &req) {
            etcd_cluster *self = reinterpret_cast<etcd_cluster *>(req.get_priv_data());
            if (NULL == self) {
                WLOGERROR("Etcd member list shouldn't has request without private data");
                return 0;
            }

            self->rpc_update_members_.reset();

            // 服务器错误则忽略
            if (0 != req.get_error_code() ||
                util::network::http_request::status_code_t::EN_ECG_SUCCESS != util::network::http_request::get_status_code_group(req.get_response_code())) {

                // only network error will trigger a etcd member update
                if (0 != req.get_error_code()) {
                    self->create_request_member_update();
                }
                WLOGERROR("Etcd member list failed, error code: %d, http code: %d\n%s", req.get_error_code(), req.get_response_code(), req.get_error_msg());

                // 出错则从host里移除无效数据
                for (size_t i = 0; i < self->conf_.hosts.size(); ++i) {
                    if (0 == UTIL_STRFUNC_STRNCASE_CMP(self->conf_.hosts[i].c_str(), req.get_url().c_str(), self->conf_.hosts[i].size())) {
                        if (i != self->conf_.hosts.size() - 1) {
                            self->conf_.hosts[self->conf_.hosts.size() - 1].swap(self->conf_.hosts[i]);
                        }

                        self->conf_.hosts.pop_back();
                        break;
                    }
                }
                return 0;
            }

            std::string http_content;
            req.get_response_stream().str().swap(http_content);
            WLOGDEBUG("Etcd cluster got http response: %s", http_content.c_str());

            do {
                // unpack
                rapidjson::Document doc;
                doc.Parse(http_content.c_str());

                // ignore empty data
                if (false == doc.IsObject()) {
                    break;
                }

                rapidjson::Value root = doc.GetObject();
                rapidjson::Document::MemberIterator members = root.FindMember("members");
                if (root.MemberEnd() == members) {
                    WLOGERROR("Etcd members not found");
                    return 0;
                }

                self->conf_.hosts.clear();
                bool need_select_node = true;
                rapidjson::Document::Array all_members = members->value.GetArray();
                for (rapidjson::Document::Array::ValueIterator iter = all_members.Begin(); iter != all_members.End(); ++iter) {
                    rapidjson::Document::MemberIterator client_urls = iter->FindMember("clientURLs");
                    if (client_urls == iter->MemberEnd()) {
                        continue;
                    }

                    rapidjson::Document::Array all_client_urls = client_urls->value.GetArray();
                    for (rapidjson::Document::Array::ValueIterator cli_url_iter = all_client_urls.Begin(); cli_url_iter != all_client_urls.End();
                         ++cli_url_iter) {
                        if (cli_url_iter->GetStringLength() > 0) {
                            self->conf_.hosts.push_back(cli_url_iter->GetString());

                            if (self->conf_.path_node == self->conf_.hosts.back()) {
                                need_select_node = false;
                            }
                        }
                    }
                }

                if (!self->conf_.hosts.empty() && need_select_node) {
                    self->conf_.path_node = self->conf_.hosts[self->random_generator_.random_between<size_t>(0, self->conf_.hosts.size())];
                }

                // 触发一次tick
                self->tick();
            } while (false);

            return 0;
        }

        bool etcd_cluster::create_request_lease_grant() {
            if (!curl_multi_ || conf_.path_node.empty()) {
                return false;
            }

            if (check_flag(flag_t::CLOSING) || !check_flag(flag_t::ENABLE_LEASE)) {
                return false;
            }

            if (rpc_keepalive_) {
                return false;
            }

            if (std::chrono::system_clock::duration::zero() >= conf_.keepalive_interval) {
                conf_.keepalive_next_update_time = util::time::time_utility::now() + std::chrono::seconds(1);
            } else {
                conf_.keepalive_next_update_time = util::time::time_utility::now() + conf_.keepalive_interval;
            }

            std::stringstream ss;
            ss << conf_.path_node << ETCD_API_V3_LEASE_GRANT;
            util::network::http_request::ptr_t req = util::network::http_request::create(curl_multi_.get(), ss.str());

            if (req) {
                rapidjson::Document doc;
                doc.SetObject();
                doc.AddMember("ID", get_lease(), doc.GetAllocator());
                doc.AddMember("TTL", std::chrono::duration_cast<std::chrono::seconds>(conf_.keepalive_timeout).count(), doc.GetAllocator());

                setup_http_request(req, doc, get_http_timeout_ms());
                req->set_priv_data(this);
                req->set_on_complete(libcurl_callback_on_lease_keepalive);

                int res = req->start(util::network::http_request::method_t::EN_MT_POST, false);
                if (res != 0) {
                    req->set_on_complete(NULL);
                    WLOGERROR("Etcd start keepalive lease %lld request to %s failed, res: %d", static_cast<long long>(get_lease()), req->get_url().c_str(),
                              res);
                    return false;
                }

                WLOGDEBUG("Etcd start keepalive lease %lld request to %s", static_cast<long long>(get_lease()), req->get_url().c_str());
                rpc_keepalive_ = req;
            }

            return true;
        }

        bool etcd_cluster::create_request_lease_keepalive() {
            if (!curl_multi_ || 0 == get_lease() || conf_.path_node.empty()) {
                return false;
            }

            if (check_flag(flag_t::CLOSING) || !check_flag(flag_t::ENABLE_LEASE)) {
                return false;
            }

            if (rpc_keepalive_) {
                return false;
            }

            if (util::time::time_utility::now() <= conf_.keepalive_next_update_time) {
                return false;
            }

            if (std::chrono::system_clock::duration::zero() >= conf_.keepalive_interval) {
                conf_.keepalive_next_update_time = util::time::time_utility::now() + std::chrono::seconds(1);
            } else {
                conf_.keepalive_next_update_time = util::time::time_utility::now() + conf_.keepalive_interval;
            }

            std::stringstream ss;
            ss << conf_.path_node << ETCD_API_V3_LEASE_KEEPALIVE;
            util::network::http_request::ptr_t req = util::network::http_request::create(curl_multi_.get(), ss.str());

            if (req) {
                rapidjson::Document doc;
                doc.SetObject();
                doc.AddMember("ID", get_lease(), doc.GetAllocator());

                setup_http_request(req, doc, get_http_timeout_ms());
                req->set_priv_data(this);
                req->set_on_complete(libcurl_callback_on_lease_keepalive);

                int res = req->start(util::network::http_request::method_t::EN_MT_POST, false);
                if (res != 0) {
                    req->set_on_complete(NULL);
                    WLOGERROR("Etcd start keepalive lease %lld request to %s failed, res: %d", static_cast<long long>(get_lease()), req->get_url().c_str(),
                              res);
                    return false;
                }

                WLOGDEBUG("Etcd start keepalive lease %lld request to %s", static_cast<long long>(get_lease()), req->get_url().c_str());
                rpc_keepalive_ = req;
            }

            return true;
        }

        int etcd_cluster::libcurl_callback_on_lease_keepalive(util::network::http_request &req) {
            etcd_cluster *self = reinterpret_cast<etcd_cluster *>(req.get_priv_data());
            if (NULL == self) {
                WLOGERROR("Etcd lease keepalive shouldn't has request without private data");
                return 0;
            }

            self->rpc_keepalive_.reset();

            // 服务器错误则忽略
            if (0 != req.get_error_code() ||
                util::network::http_request::status_code_t::EN_ECG_SUCCESS != util::network::http_request::get_status_code_group(req.get_response_code())) {

                // only network error will trigger a etcd member update
                if (0 != req.get_error_code()) {
                    self->create_request_member_update();
                }
                WLOGERROR("Etcd lease keepalive failed, error code: %d, http code: %d\n%s", req.get_error_code(), req.get_response_code(), req.get_error_msg());
                return 0;
            }

            std::string http_content;
            req.get_response_stream().str().swap(http_content);
            WLOGDEBUG("Etcd cluster got http response: %s", http_content.c_str());

            do {
                // 如果lease不存在（没有TTL）则启动创建流程
                rapidjson::Document doc;
                doc.Parse(http_content.c_str());

                // 忽略空数据
                if (false == doc.IsObject()) {
                    break;
                }

                bool is_grant = false;
                rapidjson::Value root = doc.GetObject();
                rapidjson::Value::MemberIterator result = root.FindMember("result");
                if (result == root.MemberEnd()) {
                    is_grant = true;
                } else {
                    root = result->value;
                }

                if (false == root.IsObject()) {
                    WLOGERROR("Etcd lease grant failed, root is not object.(%s)", http_content.c_str());
                    return 0;
                }

                if (root.MemberEnd() == root.FindMember("TTL")) {
                    if (is_grant) {
                        WLOGERROR("Etcd lease grant failed");
                    } else {
                        WLOGERROR("Etcd lease keepalive failed because not found, try to grant one");
                        self->create_request_lease_grant();
                    }
                    return 0;
                }

                // 更新lease
                int64_t new_lease = 0;
                etcd_packer::unpack_int(root, "ID", new_lease);

                if (0 == new_lease) {
                    WLOGERROR("Etcd cluster got a error http response for grant or keepalive lease: %s", http_content.c_str());
                    break;
                }

                if (is_grant) {
                    WLOGDEBUG("Etcd lease %lld granted", static_cast<long long>(new_lease));
                    // TODO force reset all lease in case of resume data
                } else {
                    WLOGDEBUG("Etcd lease %lld keepalive successed", static_cast<long long>(new_lease));
                }

                self->set_lease(new_lease, is_grant);
            } while (false);

            return 0;
        }

        util::network::http_request::ptr_t etcd_cluster::create_request_lease_revoke() {
            if (!curl_multi_ || 0 == get_lease() || conf_.path_node.empty()) {
                return util::network::http_request::ptr_t();
            }

            std::stringstream ss;
            ss << conf_.path_node << ETCD_API_V3_LEASE_REVOKE;
            util::network::http_request::ptr_t ret = util::network::http_request::create(curl_multi_.get(), ss.str());

            if (ret) {
                rapidjson::Document doc;
                doc.SetObject();
                doc.AddMember("ID", get_lease(), doc.GetAllocator());

                setup_http_request(ret, doc, get_http_timeout_ms());
            }

            return ret;
        }

        util::network::http_request::ptr_t etcd_cluster::create_request_kv_get(const std::string &key, const std::string &range_end, int64_t limit,
                                                                               int64_t revision) {
            if (!curl_multi_ || conf_.path_node.empty() || check_flag(flag_t::CLOSING)) {
                return util::network::http_request::ptr_t();
            }

            std::stringstream ss;
            ss << conf_.path_node << ETCD_API_V3_KV_GET;
            util::network::http_request::ptr_t ret = util::network::http_request::create(curl_multi_.get(), ss.str());

            if (ret) {
                rapidjson::Document doc;
                rapidjson::Value &root = doc.SetObject();

                etcd_packer::pack_key_range(root, key, range_end, doc);
                doc.AddMember("limit", limit, doc.GetAllocator());
                doc.AddMember("revision", revision, doc.GetAllocator());

                setup_http_request(ret, doc, get_http_timeout_ms());
            }

            return ret;
        }

        util::network::http_request::ptr_t etcd_cluster::create_request_kv_set(const std::string &key, const std::string &value, bool assign_lease,
                                                                               bool prev_kv, bool ignore_value, bool ignore_lease) {
            if (!curl_multi_ || conf_.path_node.empty() || check_flag(flag_t::CLOSING)) {
                return util::network::http_request::ptr_t();
            }

            if (assign_lease && 0 == get_lease()) {
                return util::network::http_request::ptr_t();
            }

            std::stringstream ss;
            ss << conf_.path_node << ETCD_API_V3_KV_SET;
            util::network::http_request::ptr_t ret = util::network::http_request::create(curl_multi_.get(), ss.str());

            if (ret) {
                rapidjson::Document doc;
                rapidjson::Value &root = doc.SetObject();

                etcd_packer::pack_base64(root, "key", key, doc);
                etcd_packer::pack_base64(root, "value", value, doc);
                if (assign_lease) {
                    doc.AddMember("lease", get_lease(), doc.GetAllocator());
                }

                doc.AddMember("prev_kv", prev_kv, doc.GetAllocator());
                doc.AddMember("ignore_value", ignore_value, doc.GetAllocator());
                doc.AddMember("ignore_lease", ignore_lease, doc.GetAllocator());

                setup_http_request(ret, doc, get_http_timeout_ms());
            }

            return ret;
        }

        util::network::http_request::ptr_t etcd_cluster::create_request_kv_del(const std::string &key, const std::string &range_end, bool prev_kv) {
            if (!curl_multi_ || conf_.path_node.empty() || check_flag(flag_t::CLOSING)) {
                return util::network::http_request::ptr_t();
            }

            std::stringstream ss;
            ss << conf_.path_node << ETCD_API_V3_KV_DELETE;
            util::network::http_request::ptr_t ret = util::network::http_request::create(curl_multi_.get(), ss.str());

            if (ret) {
                rapidjson::Document doc;
                rapidjson::Value &root = doc.SetObject();

                etcd_packer::pack_key_range(root, key, range_end, doc);
                doc.AddMember("prev_kv", prev_kv, doc.GetAllocator());

                setup_http_request(ret, doc, get_http_timeout_ms());
            }

            return ret;
        }

        util::network::http_request::ptr_t etcd_cluster::create_request_watch(const std::string &key, const std::string &range_end, int64_t start_revision,
                                                                              bool prev_kv, bool progress_notify) {
            if (!curl_multi_ || conf_.path_node.empty() || check_flag(flag_t::CLOSING)) {
                return util::network::http_request::ptr_t();
            }

            std::stringstream ss;
            ss << conf_.path_node << ETCD_API_V3_WATCH;
            util::network::http_request::ptr_t ret = util::network::http_request::create(curl_multi_.get(), ss.str());

            if (ret) {
                rapidjson::Document doc;
                rapidjson::Value &root = doc.SetObject();

                rapidjson::Value create_request(rapidjson::kObjectType);


                etcd_packer::pack_key_range(create_request, key, range_end, doc);
                if (prev_kv) {
                    create_request.AddMember("prev_kv", prev_kv, doc.GetAllocator());
                }

                if (progress_notify) {
                    create_request.AddMember("progress_notify", progress_notify, doc.GetAllocator());
                }

                if (0 != start_revision) {
                    create_request.AddMember("start_revision", start_revision, doc.GetAllocator());
                }

                root.AddMember("create_request", create_request, doc.GetAllocator());

                setup_http_request(ret, doc, get_http_timeout_ms());
                ret->set_opt_keepalive(75, 150);
                // 不能共享socket
                ret->set_opt_reuse_connection(false);
            }

            return ret;
        }

        void etcd_cluster::setup_http_request(util::network::http_request::ptr_t &req, rapidjson::Document &doc, time_t timeout) {
            if (!req) {
                return;
            }

            req->set_opt_follow_location(true);
            req->set_opt_ssl_verify_peer(false);
            req->set_opt_http_content_decoding(true);
            req->set_opt_timeout(timeout);
            req->set_user_agent(details::get_default_user_agent());

            // Stringify the DOM
            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            doc.Accept(writer);

            req->post_data().assign(buffer.GetString(), buffer.GetSize());
            // WLOGDEBUG("=====setup request to %s, post data: %s", req->get_url().c_str(), req->post_data().c_str());
        }

    } // namespace component
} // namespace atframe