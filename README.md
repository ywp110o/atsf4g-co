# atsf4g-co
service framework for game server using libatbus, libatapp, libcopp and etc.


# Dependency
1. [libuv](http://libuv.org/)  -- libuv is a multi-platform support library with a focus on asynchronous I/O.
2. [msgpack-c](https://github.com/msgpack/msgpack-c)  -- [MessagePack](http://msgpack.org/) is an efficient binary serialization format, and [msgpack-c](https://github.com/msgpack/msgpack-c) is a c and c++ driver for it.
3. [libiniloader](https://github.com/owt5008137/libiniloader) -- a small and lightweight ini loader code.
4. [libcurl](https://curl.haxx.se/libcurl/) -- libcurl is a free and easy-to-use client-side URL transfer library
5. [libcopp](https://github.com/owt5008137/libcopp) -- Cross-platform coroutine library written in c++.
6. [rapidjson](https://github.com/miloyip/rapidjson) -- A fast and header only json library.
7. [flatbuffer](https://github.com/google/flatbuffers) -- A simple pack/unpack library. It's used in atgateway's inner protocol.


# Prepare
1. Install [etcd](https://github.com/coreos/etcd). (It's used for atproxy to connect to each other.)
