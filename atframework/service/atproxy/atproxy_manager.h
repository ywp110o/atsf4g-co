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
                    EN_NAT_GET,
                    EN_NAT_SET,
                    EN_NAT_CREATE,
                    EN_NAT_MODIFY,
                    EN_NAT_REMOVE,
                    EN_NAT_EXPIRE,
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

                node_action_t::type action;
                uint64_t created_index;
                uint64_t modify_index;

                int error_code;
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
            std::map< ::atapp::app::app_id_t, node_info_t> proxy_set_;
        };
    }
}
#endif