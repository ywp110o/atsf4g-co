mkdir build_win64_msvc_mbedtls
cd build_win64_msvc_mbedtls

cmake .. -G "Visual Studio 14 2015 Win64" -DLIBUV_ROOT=C:\workspace\lib\network\prebuilt\win64 -DMSGPACK_ROOT=C:\workspace\lib\protocol\msgpack\prebuilt -DLIBCURL_ROOT=C:\workspace\lib\network\prebuilt\win64 -DMBEDTLS_ROOT=C:\workspace\lib\crypt\prebuilt\win64 -DPROJECT_ENABLE_SAMPLE=ON -DPROJECT_ENABLE_UNITTEST=ON