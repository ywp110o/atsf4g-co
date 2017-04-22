mkdir build_win64_msvc_openssl
cd build_win64_msvc_openssl
cmake .. -G "Visual Studio 15 2017 Win64" -DLIBUV_ROOT=C:\workspace\lib\network\prebuilt\win64 -DMSGPACK_ROOT=C:\workspace\lib\protocol\msgpack\prebuilt -DLIBCURL_ROOT=C:\workspace\lib\network\prebuilt\win64 -DOPENSSL_ROOT_DIR=C:\workspace\lib\crypt\prebuilt\openssl-1.0.2h-vs2015 -DPROJECT_ENABLE_SAMPLE=ON -DPROJECT_ENABLE_UNITTEST=ON  -DPROJECT_ENABLE_TOOLS=ON
