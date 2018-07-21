#include <algorithm>

#include "atproxy_manager.h"
#include <time/time_utility.h>


namespace atframe {
    namespace proxy {
        int atproxy_manager::tick(const ::atapp::app &app) {
            time_t now = util::time::time_utility::get_now();

            int ret = 0;
            do {
                if (check_list_.empty()) {
                    break;
                }

                check_info_t ci = check_list_.front();
                if (now <= ci.timeout_sec) {
                    break;
                }
                check_list_.pop_front();

                // skip self
                if (ci.proxy_id == app.get_id()) {
                    continue;
                }

                std::map< ::atapp::app::app_id_t, node_info_t>::iterator iter = proxy_set_.find(ci.proxy_id);
                // already removed, skip
                if (iter == proxy_set_.end()) {
                    continue;
                }

                // if has no listen addrs, skip
                if (iter->second.listens.empty()) {
                    continue;
                }

                // has another pending check info
                if (iter->second.next_action_time > ci.timeout_sec) {
                    continue;
                }

                if (app.get_bus_node()) {
                    // set next_action_time first
                    iter->second.next_action_time = 0;

                    // already connected, skip
                    if (NULL != app.get_bus_node()->get_endpoint(ci.proxy_id)) {
                        continue;
                    }

                    // try to connect to brother proxy
                    int res = app.get_bus_node()->connect(iter->second.listens.front().c_str());
                    if (res >= 0) {
                        ++ret;
                    } else {
                        WLOGERROR("try to connect to proxy: %llx, address: %s failed, res: %d", static_cast<unsigned long long>(iter->second.id),
                                  iter->second.listens.front().c_str(), res);
                    }

                    // recheck some time later
                    ci.timeout_sec = now + app.get_bus_node()->get_conf().retry_interval;
                    if (ci.timeout_sec <= now) {
                        ci.timeout_sec = now + 1;
                    }
                    // try to reconnect later
                    iter->second.next_action_time = ci.timeout_sec;
                    check_list_.push_back(ci);

                    // if failed and there is more than one listen address, use next address next time.
                    if (iter->second.listens.size() == 2) {
                        iter->second.listens.front().swap(iter->second.listens.back());
                    } else if (iter->second.listens.size() > 2) {
                        iter->second.listens.push_back(iter->second.listens.front());
                        iter->second.listens.pop_front();
                    }
                } else {
                    ci.timeout_sec = now + 1;
                    // try to reconnect later
                    iter->second.next_action_time = ci.timeout_sec;
                    check_list_.push_back(ci);
                }

            } while (true);

            return ret;
        }

        int atproxy_manager::set(node_info_t &proxy_info) {
            check_info_t ci;
            ci.timeout_sec = util::time::time_utility::get_now();
            ci.proxy_id = proxy_info.id;

            proxy_set_t::iterator iter = proxy_set_.find(proxy_info.id);
            if (iter != proxy_set_.end()) {
                // already has pending action, just skipped
                if (iter->second.next_action_time >= ci.timeout_sec) {
                    return 0;
                } else {
                    iter->second.next_action_time = ci.timeout_sec;
                }
            } else {
                proxy_info.next_action_time = ci.timeout_sec;
                proxy_set_[proxy_info.id] = proxy_info;
            }

            // push front and check it on next loop
            check_list_.push_front(ci);
            return 0;
        }

        int atproxy_manager::remove(::atapp::app::app_id_t id) {
            proxy_set_.erase(id);
            return 0;
        }

        int atproxy_manager::reset(node_list_t &all_proxys) {
            proxy_set_.clear();
            check_list_.clear();

            for (std::list<node_info_t>::iterator iter = all_proxys.nodes.begin(); iter != all_proxys.nodes.end(); ++iter) {

                // skip all empty
                if (iter->listens.empty()) {
                    continue;
                }

                check_info_t ci;
                ci.timeout_sec = util::time::time_utility::get_now();
                ci.proxy_id = iter->id;
                (*iter).next_action_time = ci.timeout_sec;

                // copy proxy info
                proxy_set_[ci.proxy_id] = *iter;

                // push front and check it on next loop
                check_list_.push_front(ci);
            }

            return 0;
        }

        int atproxy_manager::on_connected(const ::atapp::app &app, ::atapp::app::app_id_t id) { return 0; }

        int atproxy_manager::on_disconnected(const ::atapp::app &app, ::atapp::app::app_id_t id) {
            proxy_set_t::iterator iter = proxy_set_.find(id);
            if (proxy_set_.end() != iter) {
                check_info_t ci;

                // when stoping bus noe may be unavailable
                if (!app.check(::atapp::app::flag_t::STOPING)) {
                    if (app.get_bus_node() && app.get_bus_node()->get_conf().retry_interval > 0) {
                        ci.timeout_sec = util::time::time_utility::get_now() + app.get_bus_node()->get_conf().retry_interval;
                    } else {
                        ci.timeout_sec = util::time::time_utility::get_now() + 1;
                    }
                } else {
                    ci.timeout_sec = util::time::time_utility::get_now() - 1;
                }

                if (iter->second.next_action_time < ci.timeout_sec) {
                    iter->second.next_action_time = ci.timeout_sec;
                    ci.proxy_id = id;
                    check_list_.push_back(ci);
                }
            }

            return 0;
        }

        void atproxy_manager::swap(node_info_t &l, node_info_t &r) {
            using std::swap;
            swap(l.action, r.action);
            swap(l.id, r.id);
            swap(l.listens, r.listens);
            swap(l.next_action_time, r.next_action_time);
        }
    } // namespace proxy
} // namespace atframe