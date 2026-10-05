cmake_minimum_required(VERSION 3.10)
project(practica4)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

find_package(Threads REQUIRED)

add_executable(practica4
    src/main.cpp
)

target_link_libraries(practica4 Threads::Threads)
