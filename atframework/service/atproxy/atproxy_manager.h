#ifndef ATFRAME_SERVICE_ATPROXY_ATPROXY_MANAGER_H
#define ATFRAME_SERVICE_ATPROXY_ATPROXY_MANAGER_H

#pragma once

#include <ctime>
#include <list>
#include <map>
#include <string>
#include <vector>


#include <atframe/atapp.h>

namespace atframe {
    namespace proxy {
        class atproxy_manager {
        public:
            struct node_action_t {
                enum type {
                    EN_NAT_UNKNOWN = 0,
                    EN_NAT_PUT,
                    EN_NAT_DELETE,
                };
            };
            struct node_info_t {
                ::atapp::app::app_id_t id;
                std::list<std::string> listens;

                node_action_t::type action;
                time_t next_action_time;
            };

            struct node_list_t {
                std::list<node_info_t> nodes;
            };

        private:
            typedef struct {
                time_t timeout_sec;
                ::atapp::app::app_id_t proxy_id;
            } check_info_t;

        public:
            int tick(const ::atapp::app &app);

            int set(node_info_t &proxy_info);

            int remove(::atapp::app::app_id_t id);

            int reset(node_list_t &all_proxys);

            int on_connected(const ::atapp::app &app, ::atapp::app::app_id_t id);

            int on_disconnected(const ::atapp::app &app, ::atapp::app::app_id_t id);

        private:
            void swap(node_info_t &l, node_info_t &r);

        private:
            std::list<check_info_t> check_list_;
            typedef std::map< ::atapp::app::app_id_t, node_info_t> proxy_set_t;
            proxy_set_t proxy_set_;
        };
    } // namespace proxy
} // namespace atframe
#endif