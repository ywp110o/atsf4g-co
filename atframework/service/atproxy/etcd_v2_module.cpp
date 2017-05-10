#include <sstream>

#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"


#include <common/string_oprs.h>
#include <random/random_generator.h>


#include <atframe/atapp.h>

#include "etcd_v2_module.h"

/**
 * @note APIs just like this(if path=/atapp/proxy/services)
 *    Get node data => curl -i http://localhost:2379/v2/keys/atapp/proxy/services/ID -XGET
 *    Set node data => curl http://localhost:2379/v2/keys/atapp/proxy/services/ID -XPUT -d ttl=5 -d value=json.encode(...)
 *    Renew node data => curl http://localhost:2379/v2/keys/atapp/proxy/services/ID -XPUT -d ttl=5 -d refresh=true -d prevExist=true
 *    Watch => curl "http://localhost:2379/v2/keys/atapp/proxy/services?wait=true&recursive=true&waitIndex=INDEX"
 *    List members => curl http://localhost:2379/v2/members
 */

#define ETCD_API_V2_LIST_MEMBERS "/v2/members"
#define ETCD_API_V2_KEYS "/v2/keys"

namespace atframe {
    namespace proxy {
        etcd_v2_module::etcd_v2_module() : rpc_watch_index_(0), next_keepalive_refresh(false), next_tick_update_etcd_mebers(0) {
            random_generator_.init_seed((util::random::mt19937::result_type)time(NULL));
            conf_.path = "/";
            conf_.http_renew_ttl_timeout = 5000;
            conf_.http_watch_timeout = 3600000;

            conf_.keepalive_timeout = 5000;
            conf_.keepalive_interval = 2000;

            conf_.last_keepalive_tp = std::chrono::system_clock::from_time_t(0);
            conf_.host_index = 0;
        }

        etcd_v2_module::~etcd_v2_module() { reset(); }

        void etcd_v2_module::reset() {
            if (rpc_keepalive_) {
                rpc_keepalive_->set_on_complete(NULL);
            }

            if (curl_multi_) {
                util::network::http_request::destroy_curl_multi(curl_multi_);
            }
        }

        int etcd_v2_module::init() {
            // init curl
            int res = curl_global_init(CURL_GLOBAL_ALL);
            if (res) {
                WLOGERROR("init cURL failed, errcode: %d", res);
                return -1;
            }

            if (conf_.conf_hosts.empty()) {
                WLOGINFO("etcd host not found, start singel mode");
                return 0;
            }

            util::network::http_request::create_curl_multi(get_app()->get_bus_node()->get_evloop(), curl_multi_);
            if (!curl_multi_) {
                WLOGERROR("create curl multi instance failed.");
                return -1;
            }

            // connect to etcd and get all members
            {
                res = update_etcd_members(true);
                if (res < 0) {
                    return res;
                }
            }

            // get from etcd in case of conflict
            {
                util::network::http_request::ptr_t tmp_req = util::network::http_request::create(curl_multi_.get(), conf_.path_node);
                setup_http_request(tmp_req);
                tmp_req->set_opt_timeout(conf_.keepalive_timeout);

                tmp_req->start(util::network::http_request::method_t::EN_MT_GET, true);
                if (util::network::http_request::status_code_t::EN_SCT_NOT_FOUND != tmp_req->get_response_code()) {
                    WLOGERROR("get etcd member failed, atproxy %s already exists.", conf_.path_node.c_str());
                    return -1;
                }
            }

            // start ttl with set
            res = keepalive(false);
            if (res < 0) {
                return res;
            }

            // watch with init index
            res = watch();
            return res;
        }

        int etcd_v2_module::reload() {
            // load init cluster member from configure
            conf_.conf_hosts.clear();

            util::config::ini_loader &cfg = get_app()->get_configure();
            cfg.dump_to("atproxy.etcd.hosts", conf_.conf_hosts);
            cfg.dump_to("atproxy.etcd.timeout", conf_.keepalive_timeout);
            cfg.dump_to("atproxy.etcd.ticktime", conf_.keepalive_interval);
            cfg.dump_to("atproxy.etcd.path", conf_.path);

            cfg.dump_to("atproxy.etcd.http.renew_ttl_timeout", conf_.http_renew_ttl_timeout);
            cfg.dump_to("atproxy.etcd.http.watch_timeout", conf_.http_watch_timeout);

            std::stringstream ss;
            ss << conf_.path << get_app()->get_id();
            conf_.path_node = ss.str();
            return 0;
        }

