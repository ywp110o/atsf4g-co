#!/bin/sh

SYS_NAME="$(uname -s)";
SYS_NAME="$(basename $SYS_NAME)";

BUILD_DIR="build_$SYS_NAME";

SCRIPT_DIR="$(dirname $0)";
mkdir -p "$SCRIPT_DIR/$BUILD_DIR";
cd "$SCRIPT_DIR/$BUILD_DIR";

cmake .. -DPROJECT_ENABLE_UNITTEST=ON -DPROJECT_ENABLE_SAMPLE=ON -DRAPIDJSON_ROOT=$SCRIPT_DIR/3rd_party/rapidjson/repo -DLIBCURL_ROOT=/c/workspace/lib/network/prebuilt/mingw64 -DLIBUV_ROOT=/c/workspace/lib/network/prebuilt/mingw64 -DMSGPACK_ROOT=/c/workspace/lib/protocol/msgpack/prebuilt "$@";