# =========== 3rd_party ===========
unset (PROJECT_3RD_PARTY_SRC_LIST)

# =========== 3rd_party - msgpack ===========
include("${PROJECT_3RD_PARTY_ROOT_DIR}/msgpack/msgpack.cmake")

# =========== 3rd_party - libuv ===========
include("${PROJECT_3RD_PARTY_ROOT_DIR}/libuv/libuv.cmake")

# =========== 3rd_party - libiniloader ===========
include("${PROJECT_3RD_PARTY_ROOT_DIR}/libiniloader/libiniloader.cmake")

# =========== 3rd_party - libcurl ===========
include("${PROJECT_3RD_PARTY_ROOT_DIR}/libcurl/libcurl.cmake")

# =========== 3rd_party - rapidjson ===========
include("${PROJECT_3RD_PARTY_ROOT_DIR}/rapidjson/rapidjson.cmake")