        int etcd_v2_module::stop() {
            if (rpc_watch_) {
                util::network::http_request::ptr_t tmp_req = util::network::http_request::create(curl_multi_.get(), conf_.path_node);
                setup_http_request(tmp_req);
                tmp_req->set_opt_timeout(conf_.keepalive_timeout);

                // wait to delete content
                tmp_req->start(util::network::http_request::method_t::EN_MT_DELETE, true);
            }

            reset();
            return 0;
        }

        int etcd_v2_module::timeout() {
            reset();
            return 0;
        }

        const char *etcd_v2_module::name() const { return "etcd module"; }

        int etcd_v2_module::tick() {
            // singel mode
            if (conf_.conf_hosts.empty()) {
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

            // update members request
            if (0 != next_tick_update_etcd_mebers && util::time::time_utility::get_now() > next_tick_update_etcd_mebers) {
                update_etcd_members(false);
                next_tick_update_etcd_mebers = 0;
            }

            // if there is no running refresh request, refresh ttl witout set
            keepalive(next_keepalive_refresh);

            // if there is watch response, start a new watch
            watch();

            return proxy_mgr_.tick(*get_app());
        }

        int etcd_v2_module::keepalive(bool refresh) {
            if (rpc_keepalive_) {
                return 0;
            }

            std::chrono::system_clock::duration dur = util::time::time_utility::now() - conf_.last_keepalive_tp;
            if (dur <= std::chrono::milliseconds(conf_.keepalive_interval)) {
                return 0;
            }

            conf_.last_keepalive_tp = util::time::time_utility::now();
            util::network::http_request::ptr_t new_req = util::network::http_request::create(curl_multi_.get(), conf_.path_node);
            if (!new_req) {
                WLOGERROR("create http request to %s failed", conf_.path_node.c_str());
                return -1;
            }

            setup_http_request(new_req);
            new_req->set_opt_timeout(conf_.http_renew_ttl_timeout);

            new_req->add_form_field("ttl", conf_.keepalive_timeout / 1000);
            if (refresh) {
                new_req->add_form_field("refresh", "true");
                new_req->add_form_field("prevExist", "true");
            } else {
                std::string val;
                node_info_t ni;
                ::atapp::app *app = get_app();
                if (NULL == app) {
                    WLOGERROR("owner app not found");
                    return -1;
                }

                ni.id = app->get_id();
                ni.listens = app->get_bus_node()->get_listen_list();
                pack(ni, val);

                new_req->add_form_field("value", val);
            }

            new_req->set_on_complete(std::bind(&etcd_v2_module::on_keepalive_complete, this, std::placeholders::_1));

            // etcd must use PUT method
            int ret = new_req->start(util::network::http_request::method_t::EN_MT_PUT, false);
            if (0 != ret) {
                WLOGERROR("send keepalive request %s failed, ret: %d", conf_.path_node.c_str(), ret);
                new_req->set_on_complete(NULL);
                new_req->cleanup();
                new_req.reset();
            } else {
                rpc_keepalive_.swap(new_req);
            }

            return ret;
        }

        int etcd_v2_module::watch() {
            // do not watch more than one requests
            if (rpc_watch_) {
                return 0;
            }

            // if it's first watch, just pull data without waiting change.
            util::network::http_request::ptr_t new_req;
            if (0 != rpc_watch_index_) {
                // http://localhost:2379/v2/keys/atapp/proxy/services?wait=true&recursive=true&waitIndex=INDEX
                std::stringstream ss;
                ss << conf_.path_watch << "?wait=true&recursive=true&waitIndex=" << rpc_watch_index_;
                std::string url = ss.str();
                new_req = util::network::http_request::create(curl_multi_.get(), url);
                if (!new_req) {
                    WLOGERROR("create http request to %s failed", url.c_str());
                    return -1;
                } else {
                    WLOGDEBUG("watch change %s", url.c_str());
                }
            } else {
                new_req = util::network::http_request::create(curl_multi_.get(), conf_.path_watch);
                if (!new_req) {
                    WLOGERROR("create http request to %s failed", conf_.path_watch.c_str());
                    return -1;
                } else {
                    WLOGDEBUG("watch fetch %s", conf_.path_watch.c_str());
                }
            }

            setup_http_request(new_req);
            new_req->set_opt_timeout(conf_.http_watch_timeout);
            // watch request always use new connection, otherwise it will block ttl renew or etcd member request
            new_req->set_opt_bool(CURLOPT_FRESH_CONNECT, true);
            new_req->set_opt_bool(CURLOPT_FORBID_REUSE, true);

            new_req->set_on_complete(std::bind(&etcd_v2_module::on_watch_complete, this, std::placeholders::_1));
            new_req->set_on_header(std::bind(&etcd_v2_module::on_watch_header, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3,
                                             std::placeholders::_4, std::placeholders::_5));

            // etcd must use PUT method
            int ret = new_req->start(util::network::http_request::method_t::EN_MT_GET, false);
            if (0 != ret) {
                WLOGERROR("setup watch path %s failed, ret: %d", conf_.path_watch.c_str(), ret);
                new_req->set_on_complete(NULL);
                new_req->cleanup();
                new_req.reset();
            } else {
                rpc_watch_.swap(new_req);
            }

            return ret;
        }

        int etcd_v2_module::update_etcd_members(bool waiting) {
            if (rpc_update_members_) {
                return 0;
            }

            std::string selected_host;

            if (conf_.hosts.empty()) {
                if (conf_.conf_hosts.size() > 1) {
                    size_t index = random_generator_.random_between<size_t>(0, conf_.conf_hosts.size());
                    selected_host = conf_.conf_hosts[index];
                } else {
                    selected_host = conf_.conf_hosts[0];
                }
            } else {
                if (conf_.hosts.size() > 1) {
                    size_t index = random_generator_.random_between<size_t>(0, conf_.hosts.size());
                    selected_host = conf_.hosts[index];

                    // remove this host, if got a response, all hosts will be updated
                    // if it's unavailable, we will use another host at next time.
                    if (index != conf_.hosts.size() - 1) {
                        conf_.hosts.back().swap(conf_.hosts[index]);
                    }
                    conf_.hosts.pop_back();
                } else {
                    selected_host = conf_.hosts[0];
                    conf_.hosts.clear();
                }
            }
            std::stringstream ss;
            ss << selected_host << ETCD_API_V2_LIST_MEMBERS;
            std::string url = ss.str();

            util::network::http_request::ptr_t new_req = util::network::http_request::create(curl_multi_.get(), ss.str());
            if (!new_req) {
                setup_update_etcd_members();
                WLOGERROR("create http request to %s failed", url.c_str());
                return -1;
            } else {
                WLOGDEBUG("update etcd member %s", url.c_str());
            }

            setup_http_request(new_req);
            new_req->set_opt_timeout(conf_.keepalive_timeout);

            if (!waiting) {
                new_req->set_on_complete(std::bind(&etcd_v2_module::on_update_etcd_complete, this, std::placeholders::_1));
            }

            int ret = new_req->start(util::network::http_request::method_t::EN_MT_GET, waiting);
            if (waiting &&
                util::network::http_request::status_code_t::EN_ECG_SUCCESS !=
                    util::network::http_request::get_status_code_group(new_req->get_response_code())) {
                WLOGERROR("get etcd member failed, http code: %d\n%s", new_req->get_response_code(), new_req->get_error_msg());

                setup_update_etcd_members();
                return -1;
            }

            if (waiting) {
                ret = select_host(new_req->get_response_stream().str());
                new_req.reset();

            } else if (0 != ret) {
                WLOGERROR("send update members request failed, ret: %d", ret);
                new_req.reset();

                setup_update_etcd_members();
            } else {
                rpc_update_members_.swap(new_req);
            }

            return ret;
        }

        void etcd_v2_module::setup_http_request(util::network::http_request::ptr_t &req) {
            if (!req) {
                return;
            }

            req->set_opt_follow_location(true);
            req->set_opt_ssl_verify_peer(false);
            req->set_opt_http_content_decoding(true);
            req->set_user_agent("etcd client");
        }

        int etcd_v2_module::select_host(const std::string &json_data) {
            if (json_data.empty()) {
                return 0;
            }

            {
                rapidjson::Document json_doc;
                json_doc.Parse(json_data.c_str());
                rapidjson::Document::MemberIterator members = json_doc.FindMember("members");
                if (members == json_doc.MemberEnd()) {
                    WLOGERROR("member data from etcd invalid.\n%s", json_data.c_str());
                    return -1;
                }

                conf_.hosts.clear();
                // get all cluster member
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
                            conf_.hosts.push_back(cli_url_iter->GetString());
                        }
                    }
                }

