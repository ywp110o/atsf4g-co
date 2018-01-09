/**
 * etcd_watcher.h
 *
 *  Created on: 2017-12-26
 *      Author: owent
 *
 *  Released under the MIT license
 */

#ifndef ATFRAME_SERVICE_COMPONENT_ETCDCLI_ETCD_WATCHER_H
#define ATFRAME_SERVICE_COMPONENT_ETCDCLI_ETCD_WATCHER_H

#pragma once

#include <string>
#include <vector>

#include <std/chrono.h>
#include <std/functional.h>
#include <std/smart_ptr.h>


#include <config/compiler_features.h>

#include <network/http_request.h>

#include "etcd_def.h"

namespace atframe {
    namespace component {
        class etcd_cluster;

        class etcd_watcher {
        public:
            typedef std::function<bool(const std::string &)> checker_fn_t; // the parameter will be base64 of the value
            typedef std::shared_ptr<etcd_watcher> ptr_t;

            struct event_t {
                etcd_watch_event::type evt_type;
                etcd_key_value kv;
                etcd_key_value prev_kv;
            };

            struct response_t {
                int64_t watch_id;
                bool created;
                bool canceled;
                int64_t compact_revision;
                std::vector<event_t> events;
            };

        private:
            struct constrict_helper_t {};

        public:
            etcd_watcher(etcd_cluster &owner, const std::string &path, const std::string &range_end, constrict_helper_t &helper);
            static ptr_t create(etcd_cluster &owner, const std::string &path, const std::string &range_end = "+1");

            void close();

            const std::string &get_path() const;

            void active();

            etcd_cluster &get_owner() { return *owner_; }
            const etcd_cluster &get_owner() const { return *owner_; }

            inline bool is_progress_notify_enabled() const { return rpc_.enable_progress_notify; }
            inline void set_progress_notify_enabled(bool v) { rpc_.enable_progress_notify = v; }

        private:
            void process();

        private:
            static int libcurl_callback_on_changed(util::network::http_request &req);

        private:
            etcd_cluster *owner_;
            std::string path_;
            std::string range_end_;
            typedef struct {
                util::network::http_request::ptr_t rpc_opr_;
                bool is_actived;
                bool enable_progress_notify;
                std::chrono::system_clock::time_point watcher_next_request_time;
                std::chrono::system_clock::duration retry_interval;
                std::chrono::system_clock::duration request_timeout;
            } rpc_data_t;
            rpc_data_t rpc_;
        };
    } // namespace component
} // namespace atframe

#endif