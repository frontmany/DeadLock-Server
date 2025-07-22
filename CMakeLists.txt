cmake_minimum_required(VERSION 3.28.3 FATAL_ERROR)
project(server)

set(CMAKE_CXX_STANDARD 20)            
set(CMAKE_CXX_STANDARD_REQUIRED ON) 

file(GLOB_RECURSE SOURCES 
    "sources/*.cpp" 
    "sources/*.h" 
    "vendor/base_64/base64_my.h" 
	"vendor/sqlite/sqlite3.h"
	"vendor/sqlite/sqlite3.c"	
    "vendor/base_64/base64_my.cpp"
)

file(GLOB_RECURSE NET_LIB "vendor/network_library/*.h")
source_group("Source Files\\Net Library Files" FILES ${NET_LIB})

include_directories("sources" "vendor" "vendor/base_64" "vendor/sqlite" "vendor/network_library" "vendor/cryptopp") 
link_directories("vendor/cryptopp/x64/Output/Release")
add_executable(${PROJECT_NAME} ${SOURCES} ${NET_LIB})
target_link_libraries(${PROJECT_NAME} "cryptlib.lib")