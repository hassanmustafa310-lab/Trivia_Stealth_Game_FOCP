cmake_minimum_required(VERSION 3.15)
project(TRIVIA_STEALTH)

set(CMAKE_CXX_STANDARD 17)

find_package(raylib CONFIG REQUIRED)

add_executable(TRIVIA_STEALTH main.cpp)
target_link_libraries(TRIVIA_STEALTH PRIVATE raylib)
