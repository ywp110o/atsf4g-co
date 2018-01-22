#include <sstream>

#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"


#include <common/string_oprs.h>
#include <random/random_generator.h>


#include <atframe/atapp.h>

#include <etcdcli/etcd_cluster.h>
#include <etcdcli/etcd_keepalive.h>
#include <etcdcli/etcd_watcher.h>

#include "etcd_module.h"

namespace atframe {
    namespace proxy {
        namespace detail {
            std::chrono::system_clock::duration convert(const util::config::duration_value &src) {
                std::chrono::system_clock::duration ret = std::chrono::seconds(src.sec);
                ret += std::chrono::nanoseconds(src.nsec);
                return ret;
            }
        } // namespace detail

        etcd_module::etcd_module() {
            conf_.path_prefix = "/";
            conf_.watcher_retry_interval = std::chrono::seconds(15); // 重试间隔15秒
            conf_.watcher_request_timeout = std::chrono::hours(1);   // 一小时超时时间，相当于每小时重新拉取数据
        }

        etcd_module::~etcd_module() { reset(); }

        void etcd_module::reset() {
            if (cleanup_request_) {
                cleanup_request_->set_priv_data(NULL);
                cleanup_request_->set_on_complete(NULL);
                cleanup_request_->stop();
                cleanup_request_.reset();
            }

            etcd_ctx_.reset();

            if (curl_multi_) {
                util::network::http_request::destroy_curl_multi(curl_multi_);
            }
        }

        int etcd_module::init() {
            // init curl
            int res = curl_global_init(CURL_GLOBAL_ALL);
            if (res) {
                WLOGERROR("init cURL failed, errcode: %d", res);
                return -1;
            }

            if (etcd_ctx_.get_conf_hosts().empty() || conf_.path_node.empty()) {
                WLOGINFO("etcd host not found, start singel mode");
                return 0;
            }

            util::network::http_request::create_curl_multi(get_app()->get_bus_node()->get_evloop(), curl_multi_);
            if (!curl_multi_) {
                WLOGERROR("create curl multi instance failed.");
                return -1;
            }

            etcd_ctx_.init(curl_multi_);

            // generate keepalive data
            atframe::component::etcd_keepalive::ptr_t keepalive_actor;
            {
                std::string val;
                node_info_t ni;
                ni.id = get_app()->get_id();
                ni.listens = get_app()->get_bus_node()->get_listen_list();
                pack(ni, val);

                keepalive_actor = atframe::component::etcd_keepalive::create(etcd_ctx_, conf_.path_node);
                if (!keepalive_actor) {
                    WLOGERROR("create etcd_keepalive failed.");
                    return -1;
                }

                keepalive_actor->set_checker(val);
#if defined(UTIL_CONFIG_COMPILER_CXX_RVALUE_REFERENCES) && UTIL_CONFIG_COMPILER_CXX_RVALUE_REFERENCES
                keepalive_actor->set_value(std::move(val));
#else
                keepalive_actor->set_value(val);
#endif
                etcd_ctx_.add_keepalive(keepalive_actor);
            }

            // generate watch data
            {
                atframe::component::etcd_watcher::ptr_t p = atframe::component::etcd_watcher::create(etcd_ctx_, conf_.path_prefix, "+1");
                if (!p) {
                    WLOGERROR("create etcd_watcher failed.");
                    return -1;
                }

                etcd_ctx_.add_watcher(p);

                p->set_evt_handle(watcher_callback_t(*this));
            }

            // 执行到首次检测结束
            bool is_failed = false;
            while (true) {
                util::time::time_utility::update();
                etcd_ctx_.tick();

                if (keepalive_actor->is_check_run()) {
                    if (!keepalive_actor->is_check_passed()) {
                        WLOGERROR("etcd_keepalive lock %s failed.", conf_.path_node.c_str());
                        is_failed = true;
                    }

                    break;
                }

                uv_run(get_app()->get_bus_node()->get_evloop(), UV_RUN_ONCE);

                // 重试次数过多则失败退出
                if (keepalive_actor->get_check_times() >= 3 || etcd_ctx_.get_stats().continue_error_requests > 3) {
                    WLOGERROR("etcd_keepalive request %s for %llu times failed.", conf_.path_node.c_str(),
                              static_cast<unsigned long long>(keepalive_actor->get_check_times()));
                    is_failed = true;
                    break;
                }
            }

            // 初始化失败则回收资源
            if (is_failed) {
                stop();
                reset();
                return -1;
            }

            return res;
        }

