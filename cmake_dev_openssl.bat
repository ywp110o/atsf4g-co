mkdir build_win64_msvc_openssl
cd build_win64_msvc_openssl
:: openssl : see https://slproweb.com/products/Win32OpenSSL.html
:: libcurl : 
::     "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64
::     cd winbuild 
::     nmake /f Makefile.vc mode=static VC=15 MACHINE=x64
cmake .. -G "Visual Studio 15 2017 Win64" -DLIBUV_ROOT=C:\workspace\lib\network\prebuilt\win64 -DMSGPACK_ROOT=C:\workspace\lib\protocol\msgpack\prebuilt -DLIBCURL_ROOT=C:\workspace\lib\network\prebuilt\win64 -DOPENSSL_ROOT_DIR=C:\workspace\lib\crypt\prebuilt\openssl -DPROJECT_ENABLE_SAMPLE=ON -DPROJECT_ENABLE_UNITTEST=ON  -DPROJECT_ENABLE_TOOLS=ON
