
# =========== 3rdparty mbedtls ==================
if(NOT 3RD_PARTY_MBEDTLS_BASE_DIR)
    set (3RD_PARTY_MBEDTLS_BASE_DIR ${CMAKE_CURRENT_LIST_DIR})
endif()

set (3RD_PARTY_MBEDTLS_REPO_DIR "${3RD_PARTY_MBEDTLS_BASE_DIR}/repo")

find_package(MbedTLS)
if(MBEDTLS_FOUND)
    set (3RD_PARTY_MBEDTLS_INC_DIR ${MBEDTLS_INCLUDE_DIR})
    include_directories(${3RD_PARTY_MBEDTLS_INC_DIR})
    EchoWithColor(COLOR Green "-- Dependency: mbedtls found")
else()
    EchoWithColor(COLOR Yellow "-- Dependency: mbedtls not found")
endif()
