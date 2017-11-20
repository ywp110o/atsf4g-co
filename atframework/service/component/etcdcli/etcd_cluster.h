/**
 * etcd_cluster.h
 *
 *  Created on: 2017-11-17
 *      Author: owent
 *
 *  Released under the MIT license
 */

#ifndef ATFRAME_SERVICE_COMPONENT_ETCDCLI_ETCD_CLUSTER_H
#define ATFRAME_SERVICE_COMPONENT_ETCDCLI_ETCD_CLUSTER_H

#pragma once

#include <ctime>
#include <list>
#include <string>
#include <vector>

#include <std/chrono.h>
#include <std/smart_ptr.h>

#include <network/http_request.h>
#include <random/random_generator.h>
#include <time/time_utility.h>

#include "etcd_packer.h"

namespace atframe {
    namespace component {
        class etcd_cluster {
        public:
            struct flag_t {
                enum type {
                    CLOSING = 0x0001,      // closeing
                    ENABLE_LEASE = 0x0100, // enable auto get lease
                };
            };

            struct conf_t {
                std::vector<std::string> conf_hosts;
                std::vector<std::string> hosts;
                std::chrono::system_clock::duration http_cmd_timeout;

                // generated data for cluster members
                std::string path_node;
                std::chrono::system_clock::time_point etcd_members_next_update_time;
                std::chrono::system_clock::duration etcd_members_update_interval;

                // generated data for lease
                int64_t lease;
                std::chrono::system_clock::time_point keepalive_next_update_time;
                std::chrono::system_clock::duration keepalive_timeout;
                std::chrono::system_clock::duration keepalive_interval;
            };

        public:
            etcd_cluster();
            ~etcd_cluster();

            void init(const util::network::http_request::curl_m_bind_ptr_t &curl_mgr);
            void set_hosts(const std::vector<std::string> &hosts);

            void close(bool wait = false);
            void reset();
            int tick();

            inline bool check_flag(uint32_t f) const { return 0 != (flags_ & f); };
            void set_flag(flag_t::type f, bool v);

            time_t get_http_timeout() const;

        private:
            bool create_request_member_update();
            static int libcurl_callback_on_member_update(util::network::http_request &req);

            bool create_request_lease_grant();
            bool create_request_lease_keepalive();
            static int libcurl_callback_on_lease_keepalive(util::network::http_request &req);
            util::network::http_request::ptr_t create_request_lease_revoke();

        public:
            static void setup_http_request(util::network::http_request::ptr_t &req, rapidjson::Document &doc, time_t timeout);

        private:
            uint32_t flags_;
            util::random::mt19937 random_generator_;
            conf_t conf_;
            util::network::http_request::curl_m_bind_ptr_t curl_multi_;
            util::network::http_request::ptr_t rpc_update_members_;
            util::network::http_request::ptr_t rpc_keepalive_;
        };
    } // namespace component
} // namespace atframe

#endif