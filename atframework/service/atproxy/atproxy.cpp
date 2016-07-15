
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <vector>

#include <uv.h>

#include <atframe/atapp.h>
#include <time/time_utility.h>

#include "atproxy_manager.h"
#include "etcd_v2_module.h"

static int app_handle_on_send_fail(atapp::app &app, atapp::app::app_id_t src_pd, atapp::app::app_id_t dst_pd,
    const atbus::protocol::msg &m) {
    WLOGERROR("send data from %llx to %llx failed", src_pd, dst_pd);
    return 0;
}

static int app_handle_on_connected(atapp::app &app, atbus::endpoint &ep, int status) {
    WLOGINFO("node %llx connected, status: %d", ep.get_id(), status);
    return 0;
}

static int app_handle_on_disconnected(atapp::app &app, atbus::endpoint &ep, int status) {
    WLOGINFO("node %llx disconnected, status: %d", ep.get_id(), status);
    return 0;
}

int main(int argc, char *argv[]) {
    atapp::app app;

    // setup module
    app.add_module(std::make_shared<atframe::proxy::etcd_v2_module>());

    // setup message handle
    app.set_evt_on_send_fail(app_handle_on_send_fail);
    app.set_evt_on_app_connected(app_handle_on_connected);
    app.set_evt_on_app_disconnected(app_handle_on_disconnected);

    // run
    return app.run(uv_default_loop(), argc, (const char **)argv, NULL);
}
