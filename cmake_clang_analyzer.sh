#!/bin/sh

SYS_NAME="clang_analyzer";

BUILD_DIR="build_$SYS_NAME";

SCRIPT_DIR="$(dirname $0)";
mkdir -p "$SCRIPT_DIR/$BUILD_DIR";
cd "$SCRIPT_DIR/$BUILD_DIR";
if [ -e "/c/workspace" ]; then
    cmake .. -DCMAKE_C_COMPILER=$(which ccc-analyzer) -DCMAKE_CXX_COMPILER=$(which c++-analyzer) -DPROJECT_ENABLE_UNITTEST=ON -DPROJECT_ENABLE_SAMPLE=ON -DRAPIDJSON_ROOT=$SCRIPT_DIR/3rd_party/rapidjson/repo -DLIBCURL_ROOT=/c/workspace/lib/network/prebuilt/mingw64 -DLIBUV_ROOT=/c/workspace/lib/network/prebuilt/mingw64 -DMSGPACK_ROOT=/c/workspace/lib/protocol/msgpack/prebuilt "$@";
else
    cmake .. -DCMAKE_C_COMPILER=$(which ccc-analyzer) -DCMAKE_CXX_COMPILER=$(which c++-analyzer) -DPROJECT_ENABLE_UNITTEST=ON -DPROJECT_ENABLE_SAMPLE=ON -DRAPIDJSON_ROOT=$SCRIPT_DIR/3rd_party/rapidjson/repo "$@";
fi

echo "Run cd '$SCRIPT_DIR/$BUILD_DIR' && scan-build -o report --html-title='atsf4g-co static analysis' make -j4";