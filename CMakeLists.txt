cmake_minimum_required(VERSION 3.28.3 FATAL_ERROR)
project(server)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CRYPTOPP_LIB_SUFFIX "Debug" CACHE INTERNAL "")
else()
    set(CRYPTOPP_LIB_SUFFIX "Release" CACHE INTERNAL "")
endif()

include_directories(
    "${CMAKE_SOURCE_DIR}/sources"
    "${CMAKE_SOURCE_DIR}/vendor"
    "${CMAKE_SOURCE_DIR}/vendor/base_64"
    "${CMAKE_SOURCE_DIR}/vendor/sqlite"
    "${CMAKE_SOURCE_DIR}/vendor/network_library"
    "${CMAKE_SOURCE_DIR}/vendor/cryptopp"
)

if(WIN32)
    link_directories("${CMAKE_SOURCE_DIR}/vendor/cryptopp/x64/Output/${CRYPTOPP_LIB_SUFFIX}")
    set(CRYPTOPP_LIB "cryptlib.lib" CACHE INTERNAL "")
else()
    set(CRYPTOPP_LIB "${CMAKE_SOURCE_DIR}/vendor/cryptopp/libcryptopp.a" CACHE INTERNAL "")
endif()

file(GLOB_RECURSE SOURCES 
    "sources/*.cpp" 
    "sources/*.h" 
    "vendor/base_64/base64_my.*" 
    "vendor/sqlite/sqlite3.*"
)

file(GLOB_RECURSE NET_LIB 
    "vendor/network_library/*.h" 
    "vendor/network_library/*.cpp"
)
source_group("Source Files\\Net Library Files" FILES ${NET_LIB})

add_subdirectory("Deadlock_server_updater")
add_subdirectory("Deadlock_server_database_viewer")

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