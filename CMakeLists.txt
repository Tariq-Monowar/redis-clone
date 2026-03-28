cmake_minimum_required(VERSION 3.16)
project(miniredis LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(miniredis_core STATIC
    src/kv_store.cpp
    src/resp.cpp
    src/server.cpp
)
target_include_directories(miniredis_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_executable(miniredis-server src/main_server.cpp)
target_link_libraries(miniredis-server PRIVATE miniredis_core pthread)

add_executable(miniredis-cli tools/miniredis_cli.cpp src/resp.cpp)
target_include_directories(miniredis-cli PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
