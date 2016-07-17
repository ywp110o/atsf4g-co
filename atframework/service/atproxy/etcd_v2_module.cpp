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
        etcd_v2_module::etcd_v2_module() : curl_handle_(NULL), next_keepalive_refresh(false) {
            conf_.hosts.clear();
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

            util::network::http_request::destroy_curl_multi(curl_multi_);
        }

        int etcd_v2_module::init() {
            // init curl
            int res = curl_global_init(CURL_GLOBAL_ALL);
            if (res) {
                WLOGERROR("init cURL failed, errcode: %d", res);
                return -1;
            }

            if (conf_.hosts.empty()) {
                WLOGERROR("etcd host can not be empty");
                return -1;
            }

            util::network::http_request::create_curl_multi(get_app()->get_bus_node()->get_evloop(), curl_multi_);
            if (!curl_multi_) {
                WLOGERROR("create curl multi instance failed.");
                return -1;
            }

            // connect to etcd and get all members
            {
                util::random::mt19937 rnd;
                std::string selected_host;
                if (conf_.hosts.size() > 1) {
                    size_t index = rnd.random_between<size_t>(0, conf_.hosts.size());
                    selected_host = conf_.hosts[index];
                } else {
                    selected_host = conf_.hosts[0];
                }
                std::stringstream ss;
                ss << selected_host << ETCD_API_V2_LIST_MEMBERS;
                util::network::http_request::ptr_t tmp_req = util::network::http_request::create(curl_multi_.get(), ss.str());
                setup_http_request(tmp_req);
                tmp_req->set_opt_timeout(conf_.keepalive_timeout);

                tmp_req->start(util::network::http_request::method_t::EN_MT_GET, true);
                if (util::network::http_request::status_code_t::EN_ECG_SUCCESS !=
                    util::network::http_request::get_status_code_group(tmp_req->get_response_code())) {
                    WLOGERROR("get etcd member failed, http code: %d\n%s", tmp_req->get_response_code(), tmp_req->get_error_msg());
                    return -1;
                }

                res = select_host(tmp_req->get_response_stream().str());
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
            util::config::ini_loader &cfg = get_app()->get_configure();
            cfg.dump_to("atproxy.etcd.hosts", conf_.hosts);
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
            // TODO if there is no running refresh request, refresh ttl witout set
            keepalive(next_keepalive_refresh);

            // TODO if there is watch response, start a new watch
            // TODO if there is any new proxy, add it to manager and start a connect
            // TODO if there is any proxy removed, remove it from manager list
            return 0;
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
            rpc_keepalive_ = util::network::http_request::create(curl_multi_.get(), conf_.path_node);
            if (!rpc_keepalive_) {
                WLOGERROR("create http request to %s failed", conf_.path_node.c_str());
                return -1;
            }

            setup_http_request(rpc_keepalive_);
            rpc_keepalive_->set_opt_timeout(conf_.http_renew_ttl_timeout);

            rpc_keepalive_->add_form_field("ttl", conf_.keepalive_timeout / 1000);
            if (refresh) {
                rpc_keepalive_->add_form_field("refresh", "true");
                rpc_keepalive_->add_form_field("prevExist", "true");
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

                rpc_keepalive_->add_form_field("value", val);
            }

            rpc_keepalive_->set_on_complete(std::bind(&etcd_v2_module::on_keepalive_complete, this, std::placeholders::_1));

            // etcd must use PUT method
            int ret = rpc_keepalive_->start(util::network::http_request::method_t::EN_MT_PUT, false);
            if (0 != ret) {
                rpc_keepalive_->set_on_complete(NULL);
                rpc_keepalive_->cleanup();
                rpc_keepalive_.reset();
            }

            return ret;
        }

        int etcd_v2_module::watch() {
            // do not watch more than one requests
            if (rpc_watch_) {
                return 0;
            }

            return 0;
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
            util::random::mt19937 rnd;

            {
                rapidjson::Document json_doc;
                json_doc.Parse(json_data.c_str());
                if (!json_doc.HasMember("members")) {
                    WLOGERROR("member data from etcd invalid.\n%s", json_data.c_str());
                    return -1;
                }

                std::vector<std::string> hosts;
                // get all cluster member
                rapidjson::Document::Array all_members = json_doc["members"].GetArray();
                for (rapidjson::SizeType i = 0; i < all_members.Size(); ++i) {
                    rapidjson::Document::Array all_client_urls = all_members[i]["clientURLs"].GetArray();
                    for (rapidjson::SizeType j = 0; j < all_client_urls.Size(); ++j) {
                        if (all_client_urls[j].GetStringLength() > 0) {
                            hosts.push_back(all_client_urls[j].GetString());
                        }
                    }
                }

                if (hosts.empty()) {
                    WLOGERROR("member data from etcd invalid.\n%s", json_data.c_str());
                    return -1;
                }

                conf_.host_index = rnd.random_between<size_t>(0, conf_.hosts.size());
                conf_.hosts.swap(hosts);
            }

            // select one host
            {
                std::stringstream ss;
                ss << conf_.hosts[conf_.host_index] << ETCD_API_V2_KEYS << conf_.path;
                if (conf_.path.empty() || '/' != conf_.path.back()) {
                    ss << "/";
                }
                conf_.path_watch = ss.str();
                ss << get_app()->get_id();
                conf_.path_node = ss.str();
            }

            return 0;
        }

        void etcd_v2_module::unpack(node_info_t &out, rapidjson::Value &node, bool reset_data) {
            if (reset_data) {
                out.action = node_action_t::EN_NAT_NONE;
                out.created_index = 0;
                out.modify_index = 0;
                out.id = 0;
                out.error_code = 0;
                out.listens.clear();
            }

            if (!node.IsObject()) {
                return;
            }

            if (node.HasMember("action")) {
                const char *action = node["action"].GetString();
                if (0 == UTIL_STRFUNC_STRNCASE_CMP("get", action, 3)) {
                    out.action = node_action_t::EN_NAT_ADD;
                } else if (0 == UTIL_STRFUNC_STRNCASE_CMP("set", action, 3)) {
                    out.action = node_action_t::EN_NAT_ADD;
                } else if (0 == UTIL_STRFUNC_STRNCASE_CMP("create", action, 6)) {
                    out.action = node_action_t::EN_NAT_ADD;
                } else if (0 == UTIL_STRFUNC_STRNCASE_CMP("delete", action, 6)) {
                    out.action = node_action_t::EN_NAT_REMOVE;
                } else if (0 == UTIL_STRFUNC_STRNCASE_CMP("expire", action, 6)) {
                    out.action = node_action_t::EN_NAT_REMOVE;
                } else if (0 == UTIL_STRFUNC_STRNCASE_CMP("update", action, 6)) {
                    out.action = node_action_t::EN_NAT_MOD;
                } else if (0 == UTIL_STRFUNC_STRNCASE_CMP("compareAndSwap", action, 14)) {
                    out.action = node_action_t::EN_NAT_MOD;
                } else if (0 == UTIL_STRFUNC_STRNCASE_CMP("compareAndDelete", action, 16)) {
                    out.action = node_action_t::EN_NAT_REMOVE;
                }
            }

            if (node.HasMember("modifiedIndex")) {
                out.modify_index = node["modifiedIndex"].GetUint64();
            }

            if (node.HasMember("createdIndex")) {
                out.created_index = node["createdIndex"].GetUint64();
            }

            if (node.HasMember("errorCode")) {
                out.error_code = node["errorCode"].GetInt();
            }

            if (node.HasMember("value")) {
                rapidjson::Document doc;
                doc.Parse(node["value"].GetString());
                if (doc.IsObject()) {
                    rapidjson::Value val = doc.GetObjectA();
                    if (val.HasMember("id")) {
                        out.id = val["id"].GetUint64();
                    }

                    if (val.HasMember("listen")) {
                        rapidjson::Document::Array nodes = val["listen"].GetArray();
                        for (rapidjson::Document::Array::ValueIterator iter = nodes.begin(); iter != nodes.end(); ++iter) {
                            out.listens.push_back(iter->GetString());
                        }
                    }
                }
            } else if (node.HasMember("node")) {
                unpack(out, node["node"], false);
            }
        }

        void etcd_v2_module::unpack(node_list_t &out, rapidjson::Value &node, bool reset_data) {
            if (reset_data) {
                out.created_index = 0;
                out.modify_index = 0;
                out.error_code = 0;
            }

            if (!node.IsObject()) {
                return;
            }

            if (node.HasMember("modifiedIndex")) {
                out.modify_index = node["modifiedIndex"].GetUint64();
            }

            if (node.HasMember("createdIndex")) {
                out.created_index = node["createdIndex"].GetUint64();
            }

            if (node.HasMember("errorCode")) {
                out.error_code = node["errorCode"].GetInt();
            }

            if (node.HasMember("nodes")) {
                rapidjson::Document::Array nodes = node["nodes"].GetArray();
                for (rapidjson::Document::Array::ValueIterator iter = nodes.begin(); iter != nodes.end(); ++iter) {
                    out.nodes.push_back(node_info_t());
                    unpack(out.nodes.back(), *iter, true);
                }
            } else if (node.HasMember("node")) {
                unpack(out, node["node"], false);
            }
        }

        void etcd_v2_module::unpack(node_info_t &out, const std::string &json) {
            if (json.empty()) {
                return;
            }

            rapidjson::Document doc;
            doc.Parse(json.c_str());

            rapidjson::Value v = doc.GetObjectA();
            unpack(out, v, true);
        }

        void etcd_v2_module::unpack(node_list_t &out, const std::string &json) {
            if (json.empty()) {
                return;
            }

            rapidjson::Document doc;
            doc.Parse(json.c_str());

            rapidjson::Value v = doc.GetObjectA();
            unpack(out, v, true);
        }

        void etcd_v2_module::pack(const node_info_t &src, std::string &json) {
            rapidjson::Document doc;
            doc.SetObject();

            doc.AddMember("id", src.id, doc.GetAllocator());

            rapidjson::Value listens;
            listens.SetArray();
            for (std::list<std::string>::const_iterator iter = src.listens.begin(); iter != src.listens.end(); ++iter) {
                listens.PushBack(rapidjson::StringRef((*iter).c_str(), (*iter).size()), doc.GetAllocator());
            }
            doc.AddMember("listen", listens, doc.GetAllocator());

            // Stringify the DOM
            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            doc.Accept(writer);

            json.assign(buffer.GetString(), buffer.GetSize());
        }

        int etcd_v2_module::on_keepalive_complete(util::network::http_request &req) {
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
    }
}