cmake_minimum_required(VERSION 3.28.3 FATAL_ERROR)
project(server)

set(CMAKE_CXX_STANDARD 20)            
set(CMAKE_CXX_STANDARD_REQUIRED ON) 

file(GLOB_RECURSE SOURCES 
    "sources/*.cpp" 
    "sources/*.h" 
    "vendor/base_64/base64.h" 
	"vendor/sqlite/sqlite3.h"
	"vendor/sqlite/sqlite3.c"	
    "vendor/base_64/base64.cpp"
)

include_directories("sources" "vendor" "vendor/base_64" "vendor/sqlite") 
add_executable(${PROJECT_NAME} ${SOURCES})
target_link_libraries(${PROJECT_NAME})