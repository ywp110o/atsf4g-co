
#include "atframe/atapp.h"


namespace atapp {
    app::app() {}
    app::~app() {}

    int app::run(int argc, const char *argv[], void *priv_data) {
        // step 1. bind default options

        // step 2. load options from cmd line
        cmd_opts_.start(argc, argv, false, priv_data);

        // step 3. load options from cmd line
        int ret = reload();
        if (ret < 0) {
            return ret;
        }

        // TODO step 4. setup signal

        // TODO step 5. all modules init

        // TODO step 6. all modules reload

        // TODO step 7. set running
        return ev_loop();
    }

    int app::reload() {
        // TODO step 1. reset configure
        // TODO step 2. reload from program configure file
        // TODO step 3. reload from external configure files
        // TODO step 4. merge configure
        // TODO step 5. reset log
        // TODO step 6. if inited, let all modules reload

        // TODO step 7. if running and tick interval changed, reset timer

        return 0;
    }

    int app::stop() {
        // TODO step 1. set stop flag.
        // TODO step 2. stop libuv and return from uv_run
        return 0;
    }

    int app::proc() {
        // TODO step 1. proc atbus
        // TODO step 2. proc available modules
        return 0;
    }

    int app::ev_loop() {
        // TODO set tick timer
        // TODO step X. loop uv_run util stop flag is set
        // TODO step X. notify all modules to finish and wait for all modules stop
        // TODO step X. if stop is blocked, setup stop timeout and waiting for all modules finished
        return 0;
    }
}
