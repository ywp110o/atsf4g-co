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
        class etcd_keepalive;
        class etcd_watcher;

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

            util::network::http_request::ptr_t close(bool wait = false);
            void reset();
            int tick();

            inline bool check_flag(uint32_t f) const { return 0 != (flags_ & f); };
            void set_flag(flag_t::type f, bool v);

            // ====================== apis for configure ==================
            inline const std::vector<std::string> &get_available_hosts() const { return conf_.hosts; }
            inline const std::string &get_selected_host() const { return conf_.path_node; }

            inline int64_t get_keepalive_lease() const { return get_lease(); }

            inline void set_conf_hosts(const std::vector<std::string> &hosts) { conf_.conf_hosts = hosts; }
            inline const std::vector<std::string> &get_conf_hosts() const { return conf_.conf_hosts; }

            inline void set_conf_http_timeout(std::chrono::system_clock::duration v) { conf_.http_cmd_timeout = v; }
            inline void set_conf_http_timeout_sec(time_t v) { set_conf_http_timeout(std::chrono::seconds(v)); }
            inline const std::chrono::system_clock::duration &get_conf_http_timeout() const { return conf_.http_cmd_timeout; }
            time_t get_http_timeout_ms() const;

            inline void set_conf_etcd_members_update_interval(std::chrono::system_clock::duration v) { conf_.etcd_members_update_interval = v; }
            inline void set_conf_etcd_members_update_interval_min(time_t v) { set_conf_etcd_members_update_interval(std::chrono::minutes(v)); }
            inline const std::chrono::system_clock::duration &get_conf_etcd_members_update_interval() const { return conf_.etcd_members_update_interval; }

            inline void set_conf_keepalive_timeout(std::chrono::system_clock::duration v) { conf_.keepalive_timeout = v; }
            inline void set_conf_keepalive_timeout_sec(time_t v) { set_conf_keepalive_timeout(std::chrono::seconds(v)); }
            inline const std::chrono::system_clock::duration &get_conf_keepalive_timeout() const { return conf_.keepalive_timeout; }

            inline void set_conf_keepalive_interval(std::chrono::system_clock::duration v) { conf_.keepalive_interval = v; }
            inline void set_conf_keepalive_interval_sec(time_t v) { set_conf_keepalive_interval(std::chrono::seconds(v)); }
            inline const std::chrono::system_clock::duration &get_conf_keepalive_interval() const { return conf_.keepalive_interval; }

            // ================== apis for sub-services ==================
            bool add_keepalive(const std::shared_ptr<etcd_keepalive> &keepalive);
            bool add_retry_keepalive(const std::shared_ptr<etcd_keepalive> &keepalive);
            bool add_watcher(const std::shared_ptr<etcd_watcher> &watcher);

            // ================== apis of create request for key-value operation ==================
        public:
            /**
             * @brief               create request for range get key-value data
             * @param key	        key is the first key for the range. If range_end is not given, the request only looks up key.
             * @param range_end	    range_end is the upper bound on the requested range [key, range_end). just like etcd_packer::pack_key_range
             * @param limit	        limit is a limit on the number of keys returned for the request. When limit is set to 0, it is treated as no limit.
             * @param revision	    revision is the point-in-time of the key-value store to use for the range. If revision is less or equal to zero, the range
             *                      is over the newest key-value store. If the revision has been compacted, ErrCompacted is returned as a response.
             * @return http request
             */
            util::network::http_request::ptr_t create_request_kv_get(const std::string &key, const std::string &range_end = "", int64_t limit = 0,
                                                                     int64_t revision = 0);

            /**
             * @brief               create request for set key-value data
             * @param key	        key is the key, in bytes, to put into the key-value store.
             * @param value	        value is the value, in bytes, to associate with the key in the key-value store.
             * @param assign_lease	if add lease ID to associate with the key in the key-value store. A lease value of 0 indicates no lease.
             * @param prev_kv	    If prev_kv is set, etcd gets the previous key-value pair before changing it. The previous key-value pair will be returned in
             *                      the put response.
             * @param ignore_value	If ignore_value is set, etcd updates the key using its current value. Returns an error if the key does not exist.
             * @param ignore_lease	If ignore_lease is set, etcd updates the key using its current lease. Returns an error if the key does not exist.
             * @return http request
             */
            util::network::http_request::ptr_t create_request_kv_set(const std::string &key, const std::string &value, bool assign_lease = false,
                                                                     bool prev_kv = false, bool ignore_value = false, bool ignore_lease = false);

            /**
             * @brief               create request for range delete key-value data
             * @param key	        key is the first key for the range. If range_end is not given, the request only looks up key.
             * @param range_end	    range_end is the upper bound on the requested range [key, range_end). just like etcd_packer::pack_key_range
             * @param prev_kv	    If prev_kv is set, etcd gets the previous key-value pairs before deleting it. The previous key-value pairs will be
             *                      returned in the delete response.
             * @return http request
             */
            util::network::http_request::ptr_t create_request_kv_del(const std::string &key, const std::string &range_end = "", bool prev_kv = false);

            /**
             * @brief                   create request for watch
             * @param key	            key is the first key for the range. If range_end is not given, the request only looks up key.
             * @param range_end	        range_end is the upper bound on the requested range [key, range_end). just like etcd_packer::pack_key_range
             * @param start_revision	start_revision is an optional revision to watch from (inclusive). No start_revision or 0 is "now".
             * @param prev_kv	        If prev_kv is set, created watcher gets the previous KV before the event happens. If the previous KV is already
             *                          compacted, nothing will be returned.
             * @param progress_notify   progress_notify is set so that the etcd server will periodically send a WatchResponse with no events to the new watcher
             *                          if there are no recent events. It is useful when clients wish to recover a disconnected watcher starting from a recent
             *                          known revision. The etcd server may decide how often it will send notifications based on current load.
             * @return http request
             */
            util::network::http_request::ptr_t create_request_watch(const std::string &key, const std::string &range_end = "", int64_t start_revision = 0,
                                                                    bool prev_kv = false, bool progress_notify = true);

        private:
            void set_lease(int64_t v, bool force_active_keepalives);
            inline int64_t get_lease() const { return conf_.lease; }

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
            std::vector<std::shared_ptr<etcd_keepalive> > keepalive_actors_;
            std::vector<std::shared_ptr<etcd_keepalive> > keepalive_retry_actors_;
            std::vector<std::shared_ptr<etcd_watcher> > watcher_actors_;
        };
    } // namespace component
} // namespace atframe

#endif