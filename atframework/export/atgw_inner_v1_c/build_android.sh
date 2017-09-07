#!/bin/bash

#
###########################################################################
#
# Don't change anything here
WORKING_DIR="$PWD";

ARCHS="x86 x86_64 armeabi armeabi-v7a arm64-v8a";
NDK_ROOT=$NDK_ROOT;
SOURCE_DIR="$PWD";
ANDROID_NATIVE_API_LEVEL=16 ;
ANDROID_TOOLCHAIN=clang ;
ANDROID_STL=c++_static ; #
MBEDTLS_ROOT="" ;
OPENSSL_ROOT="" ;
BUILD_TYPE="RelWithDebInfo" ;

# ======================= options ======================= 
while getopts "a:b:c:n:hl:m:o:r:t:-" OPTION; do
    case $OPTION in
        a)
            ARCHS="$OPTARG";
        ;;
        b)
            BUILD_TYPE="$OPTARG";
        ;;
        c)
            ANDROID_STL="$OPTARG";
        ;;
        n)
            NDK_ROOT="$OPTARG";
        ;;
        h)
            echo "usage: $0 [options] -n NDK_ROOT -r SOURCE_DIR [-- [cmake options]]";
            echo "options:";
            echo "-a [archs]                    which arch need to built, multiple values must be split by space(default: $ARCHS)";
            echo "-b [build type]               build type(default: $BUILD_TYPE, available: Debug, Release, RelWithDebInfo, MinSizeRel)";
            echo "-c [android stl]              stl used by ndk(default: $ANDROID_STL, available: system, stlport_static, stlport_shared, gnustl_static, gnustl_shared, c++_static, c++_shared, none)";
            echo "-n [ndk root directory]       ndk root directory.(default: $DEVELOPER_ROOT)";
            echo "-l [api level]                API level, see $NDK_ROOT/platforms for detail.(default: $ANDROID_NATIVE_API_LEVEL)";
            echo "-r [source dir]               root directory of this library";
            echo "-t [toolchain]                ANDROID_TOOLCHAIN.(gcc version/clang, default: $ANDROID_TOOLCHAIN, @see CMAKE_ANDROID_NDK_TOOLCHAIN_VERSION in cmake)";
            echo "-o [openssl root directory]   openssl root directory, which has [$ARCHS]/include and [$ARCHS]/lib";
            echo "-m [mbedtls root directory]   mbedtls root directory, which has [$ARCHS]/include and [$ARCHS]/lib";
            echo "-h                            help message.";
            exit 0;
        ;;
        r)
            SOURCE_DIR="$OPTARG";
        ;;
        t)
            ANDROID_TOOLCHAIN="$OPTARG";
        ;;
        l)
            ANDROID_NATIVE_API_LEVEL=$OPTARG;
        ;;
        o)
            OPENSSL_ROOT="$OPTARG";
        ;;
        m)
            MBEDTLS_ROOT="$OPTARG";
        ;;
        -) 
            break;
            break;
        ;;
        ?)  #当有不认识的选项的时候arg为?
            echo "unkonw argument detected";
            exit 1;
        ;;
    esac
done

shift $(($OPTIND-1));

##########
if [ ! -e "$SOURCE_DIR/CMakeLists.txt" ]; then
    echo "$SOURCE_DIR/CMakeLists.txt not found";
    exit -2;
fi
SOURCE_DIR="$(cd "$SOURCE_DIR" && pwd)";

mkdir -p "$WORKING_DIR/lib";

CMAKE_ANDROID_NDK_TOOLCHAIN_VERSION=$ANDROID_TOOLCHAIN;
if [ "${ANDROID_TOOLCHAIN:0:5}" != "clang" ]; then
    ANDROID_TOOLCHAIN="gcc";
fi

for ARCH in ${ARCHS}; do
    echo "================== Compling $ARCH ==================";
    echo "Building mbedtls for android-$ANDROID_NATIVE_API_LEVEL ${ARCH}"
    
    # sed -i.bak '4d' Makefile;
    echo "Please stand by..."
    if [ -e "$WORKING_DIR/build/$ARCH" ]; then
        rm -rf "$WORKING_DIR/build/$ARCH";
    fi
    mkdir -p "$WORKING_DIR/build/$ARCH";
    cd "$WORKING_DIR/build/$ARCH";
    
    mkdir -p "$WORKING_DIR/lib/$ARCH";

    EXT_OPTIONS="";
    if [ ! -z "$OPENSSL_ROOT" ] && [ -e "$OPENSSL_ROOT" ]; then
        EXT_OPTIONS="$EXT_OPTIONS -DCRYPTO_USE_OPENSSL=YES -DOPENSSL_ROOT_DIR=$OPENSSL_ROOT/$ARCH";
    fi
    if [ ! -z "$MBEDTLS_ROOT" ] && [ -e "$MBEDTLS_ROOT" ]; then
        EXT_OPTIONS="$EXT_OPTIONS -DCRYPTO_USE_MBEDTLS=YES -DMBEDTLS_ROOT=$MBEDTLS_ROOT/$ARCH";
    fi

    # 64 bits must at least using android-21
    # @see $NDK_ROOT/build/cmake/android.toolchain.cmake
    echo $ARCH | grep -E '64(-v8a)?$' ;
    if [ $? -eq 0 ] && [ $ANDROID_NATIVE_API_LEVEL -lt 21 ]; then
        ANDROID_NATIVE_API_LEVEL=21 ;
    fi

    # add -DCMAKE_OSX_DEPLOYMENT_TARGET=7.1 to specify the min SDK version
    cmake "$SOURCE_DIR" -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
        -DCMAKE_LIBRARY_OUTPUT_DIRECTORY="$WORKING_DIR/lib/$ARCH" -DCMAKE_ARCHIVE_OUTPUT_DIRECTORY="$WORKING_DIR/lib/$ARCH" \
        -DCMAKE_TOOLCHAIN_FILE="$NDK_ROOT/build/cmake/android.toolchain.cmake" \
        -DANDROID_NDK="$NDK_ROOT" -DCMAKE_ANDROID_NDK="$NDK_ROOT" \
        -DANDROID_NATIVE_API_LEVEL=$ANDROID_NATIVE_API_LEVEL -DCMAKE_ANDROID_API=$ANDROID_NATIVE_API_LEVEL \
        -DANDROID_TOOLCHAIN=$ANDROID_TOOLCHAIN -DCMAKE_ANDROID_NDK_TOOLCHAIN_VERSION=$CMAKE_ANDROID_NDK_TOOLCHAIN_VERSION \
        -DANDROID_ABI=$ARCH -DCMAKE_ANDROID_ARCH_ABI=$ARCH \
        -DANDROID_STL=$ANDROID_STL -DCMAKE_ANDROID_STL_TYPE=$ANDROID_STL \
        -DANDROID_PIE=YES $EXT_OPTIONS "$@";

    cmake --build . ;
done

cd "$WORKING_DIR";
echo "Copying include files...";

if [ "$WORKING_DIR" != "$SOURCE_DIR" ]; then
    if [ -e "$WORKING_DIR/include" ]; then
        rm -rf "$WORKING_DIR/include";
    fi

    cp -rf "$SOURCE_DIR/include" "$WORKING_DIR/include";
fi
echo "Building done.";
