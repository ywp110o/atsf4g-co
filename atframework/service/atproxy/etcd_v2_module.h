#ifndef _ATFRAME_SERVICE_ATPROXY_ETCD_V2_MODULE_H_
#define _ATFRAME_SERVICE_ATPROXY_ETCD_V2_MODULE_H_

#pragma once

#include <string>
#include <vector>
#include <list>
#include <ctime>
#include <std/smart_ptr.h>

#include "rapidjson/document.h"

#include <time/time_utility.h>
#include <network/http_request.h>

#include <atframe/atapp_module_impl.h>

namespace atframe {
    namespace proxy {
        class etcd_v2_module : public ::atapp::module_impl {
        public:
            struct conf_t {
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

            struct node_action_t {
                enum type {
                    EN_NAT_NONE = 0,
                    EN_NAT_ADD,
                    EN_NAT_MOD,
                    EN_NAT_REMOVE,
                };
            };
            struct node_info_t {
                ::atapp::app::app_id_t id;
                std::list<std::string> listens;

                node_action_t::type action;
                size_t created_index;
                size_t modify_index;

                int error_code;
            };

            struct node_list_t {
                std::list<node_info_t> nodes;
                uint64_t created_index;
                uint64_t modify_index;

                int error_code;
            };
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

        private:
            void  setup_http_request(util::network::http_request::ptr_t& req);
            int select_host(const std::string& json_data);

            void unpack(node_info_t& out, rapidjson::Value& node, bool reset_data);
            void unpack(node_list_t& out, rapidjson::Value& node, bool reset_data);

            void unpack(node_info_t& out, const std::string& json);
            void unpack(node_list_t& out, const std::string& json);

            void pack(const node_info_t& out, std::string& json);

            int on_keepalive_complete(util::network::http_request& req);

        private:
            util::network::http_request::curl_m_bind_ptr_t curl_multi_;
            CURLM *curl_handle_;
            conf_t conf_;
            util::network::http_request::ptr_t rpc_keepalive_;
            util::network::http_request::ptr_t rpc_watch_;

            bool next_keepalive_refresh;
        };
    }
}

#endif