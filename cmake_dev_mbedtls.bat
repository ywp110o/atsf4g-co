mkdir build_mbedtls
cd build_mbedtls

cmake .. -G "Visual Studio 14 2015 Win64" -DLIBUV_ROOT=C:\workspace\lib\network\libuv-win32 -DMSGPACK_ROOT=C:\workspace\lib\protocol\msgpack\prebuilt -DLIBCURL_ROOT=C:\workspace\lib\network\prebuilt\msvc -DMBEDTLS_ROOT=C:\workspace\lib\crypto\prebuilt\mbedtls-2.3.0_x86_64 -DPROJECT_ENABLE_SAMPLE=ON -DPROJECT_ENABLE_UNITTEST=ON