#ifndef ATFRAME_SERVICE_ATPROXY_ETCD_MODULE_H
#define ATFRAME_SERVICE_ATPROXY_ETCD_MODULE_H

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

#include <etcdcli/etcd_cluster.h>
#include <etcdcli/etcd_watcher.h>

#include "atproxy_manager.h"


namespace atframe {
    namespace proxy {
        class etcd_module : public ::atapp::module_impl {
        public:
            typedef atproxy_manager::node_action_t node_action_t;
            typedef atproxy_manager::node_info_t node_info_t;
            typedef atproxy_manager::node_list_t node_list_t;

            struct conf_t {
                std::string path_prefix;
                std::string path_node;
                std::chrono::system_clock::duration etcd_init_timeout;
                std::chrono::system_clock::duration watcher_retry_interval;
                std::chrono::system_clock::duration watcher_request_timeout;
            };

        public:
            etcd_module();
            virtual ~etcd_module();

        public:
            void reset();

            virtual int init() UTIL_CONFIG_OVERRIDE;

            virtual int reload() UTIL_CONFIG_OVERRIDE;

            virtual int stop() UTIL_CONFIG_OVERRIDE;

            virtual int timeout() UTIL_CONFIG_OVERRIDE;

            virtual const char *name() const UTIL_CONFIG_OVERRIDE;

            virtual int tick() UTIL_CONFIG_OVERRIDE;

            int update_etcd_members(bool waiting);

        private:
            static void unpack(node_info_t &out, const std::string &path, const std::string &json, bool reset_data);
            static void pack(const node_info_t &out, std::string &json);

            static int http_callback_on_etcd_closed(util::network::http_request &req);

            struct watcher_callback_t {
                etcd_module *mod;

                watcher_callback_t(etcd_module &m);
                void operator()(const ::atframe::component::etcd_response_header &header, const ::atframe::component::etcd_watcher::response_t &evt_data);
            };


        public:
            inline atproxy_manager &get_proxy_manager() { return proxy_mgr_; }
            inline const atproxy_manager &get_proxy_manager() const { return proxy_mgr_; }

        private:
            conf_t conf_;
            util::network::http_request::curl_m_bind_ptr_t curl_multi_;
            util::network::http_request::ptr_t cleanup_request_;
            atproxy_manager proxy_mgr_;
            ::atframe::component::etcd_cluster etcd_ctx_;
        };
    } // namespace proxy
} // namespace atframe

#endif