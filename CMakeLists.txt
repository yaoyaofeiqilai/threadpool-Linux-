cmake_minimum_required(VERSION 3.10)
project(threadpool)
add_executable(threadpool test.cpp threadpool.cpp)

target_link_libraries(threadpool pthread)