#!/bin/bash

###########################################################################
#  Change values here
#
SDKVERSION=$(xcrun -sdk iphoneos --show-sdk-version);
#
###########################################################################
#
# Don't change anything here
WORKING_DIR="$PWD";
ARCHS="i386 x86_64 armv7 armv7s arm64";
DEVELOPER=$(xcode-select -print-path);

if [ $# -lt 1 ]; then
    echo "Usage: $0 <mbedtle-* dir name>";
    exit -1;  
fi

SRC_DIR="$(cd "$1" && pwd)";

##########
if [ ! -e "$SRC_DIR/CMakeLists.txt" ]; then
    echo "$SRC_DIR/CMakeLists.txt not found";
    exit -2;
fi
shift;

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
    
    export DEVROOT="${DEVELOPER}/Platforms/${PLATFORM}.platform/Developer"
    export SDKROOT="${DEVROOT}/SDKs/${PLATFORM}${SDKVERSION}.sdk"
    export BUILD_TOOLS="${DEVELOPER}"
    if [ -e "$WORKING_DIR/build-$ARCH" ]; then
        rm -rf "$WORKING_DIR/build-$ARCH";
    fi
    mkdir -p "$WORKING_DIR/build-$ARCH";
    cd "$WORKING_DIR/build-$ARCH";
    
    cmake "$SRC_DIR" -DCMAKE_OSX_SYSROOT=$SDKROOT -DCMAKE_SYSROOT=$SDKROOT -DCMAKE_OSX_ARCHITECTURES=$ARCH "$@";
    make -j4;
done

cd "$WORKING_DIR";
echo "Linking and packaging library...";

for LIB_NAME in "libatgw_inner_v1_c"; do
    lipo -create $(find "$WORKING_DIR/build-"* -name $LIB_NAME.a) -output "$WORKING_DIR/lib/$LIB_NAME.a";
done

echo "Building done.";
