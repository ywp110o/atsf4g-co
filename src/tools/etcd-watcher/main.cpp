#include <cstdio>
#include <cstdlib>

#include <time/time_utility.h>

#include <log/log_wrapper.h>

#include <uv.h>

#include <etcdcli/etcd_cluster.h>

static bool is_run = true;
static int wait_for_close = 0;
static void tick_timer_callback(uv_timer_t *handle) {
    util::time::time_utility::update();

    atframe::component::etcd_cluster *ec = reinterpret_cast<atframe::component::etcd_cluster *>(handle->data);
    ec->tick();
}

static void signal_callback(uv_signal_t *handle, int signum) {
    is_run = false;
    uv_stop(handle->loop);
}

static void close_callback(uv_handle_t *handle) { --wait_for_close; }

static void log_callback(const util::log::log_wrapper::caller_info_t &caller, const char *content, size_t content_size) { printf("%s\n", content); }

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: %s <init host> <watch path> <keepalive path> <keepalive value>\n", argv[0]);
        printf("  Example: %s http://127.0.0.1:2379 /atapp/proxy/services /atapp/proxy/services/123456 \"{}\"\n", argv[0]);
        return 0;
    }

    util::time::time_utility::update();
    WLOG_GETCAT(util::log::log_wrapper::categorize_t::DEFAULT)->init();
    WLOG_GETCAT(util::log::log_wrapper::categorize_t::DEFAULT)->add_sink(log_callback);

    util::network::http_request::curl_m_bind_ptr_t curl_mgr;
    util::network::http_request::create_curl_multi(uv_default_loop(), curl_mgr);
    atframe::component::etcd_cluster ec;
    ec.init(curl_mgr);
    std::vector<std::string> hosts;
    hosts.push_back(argv[1]);
    ec.set_hosts(hosts);

    if (argc > 3) {
        ec.set_flag(atframe::component::etcd_cluster::flag_t::ENABLE_LEASE, true);
    }

    uv_timer_t tick_timer;
    uv_timer_init(uv_default_loop(), &tick_timer);
    tick_timer.data = &ec;
    uv_timer_start(&tick_timer, tick_timer_callback, 1000, 1000);

    uv_signal_t sig;
    uv_signal_init(uv_default_loop(), &sig);
    uv_signal_start(&sig, signal_callback, SIGINT);

    while (is_run) {
        uv_run(uv_default_loop(), UV_RUN_DEFAULT);
    }

    wait_for_close = 2;

    uv_timer_stop(&tick_timer);
    uv_close((uv_handle_t *)&tick_timer, close_callback);

    uv_signal_stop(&sig);
    uv_close((uv_handle_t *)&sig, close_callback);

    util::network::http_request::destroy_curl_multi(curl_mgr);

    while (wait_for_close > 0) {
        uv_run(uv_default_loop(), UV_RUN_ONCE);
    }

    return 0;
}