
# =========== 3rdparty openssl ==================
if(NOT 3RD_PARTY_OPENSSL_BASE_DIR)
    set (3RD_PARTY_OPENSSL_BASE_DIR ${CMAKE_CURRENT_LIST_DIR})
endif()

set (3RD_PARTY_OPENSSL_REPO_DIR "${3RD_PARTY_OPENSSL_BASE_DIR}/repo")

find_package(OpenSSL)
if(OPENSSL_FOUND)
    set (3RD_PARTY_OPENSSL_INC_DIR ${OPENSSL_INCLUDE_DIR})
    include_directories(${3RD_PARTY_OPENSSL_INC_DIR})
else()
    EchoWithColor(COLOR Yellow "-- Dependency: openssl not found")
endif()
