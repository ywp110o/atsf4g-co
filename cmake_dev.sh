#!/bin/sh

SYS_NAME="$(uname -s)";
SYS_NAME="$(basename $SYS_NAME)";

BUILD_DIR="build_$SYS_NAME";

cd "$(dirname $0)";
mkdir -p "$BUILD_DIR";
cd "$BUILD_DIR";

cmake .. -DPROJECT_ENABLE_UNITTEST=ON -DPROJECT_ENABLE_SAMPLE=ON -DLIBUV_ROOT=/c/workspace/lib/network/prebuilt/mingw64 -DMSGPACK_ROOT=/c/workspace/lib/protocol/msgpack/prebuilt "$@";