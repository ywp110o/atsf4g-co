#!/bin/bash

###########################################################################
#  Change values here
#
SDKVERSION=$(xcrun -sdk iphoneos --show-sdk-version);
#
###########################################################################
#
# Don't change anything here
WORKING_DIR="$(pwd)";

ARCHS="i386 x86_64 armv7 armv7s arm64";
DEVELOPER_ROOT=$(xcode-select -print-path);
SRC_DIR="$PWD";
MBEDTLS_ROOT="" ;
OPENSSL_ROOT="" ;
BUILD_TYPE="RelWithDebInfo" ;
OTHER_CFLAGS="-fPIC" ;

# ======================= options ======================= 
while getopts "a:b:d:hi:m:o:r:s:-" OPTION; do
    case $OPTION in
        a)
            ARCHS="$OPTARG";
        ;;
        b)
            BUILD_TYPE="$OPTARG";
        ;;
        d)
            DEVELOPER_ROOT="$OPTARG";
        ;;
        h)
            echo "usage: $0 [options] -r SOURCE_DIR [-- [cmake options]]";
            echo "options:";
            echo "-a [archs]                    which arch need to built, multiple values must be split by space(default: $ARCHS)";
            echo "-b [build type]               build type(default: $BUILD_TYPE, available: Debug, Release, RelWithDebInfo, MinSizeRel)";
            echo "-d [developer root directory] developer root directory, we use xcode-select -print-path to find default value.(default: $DEVELOPER_ROOT)";
            echo "-h                            help message.";
            echo "-i [option]                   enable bitcode support(available: off, all, bitcode, marker)";
            echo "-s [sdk version]              sdk version, we use xcrun -sdk iphoneos --show-sdk-version to find default value.(default: $SDKVERSION)";
            echo "-r [source dir]               root directory of this library";
            echo "-o [openssl root directory]   openssl root directory, which has [$ARCHS]/include and [$ARCHS]/lib";
            echo "-m [mbedtls root directory]   mbedtls root directory, which has [$ARCHS]/include and [$ARCHS]/lib";
            exit 0;
        ;;
        i)
            if [ ! -z "$OPTARG" ]; then
                OTHER_CFLAGS="$OTHER_CFLAGS -fembed-bitcode=$OPTARG";
            else
                OTHER_CFLAGS="$OTHER_CFLAGS -fembed-bitcode";
            fi
        ;;
        o)
            OPENSSL_ROOT="$OPTARG";
        ;;
        m)
            MBEDTLS_ROOT="$OPTARG";
        ;;
        r)
            SRC_DIR="$OPTARG";
        ;;
        s)
            SDKVERSION="$OPTARG";
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

echo "Ready to build for ios";
echo "WORKING_DIR=${WORKING_DIR}";
echo "ARCHS=${ARCHS}";
echo "DEVELOPER_ROOT=${DEVELOPER_ROOT}";
echo "SDKVERSION=${SDKVERSION}";
echo "cmake options=$@";
echo "SOURCE=$SRC_DIR";

##########
if [ ! -e "$SRC_DIR/CMakeLists.txt" ]; then
    echo "$SRC_DIR/CMakeLists.txt not found";
    exit -2;
fi

SRC_DIR="$(cd $SRC_DIR && pwd)";
mkdir -p "$WORKING_DIR/lib";

for ARCH in ${ARCHS}; do
    echo "================== Compling $ARCH ==================";
    if [[ "${ARCH}" == "i386" || "${ARCH}" == "x86_64" ]]; then
        PLATFORM="iPhoneSimulator"
    else
        PLATFORM="iPhoneOS"
    fi

    echo "Building for ${PLATFORM} ${SDKVERSION} ${ARCH}"
    echo "Please stand by..."
    
    export DEVROOT="${DEVELOPER_ROOT}/Platforms/${PLATFORM}.platform/Developer"
    export SDKROOT="${DEVROOT}/SDKs/${PLATFORM}${SDKVERSION}.sdk"
    export BUILD_TOOLS="${DEVELOPER_ROOT}"
    if [ -e "$WORKING_DIR/build-$ARCH" ]; then
        rm -rf "$WORKING_DIR/build-$ARCH";
    fi
    mkdir -p "$WORKING_DIR/build-$ARCH";
    cd "$WORKING_DIR/build-$ARCH";
    
    EXT_OPTIONS="";
    if [ ! -z "$OPENSSL_ROOT" ] && [ -e "$OPENSSL_ROOT" ]; then
        EXT_OPTIONS="$EXT_OPTIONS -DCRYPTO_USE_OPENSSL=YES -DOPENSSL_ROOT_DIR=$OPENSSL_ROOT/$ARCH";
    fi
    if [ ! -z "$MBEDTLS_ROOT" ] && [ -e "$MBEDTLS_ROOT" ]; then
        if [ -e "$MBEDTLS_ROOT/$ARCH" ]; then
            EXT_OPTIONS="$EXT_OPTIONS -DCRYPTO_USE_MBEDTLS=YES -DMBEDTLS_ROOT=$MBEDTLS_ROOT/$ARCH";
        else
            EXT_OPTIONS="$EXT_OPTIONS -DCRYPTO_USE_MBEDTLS=YES -DMBEDTLS_ROOT=$MBEDTLS_ROOT";
        fi
    fi

    # add -DCMAKE_OSX_DEPLOYMENT_TARGET=7.1 to specify the min SDK version
    cmake "$SRC_DIR" -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DCMAKE_OSX_SYSROOT=$SDKROOT -DCMAKE_SYSROOT=$SDKROOT -DCMAKE_OSX_ARCHITECTURES=$ARCH -DCMAKE_CXX_FLAGS="$OTHER_CFLAGS" -DCMAKE_C_FLAGS="$OTHER_CFLAGS" $EXT_OPTIONS "$@";
    cmake --build . ;
done

cd "$WORKING_DIR";
echo "Linking and packaging library...";

for LIB_NAME in "libatgw_inner_v1_c"; do
    lipo -create $(find "$WORKING_DIR/build-"* -name $LIB_NAME.a) -output "$WORKING_DIR/lib/$LIB_NAME.a";
done

echo "Copying include files...";

if [ "$WORKING_DIR" != "$SRC_DIR" ]; then
    if [ -e "$WORKING_DIR/include" ]; then
        rm -rf "$WORKING_DIR/include";
    fi  
    
    cp -rf "$SRC_DIR/include" "$WORKING_DIR/include";
fi  

echo "Building done.";
