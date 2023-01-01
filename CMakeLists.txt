cmake_minimum_required(VERSION 3.19)
project(isc_challenge)

set(CMAKE_CXX_STANDARD 14)

include_directories(${PROJECT_SOURCE_DIR})

add_subdirectory(server)
add_subdirectory(client)
