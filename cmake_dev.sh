#!/bin/bash

SYS_NAME="$(uname -s)";
SYS_NAME="$(basename $SYS_NAME)";
CC=gcc;
CXX=g++;

CMAKE_OPTIONS="";
CMAKE_CLANG_TIDY="";

while getopts "c:hm:o:tus-" OPTION; do
    case $OPTION in
        c)
            CC="$OPTARG";
            CXX="${CC/clang/clang++}";
            CXX="${CXX/gcc/g++}";
        ;;
        h)
            echo "usage: $0 [options] [-- [cmake options...] ]";
            echo "options:";
            echo "-c <compiler>               compiler toolchains(gcc, clang or others).";
            echo "-h                          help message.";
            echo "-m [mbedtls root]           set root of mbedtls.";
            echo "-o [openssl root]           set root of openssl.";
            echo "-t                          enable clang-tidy.";
            echo "-u                          enable unit test.";
            echo "-s                          enable sample.";
            exit 0;
        ;;
        m)
            if [ -z "$OPTARG" ]; then
                CMAKE_OPTIONS="$CMAKE_OPTIONS -DMSGPACK_ROOT=$OPTARG";
            else
                CMAKE_OPTIONS="$CMAKE_OPTIONS -DMSGPACK_ROOT=c:/workspace/lib/crypt/prebuilt/win64";
            fi
        ;;
        o)
            if [ -z "$OPTARG" ]; then
                CMAKE_OPTIONS="$CMAKE_OPTIONS -DOPENSSL_ROOT_DIR=$OPTARG";
            else
                CMAKE_OPTIONS="$CMAKE_OPTIONS -DOPENSSL_ROOT_DIR=c:/workspace/lib/crypt/prebuilt/openssl-1.0.2h-vs2015";
            fi
        ;;
        t)
            CMAKE_CLANG_TIDY="-D -checks=* --";
        ;;
        u)
            CMAKE_OPTIONS="$CMAKE_OPTIONS -DPROJECT_ENABLE_UNITTEST=ON";
        ;;
        s)
            CMAKE_OPTIONS="$CMAKE_OPTIONS -DPROJECT_ENABLE_SAMPLE=ON";
        ;;
        -)
            break;
        ;;
        ?)  #当有不认识的选项的时候arg为?
            echo "unkonw argument detected";
            exit 1;
        ;;
    esac
done

shift $(($OPTIND - 1));

BUILD_DIR=$(echo "build_$SYS_NAME" | tr '[:upper:]' '[:lower:]');
SCRIPT_DIR="$(dirname $0)";
mkdir -p "$SCRIPT_DIR/$BUILD_DIR";
cd "$SCRIPT_DIR/$BUILD_DIR";

CMAKE_OPTIONS="$CMAKE_OPTIONS -DCMAKE_C_COMPILER=$CC -DCMAKE_CXX_COMPILER=$CXX";

CHECK_MSYS=$(echo ${MSYSTEM:0:5} | tr '[:upper:]' '[:lower:]');
if [ "mingw" == "$CHECK_MSYS" ]; then
    cmake .. -G "MSYS Makefiles" -DRAPIDJSON_ROOT=$SCRIPT_DIR/3rd_party/rapidjson/repo $CMAKE_OPTIONS "$@";
else
    cmake .. -DRAPIDJSON_ROOT=$SCRIPT_DIR/3rd_party/rapidjson/repo $CMAKE_OPTIONS "$@";
fi
