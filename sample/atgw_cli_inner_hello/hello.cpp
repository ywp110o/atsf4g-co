#include <cstdio>
#include <cstdlib>

#include "uv.h"

#include <config/atframe_services_build_feature.h>

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s <ip> <port> <dhparam>", argv[0]);
        return -1;
    }

    const char *ip = argv[1];
    long port = strtol(argv[2], NULL, 10);
    const char *dhparam_file = NULL;
    if (argc > 3) {
        dhparam_file = argv[3];
    }

    return 0;
}