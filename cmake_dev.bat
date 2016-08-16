mkdir build
cd build
cmake .. -G "Visual Studio 14 2015 Win64" -DLIBUV_ROOT=C:\workspace\lib\network\libuv-win32 -DMSGPACK_ROOT=C:\workspace\lib\protocol\msgpack\prebuilt -DLIBCURL_ROOT=C:\workspace\lib\network\prebuilt\msvc -DOPENSSL_ROOT_DIR=C:\workspace\lib\crypto\prebuilt\openssl-1.0.2h-vs2015 -DPROJECT_ENABLE_SAMPLE=ON -DPROJECT_ENABLE_UNITTEST=ON