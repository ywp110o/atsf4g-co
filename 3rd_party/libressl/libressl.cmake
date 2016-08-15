
# =========== 3rdparty libressl ==================
if(NOT 3RD_PARTY_LIBRESSL_BASE_DIR)
    set (3RD_PARTY_LIBRESSL_BASE_DIR ${CMAKE_CURRENT_LIST_DIR})
endif()

set (3RD_PARTY_LIBRESSL_REPO_DIR "${3RD_PARTY_LIBRESSL_BASE_DIR}/repo")

find_package(LibreSSL)
if(LIBRESSL_FOUND)
    set (3RD_PARTY_LIBRESSL_INC_DIR ${LIBRESSL_INCLUDE_DIR})
    include_directories(${3RD_PARTY_LIBRESSL_INC_DIR})
else()
    EchoWithColor(COLOR Yellow "-- Dependency: libressl not found")
endif()
