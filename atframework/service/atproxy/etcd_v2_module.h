#ifndef _ATFRAME_SERVICE_ATPROXY_ETCD_V2_MODULE_H_
#define _ATFRAME_SERVICE_ATPROXY_ETCD_V2_MODULE_H_

#pragma once

#include <ctime>
#include <list>
#include <std/smart_ptr.h>
#include <string>
#include <vector>


#include "rapidjson/document.h"

#include <network/http_request.h>
#include <random/random_generator.h>
#include <time/time_utility.h>


#include <atframe/atapp_module_impl.h>

#include "atproxy_manager.h"

namespace atframe {
    namespace proxy {
        class etcd_v2_module : public ::atapp::module_impl {
        public:
            struct conf_t {
                std::vector<std::string> conf_hosts;
                std::vector<std::string> hosts;
                time_t keepalive_timeout;
                time_t keepalive_interval;
                std::string path;

                time_t http_renew_ttl_timeout;
                time_t http_watch_timeout;

                // generated path
                std::string path_watch;
                std::string path_node;
                size_t host_index;
                std::chrono::system_clock::time_point last_keepalive_tp;
            };

            typedef atproxy_manager::node_action_t node_action_t;
            typedef atproxy_manager::node_info_t node_info_t;
            typedef atproxy_manager::node_list_t node_list_t;

        public:
            etcd_v2_module();
            virtual ~etcd_v2_module();

        public:
            void reset();

            virtual int init() CLASS_OVERRIDE;

            virtual int reload() CLASS_OVERRIDE;

            virtual int stop() CLASS_OVERRIDE;

            virtual int timeout() CLASS_OVERRIDE;

            virtual const char *name() const CLASS_OVERRIDE;

            virtual int tick() CLASS_OVERRIDE;

            int keepalive(bool refresh);

            int watch();

            int update_etcd_members(bool waiting);

        private:
            void setup_http_request(util::network::http_request::ptr_t &req);
            int select_host(const std::string &json_data);
            void setup_update_etcd_members();

            void unpack(node_info_t &out, rapidjson::Value &node, rapidjson::Value *prev_node, bool reset_data);
            void unpack(node_list_t &out, rapidjson::Value &node, bool reset_data);

            void unpack(node_info_t &out, const std::string &json);
            void unpack(node_list_t &out, const std::string &json);

            void pack(const node_info_t &out, std::string &json);

            int on_keepalive_complete(util::network::http_request &req);
            int on_watch_complete(util::network::http_request &req);
            int on_watch_header(util::network::http_request &req, const char *key, size_t keylen, const char *val, size_t vallen);
            int on_update_etcd_complete(util::network::http_request &req);

        public:
            inline atproxy_manager &get_proxy_manager() { return proxy_mgr_; }
            inline const atproxy_manager &get_proxy_manager() const { return proxy_mgr_; }

        private:
            util::random::mt19937 random_generator_;
            util::network::http_request::curl_m_bind_ptr_t curl_multi_;
            conf_t conf_;
            util::network::http_request::ptr_t rpc_keepalive_;
            util::network::http_request::ptr_t rpc_watch_;
            uint64_t rpc_watch_index_;
            util::network::http_request::ptr_t rpc_update_members_;

            bool next_keepalive_refresh;
            time_t next_tick_update_etcd_mebers;

            atproxy_manager proxy_mgr_;
        };
    }
}

#endif