cmake_minimum_required(VERSION 3.24)
project(nft)

set(CMAKE_CXX_STANDARD 17)

add_executable(nft main.cpp )
add_executable(old main_old.cpp)
target_link_libraries(nft -lWs2_32)
target_link_libraries(old -lWs2_32)

