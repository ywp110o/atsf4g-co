mkdir build_win64_msvc_mbedtls
cd build_win64_msvc_mbedtls
:: 
:: libcurl : 
::     "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64
::     cd winbuild 
::     nmake /f Makefile.vc mode=static VC=15 MACHINE=x64
::     copy all files and rename libcurl_a.lib to libcurl.lib
cmake .. -G "Visual Studio 15 2017 Win64" -DCMAKE_BUILD_TYPE=Debug -DLIBUV_ROOT=C:\workspace\lib\network\prebuilt\win64 -DMSGPACK_ROOT=C:\workspace\lib\protocol\msgpack\prebuilt -DLIBCURL_ROOT=C:\workspace\lib\network\prebuilt\win64 -DMBEDTLS_ROOT=C:\workspace\lib\crypt\prebuilt\win64 -DPROJECT_ENABLE_SAMPLE=ON -DPROJECT_ENABLE_UNITTEST=ON -DPROJECT_ENABLE_TOOLS=ON