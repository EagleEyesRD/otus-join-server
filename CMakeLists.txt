cmake_minimum_required(VERSION 3.5)
project(join-server)

find_package(Boost REQUIRED)

include_directories(${Boost_INCLUDE_DIR})

set(CMAKE_CXX_STANDARD 14)

add_executable(join-server main.cpp)


if(MSVC)
    add_definitions(-D_SCL_SECURE_NO_WARNINGS)
endif()