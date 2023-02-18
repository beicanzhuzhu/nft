cmake_minimum_required(VERSION 3.24)
project(nft)

set(CMAKE_CXX_STANDARD 11)

add_executable(nft main.cpp)
add_executable(test test.cpp)
target_link_libraries(nft -lWs2_32)