                if (conf_.hosts.empty()) {
                    WLOGERROR("member data from etcd invalid.\n%s", json_data.c_str());
                    return -1;
                }

                conf_.host_index = random_generator_.random_between<size_t>(0, conf_.hosts.size());
            }

            // select one host
            {
                std::stringstream ss;
                ss << conf_.hosts[conf_.host_index] << ETCD_API_V2_KEYS << conf_.path;
                conf_.path_watch = ss.str();

                if (conf_.path.empty() || '/' != *conf_.path.rbegin()) {
                    ss << "/";
                }
                ss << get_app()->get_id();
                conf_.path_node = ss.str();
            }

            return 0;
        }

        void etcd_v2_module::setup_update_etcd_members() {
            // update members for 1-2s later
            next_tick_update_etcd_mebers = util::time::time_utility::get_now() + 1;
        }

        void etcd_v2_module::unpack(node_info_t &out, rapidjson::Value &node, rapidjson::Value *prev_node, bool reset_data) {
            if (reset_data) {
                out.action = node_action_t::EN_NAT_UNKNOWN;
                out.created_index = 0;
                out.modify_index = 0;
                out.id = 0;
                out.error_code = 0;
                out.listens.clear();
            }

            if (!node.IsObject()) {
                return;
            }

            rapidjson::Value::MemberIterator iter;
            bool has_data = false;

#define IF_SELECT_DATA(x)                                                         \
    has_data = (node.MemberEnd() != (iter = node.FindMember(x)));                 \
    if (!has_data && NULL != prev_node) {                                         \
        has_data = (prev_node->MemberEnd() != (iter = prev_node->FindMember(x))); \
    }                                                                             \
    if (has_data)

            if (reset_data) {
                IF_SELECT_DATA("action") {
                    const char *action = iter->value.GetString();
                    if (0 == UTIL_STRFUNC_STRNCASE_CMP("get", action, 3)) {
                        out.action = node_action_t::EN_NAT_GET;
                    } else if (0 == UTIL_STRFUNC_STRNCASE_CMP("set", action, 3)) {
                        out.action = node_action_t::EN_NAT_SET;
                    } else if (0 == UTIL_STRFUNC_STRNCASE_CMP("create", action, 6)) {
                        out.action = node_action_t::EN_NAT_CREATE;
                    } else if (0 == UTIL_STRFUNC_STRNCASE_CMP("delete", action, 6)) {
                        out.action = node_action_t::EN_NAT_REMOVE;
                    } else if (0 == UTIL_STRFUNC_STRNCASE_CMP("expire", action, 6)) {
                        out.action = node_action_t::EN_NAT_EXPIRE;
                    } else if (0 == UTIL_STRFUNC_STRNCASE_CMP("update", action, 6)) {
                        out.action = node_action_t::EN_NAT_MODIFY;
                    } else if (0 == UTIL_STRFUNC_STRNCASE_CMP("compareAndSwap", action, 14)) {
                        out.action = node_action_t::EN_NAT_MODIFY;
                    } else if (0 == UTIL_STRFUNC_STRNCASE_CMP("compareAndDelete", action, 16)) {
                        out.action = node_action_t::EN_NAT_REMOVE;
                    }
                }
            }

            IF_SELECT_DATA("modifiedIndex") { out.modify_index = iter->value.GetUint64(); }

            IF_SELECT_DATA("createdIndex") { out.created_index = iter->value.GetUint64(); }

            IF_SELECT_DATA("errorCode") { out.error_code = iter->value.GetInt(); }

            IF_SELECT_DATA("value") {
                rapidjson::Document doc;
                doc.Parse(iter->value.GetString());
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
            else {
                IF_SELECT_DATA("node") {
                    rapidjson::Value::MemberIterator prev_iter = node.FindMember("prevNode");
                    unpack(out, iter->value, node.MemberEnd() == prev_iter ? NULL : &prev_iter->value, false);
                }
            }

#undef IF_SELECT_DATA
        }

        void etcd_v2_module::unpack(node_list_t &out, rapidjson::Value &node, bool reset_data) {
            if (reset_data) {
                out.action = node_action_t::EN_NAT_UNKNOWN;
                out.created_index = 0;
                out.modify_index = 0;
                out.error_code = 0;
            }

            if (!node.IsObject()) {
                return;
            }

            rapidjson::Value::MemberIterator iter;

            if (reset_data && node.MemberEnd() != (iter = node.FindMember("action"))) {
                const char *action = iter->value.GetString();
                if (0 == UTIL_STRFUNC_STRNCASE_CMP("get", action, 3)) {
                    out.action = node_action_t::EN_NAT_GET;
                } else if (0 == UTIL_STRFUNC_STRNCASE_CMP("set", action, 3)) {
                    out.action = node_action_t::EN_NAT_SET;
                } else if (0 == UTIL_STRFUNC_STRNCASE_CMP("create", action, 6)) {
                    out.action = node_action_t::EN_NAT_CREATE;
                } else if (0 == UTIL_STRFUNC_STRNCASE_CMP("delete", action, 6)) {
                    out.action = node_action_t::EN_NAT_REMOVE;
                } else if (0 == UTIL_STRFUNC_STRNCASE_CMP("expire", action, 6)) {
                    out.action = node_action_t::EN_NAT_EXPIRE;
                } else if (0 == UTIL_STRFUNC_STRNCASE_CMP("update", action, 6)) {
                    out.action = node_action_t::EN_NAT_MODIFY;
                } else if (0 == UTIL_STRFUNC_STRNCASE_CMP("compareAndSwap", action, 14)) {
                    out.action = node_action_t::EN_NAT_MODIFY;
                } else if (0 == UTIL_STRFUNC_STRNCASE_CMP("compareAndDelete", action, 16)) {
                    out.action = node_action_t::EN_NAT_REMOVE;
                }
            }

            if (node.MemberEnd() != (iter = node.FindMember("modifiedIndex"))) {
                out.modify_index = iter->value.GetUint64();
            }

            if (node.MemberEnd() != (iter = node.FindMember("createdIndex"))) {
                out.created_index = iter->value.GetUint64();
            }

            if (node.MemberEnd() != (iter = node.FindMember("errorCode"))) {
                out.error_code = iter->value.GetInt();
            }

            if (node.MemberEnd() != (iter = node.FindMember("nodes"))) {
                rapidjson::Document::Array nodes = iter->value.GetArray();
                for (rapidjson::Document::Array::ValueIterator iter = nodes.Begin(); iter != nodes.End(); ++iter) {
                    out.nodes.push_back(node_info_t());
                    node_info_t &n = out.nodes.back();
                    unpack(n, *iter, NULL, true);

                    if (0 == out.created_index) {
                        out.created_index = n.created_index;
                    }
                    if (0 == out.modify_index) {
                        out.modify_index = n.modify_index;
                    }
                    if (0 == out.error_code) {
                        out.error_code = n.error_code;
                    }

                    // assign action
                    n.action = out.action;
                }
            } else if (node.MemberEnd() != (iter = node.FindMember("node"))) {
                bool is_dir = false;
                rapidjson::Value::MemberIterator isdir_iter = iter->value.FindMember("dir");
                if (iter->value.MemberEnd() != isdir_iter) {
                    is_dir = isdir_iter->value.GetBool();
                }

                if (is_dir) {
                    unpack(out, iter->value, false);
                } else {
                    rapidjson::Value::MemberIterator prev_iter = node.FindMember("prevNode");

                    out.nodes.push_back(node_info_t());
                    node_info_t &n = out.nodes.back();
                    unpack(n, iter->value, node.MemberEnd() == prev_iter ? NULL : &prev_iter->value, false);

                    if (0 == out.created_index) {
                        out.created_index = n.created_index;
                    }
                    if (0 == out.modify_index) {
                        out.modify_index = n.modify_index;
                    }
                    if (0 == out.error_code) {
                        out.error_code = n.error_code;
                    }

                    // assign action
                    n.action = out.action;
                }
            }
        }

        void etcd_v2_module::unpack(node_info_t &out, const std::string &json) {
            if (json.empty()) {
                return;
            }

            rapidjson::Document doc;
            doc.Parse(json.c_str());

            rapidjson::Value v = doc.GetObject();
            unpack(out, v, NULL, true);
        }

        void etcd_v2_module::unpack(node_list_t &out, const std::string &json) {
            if (json.empty()) {
                return;
            }

            rapidjson::Document doc;
            doc.Parse(json.c_str());

            rapidjson::Value v = doc.GetObject();
            unpack(out, v, true);
        }

        void etcd_v2_module::pack(const node_info_t &src, std::string &json) {
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

        int etcd_v2_module::on_keepalive_complete(util::network::http_request &req) {
            if (0 != req.get_error_code() ||
                util::network::http_request::status_code_t::EN_ECG_SUCCESS != util::network::http_request::get_status_code_group(req.get_response_code())) {

                // only network error will trigger a etcd member update
                if (0 != req.get_error_code()) {
                    setup_update_etcd_members();
                }
                WLOGERROR("keepalive failed, error code: %d, http code: %d\n%s", req.get_error_code(), req.get_response_code(), req.get_error_msg());
            }

            // if not found, keepalive(false)

            if (util::network::http_request::status_code_t::EN_SCT_NOT_FOUND == req.get_response_code()) {
                next_keepalive_refresh = false;
            } else {
                next_keepalive_refresh = true;
            }

            node_info_t ni;
            unpack(ni, req.get_response_stream().str());

            // 100 == etcd not found
            if (100 == ni.error_code) {
                next_keepalive_refresh = false;
            }

            rpc_keepalive_.reset();
            conf_.last_keepalive_tp = util::time::time_utility::now();
            return 0;
        }

        int etcd_v2_module::on_watch_complete(util::network::http_request &req) {
            int errcode = req.get_error_code();
            if (0 != errcode ||
                util::network::http_request::status_code_t::EN_ECG_SUCCESS != util::network::http_request::get_status_code_group(req.get_response_code())) {

                // skip timeout
                if (CURLE_AGAIN == errcode || CURLE_OPERATION_TIMEDOUT == errcode) {
                    rpc_watch_.reset();
                    return 0;
                } else {
                    // only network error will trigger a etcd member update
                    if (0 != errcode) {
                        setup_update_etcd_members();
                    }
                    WLOGERROR("watch failed, error code: %d, http code: %d\n%s", req.get_error_code(), req.get_response_code(), req.get_error_msg());
                }
            }

            // use http header: X-Etcd-Index

            std::string msg = req.get_response_stream().str();
            node_list_t nl;
            unpack(nl, msg);
            WLOGDEBUG("got watch message:\n%s", msg.c_str());

            // after these if sentense, nl will not be available anymore
            if (node_action_t::EN_NAT_GET == nl.action && !nl.nodes.empty()) {
                proxy_mgr_.reset(nl);
            } else {
                // if it not a get request, the header will contain previews index
                uint64_t index = 0;
                for (std::list<node_info_t>::iterator iter = nl.nodes.begin(); iter != nl.nodes.end(); ++iter) {
                    if (iter->modify_index > index) {
                        index = iter->modify_index;
                    }

                    switch (iter->action) {

                    case node_action_t::EN_NAT_SET:
                    case node_action_t::EN_NAT_CREATE:
                    case node_action_t::EN_NAT_MODIFY: {
                        // update all new nodes
                        if (iter->listens.empty()) {
                            proxy_mgr_.remove(iter->id);
                        } else {
                            proxy_mgr_.set(*iter);
                        }
                    }
                    case node_action_t::EN_NAT_REMOVE:
                    case node_action_t::EN_NAT_EXPIRE: {
                        // remove all expired/removed nodes
                        proxy_mgr_.remove(iter->id);
                        break;
                    }
                    default: {
                        WLOGERROR("unknown action");
                        break;
                    }
                    }
                }

                if (index != 0) {
                    rpc_watch_index_ = index + 1;
                }
            }

            rpc_watch_.reset();
            return 0;
        }

        int etcd_v2_module::on_watch_header(util::network::http_request &req, const char *key, size_t keylen, const char *val, size_t vallen) {
            if (NULL != key && 0 == UTIL_STRFUNC_STRNCASE_CMP("X-Etcd-Index", key, keylen)) {
                if (NULL != val && vallen > 0) {
                    rpc_watch_index_ = static_cast<uint64_t>(strtoull(val, NULL, 10)) + 1;
                }
            }

            return 0;
        }

        int etcd_v2_module::on_update_etcd_complete(util::network::http_request &req) {
            node_list_t nl;

            if (0 != req.get_error_code() ||
                util::network::http_request::status_code_t::EN_ECG_SUCCESS != util::network::http_request::get_status_code_group(req.get_response_code())) {
                setup_update_etcd_members();
                WLOGERROR("update etcd members failed, error code: %d, http code: %d\n%s", req.get_error_code(), req.get_response_code(), req.get_error_msg());
            }

            std::string msg = req.get_response_stream().str();
            WLOGDEBUG("got etcd members message:\n%s", msg.c_str());

            select_host(msg);
            if (conf_.hosts.empty()) {
                setup_update_etcd_members();
            }

            rpc_update_members_.reset();
            return 0;
        }
    }
}
