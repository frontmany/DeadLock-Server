cmake_minimum_required(VERSION 3.28.3 FATAL_ERROR)
project(server_database_viewer)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB_RECURSE SOURCES 
    "sources/*.cpp"
    "sources/*.h"
	"../vendor/sqlite/sqlite3.*"
)

add_executable(${PROJECT_NAME} ${SOURCES})

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