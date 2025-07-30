cmake_minimum_required(VERSION 3.28.3 FATAL_ERROR)
project(server_updater)

set(CMAKE_CXX_STANDARD 20)            
set(CMAKE_CXX_STANDARD_REQUIRED ON) 

file(GLOB_RECURSE SOURCES 
    "sources/*.cpp" 
    "sources/*.h" 
)

include_directories("sources") 
add_executable(${PROJECT_NAME} ${SOURCES})