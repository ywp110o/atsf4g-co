#include <cstdio>
#include <cstdlib>

#include "uv.h"

#include <config/atframe_services_build_feature.h>

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s <ip> <port> [dhparam] [mode]\n\tmode can be tick,busy,benchmark,reconnect", argv[0]);
        return -1;
    }

    // const char *mode = "tick";
    // const char *ip = argv[1];
    // long port = strtol(argv[2], NULL, 10);
    // const char *dhparam_file = NULL;
    // if (argc > 3) {
    //     dhparam_file = argv[3];
    // }

    // if (argc > 4) {
    //     mode = argv[4];
    // }

    return 0;
}