        int etcd_module::reload() {
            // load init cluster member from configure
            util::config::ini_loader &cfg = get_app()->get_configure();

            cfg.dump_to("atproxy.etcd.path", conf_.path_prefix);
            if (!conf_.path_prefix.empty() && conf_.path_prefix[conf_.path_prefix.size() - 1] != '/' &&
                conf_.path_prefix[conf_.path_prefix.size() - 1] != '\\') {
                conf_.path_prefix += '/';
            }

            {
                std::vector<std::string> conf_hosts;
                cfg.dump_to("atproxy.etcd.hosts", conf_hosts);
                if (!conf_hosts.empty()) {
                    etcd_ctx_.set_conf_hosts(conf_hosts);
                }
            }

            {
                util::config::duration_value dur;
                cfg.dump_to("atproxy.etcd.http.timeout", dur, true);
                if (0 != dur.sec || 0 != dur.nsec) {
                    etcd_ctx_.set_conf_http_timeout(detail::convert(dur));
                }
            }

            {
                util::config::duration_value dur;
                cfg.dump_to("atproxy.etcd.cluster.update_interval", dur, true);
                if (0 != dur.sec || 0 != dur.nsec) {
                    etcd_ctx_.set_conf_etcd_members_update_interval(detail::convert(dur));
                }
            }

            {
                util::config::duration_value dur;
                cfg.dump_to("atproxy.etcd.keepalive.timeout", dur, true);
                if (0 != dur.sec || 0 != dur.nsec) {
                    etcd_ctx_.set_conf_keepalive_timeout(detail::convert(dur));
                }
            }

            {
                util::config::duration_value dur;
                cfg.dump_to("atproxy.etcd.keepalive.ttl", dur, true);
                if (0 != dur.sec || 0 != dur.nsec) {
                    etcd_ctx_.set_conf_keepalive_interval(detail::convert(dur));
                }
            }

            {
                util::config::duration_value dur;
                cfg.dump_to("atproxy.etcd.watcher.retry_interval", dur, true);
                if (0 != dur.sec || 0 != dur.nsec) {
                    conf_.watcher_retry_interval = detail::convert(dur);
                }
            }

            {
                util::config::duration_value dur;
                cfg.dump_to("atproxy.etcd.watcher.request_timeout", dur, true);
                if (0 != dur.sec || 0 != dur.nsec) {
                    conf_.watcher_request_timeout = detail::convert(dur);
                }
            }

            std::stringstream ss;
            ss << conf_.path_prefix << get_app()->get_id();
            conf_.path_node = ss.str();
            return 0;
        }

        int etcd_module::http_callback_on_etcd_closed(util::network::http_request &req) {
            etcd_module *self = reinterpret_cast<etcd_module *>(req.get_priv_data());
            if (NULL == self) {
                WLOGERROR("etcd_module get request shouldn't has request without private data");
                return 0;
            }

            self->cleanup_request_.reset();

            // call stop to trigger stop process again.
            self->get_app()->stop();

            return 0;
        }

        int etcd_module::stop() {
            if (!cleanup_request_) {
                cleanup_request_ = etcd_ctx_.close(false);

                if (cleanup_request_ && cleanup_request_->is_running()) {
                    cleanup_request_->set_priv_data(this);
                    cleanup_request_->set_on_complete(http_callback_on_etcd_closed);
                }
            }

            if (cleanup_request_) {
                return 1;
            }

            // recycle all resources
            reset();
            return 0;
        }

        int etcd_module::timeout() {
            reset();
            return 0;
        }

        const char *etcd_module::name() const { return "etcd module"; }

        int etcd_module::tick() {
            // single mode
            if (etcd_ctx_.get_conf_hosts().empty()) {
                return 0;
            }

            // first startup when reloaded
            if (!curl_multi_) {
                int res = init();
                if (res < 0) {
                    WLOGERROR("initialize etcd failed, res: %d", res);
                    get_app()->stop();
                    return -1;
                }
            }

            etcd_ctx_.tick();

            return proxy_mgr_.tick(*get_app());
        }

        void etcd_module::unpack(node_info_t &out, const std::string &json, bool reset_data) {
            if (reset_data) {
                out.action = node_action_t::EN_NAT_UNKNOWN;
                out.id = 0;
                out.next_action_time = 0;
                out.listens.clear();
            }

            rapidjson::Document doc;
            doc.Parse(json.c_str(), json.size());
            if (doc.IsObject()) {
                rapidjson::Value val = doc.GetObject();
                rapidjson::Value::MemberIterator atproxy_iter;
                if (val.MemberEnd() != (atproxy_iter = val.FindMember("id"))) {
                    out.id = atproxy_iter->value.GetUint64();
                }

                if (val.MemberEnd() != (atproxy_iter = val.FindMember("listen"))) {
                    rapidjson::Document::Array nodes = atproxy_iter->value.GetArray();
                    for (rapidjson::Document::Array::ValueIterator iter = nodes.Begin(); iter != nodes.End(); ++iter) {
                        out.listens.push_back(iter->GetString());
                    }
                }
            }
        }

        void etcd_module::pack(const node_info_t &src, std::string &json) {
            rapidjson::Document doc;
            doc.SetObject();

            doc.AddMember("id", src.id, doc.GetAllocator());

            rapidjson::Value listens;
            listens.SetArray();
            for (std::list<std::string>::const_iterator iter = src.listens.begin(); iter != src.listens.end(); ++iter) {
                // only report the channel available on different machine
                if (0 != UTIL_STRFUNC_STRNCASE_CMP("mem:", iter->c_str(), 4) && 0 != UTIL_STRFUNC_STRNCASE_CMP("shm:", iter->c_str(), 4)) {
                    listens.PushBack(rapidjson::StringRef((*iter).c_str(), (*iter).size()), doc.GetAllocator());
                }
            }
            doc.AddMember("listen", listens, doc.GetAllocator());

            // Stringify the DOM
            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            doc.Accept(writer);

            json.assign(buffer.GetString(), buffer.GetSize());
        }

        etcd_module::watcher_callback_t::watcher_callback_t(etcd_module &m) : mod(&m) {}
        void etcd_module::watcher_callback_t::operator()(const ::atframe::component::etcd_response_header &header,
                                                         const ::atframe::component::etcd_watcher::response_t &body) {
            // decode data
            for (size_t i = 0; i < body.events.size(); ++i) {
                const ::atframe::component::etcd_watcher::event_t &evt_data = body.events[i];
                node_info_t node;
                unpack(node, evt_data.kv.value, true);

                if (evt_data.evt_type == ::atframe::component::etcd_watch_event::EN_WEVT_DELETE) {
                    node.action = node_action_t::EN_NAT_DELETE;
                    // trigger manager
                    mod->get_proxy_manager().remove(node.id);
                } else {
                    node.action = node_action_t::EN_NAT_PUT;
                    // trigger manager
                    mod->get_proxy_manager().set(node);
                }
            }
        }
    } // namespace proxy
} // namespace atframe
