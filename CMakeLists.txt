cmake_minimum_required(VERSION 3.28.3 FATAL_ERROR)
project(server)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CRYPTOPP_LIB_SUFFIX "Debug")
else()
    set(CRYPTOPP_LIB_SUFFIX "Release")
endif()

file(GLOB_RECURSE SOURCES 
    "sources/*.cpp" 
    "sources/*.h" 
    "vendor/base_64/base64_my.h" 
    "vendor/sqlite/sqlite3.h"
    "vendor/sqlite/sqlite3.c"    
    "vendor/base_64/base64_my.cpp"
)

file(GLOB_RECURSE NET_LIB 
    "vendor/network_library/*.h" 
    "vendor/network_library/*.cpp"
)

source_group("Source Files\\Net Library Files" FILES ${NET_LIB})

if(WIN32)
    add_subdirectory("Deadlock_server_updater")
endif()

if(WIN32)
    link_directories("vendor/cryptopp/x64/Output/${CRYPTOPP_LIB_SUFFIX}")
    set(CRYPTOPP_LIB "cryptlib.lib")
else()
    set(CRYPTOPP_LIB "${CMAKE_SOURCE_DIR}/vendor/cryptopp/libcryptopp.a")
endif()

include_directories(
    "sources" 
    "vendor" 
    "vendor/base_64" 
    "vendor/sqlite" 
    "vendor/network_library" 
    "vendor/cryptopp"
)

add_executable(${PROJECT_NAME} ${SOURCES} ${NET_LIB})

if(WIN32)
    target_link_libraries(${PROJECT_NAME} 
        ${CRYPTOPP_LIB}
        ws2_32 
    )
else()
    target_link_libraries(${PROJECT_NAME} 
        ${CRYPTOPP_LIB}
        pthread 
    )
endif()