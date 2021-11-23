cmake_minimum_required(VERSION 3.5 FATAL_ERROR)

project(threadpool LANGUAGES CXX)

if(MSVC AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 19.29.30129 AND CMAKE_VERSION VERSION_GREATER 3.20.3)
    set(CMAKE_CXX_STANDARD 23) # /std:c++latest - unlocks the non stable cpp20 features. For new 16.11 versions
else()
    set(CMAKE_CXX_STANDARD 20) # /std:c++latest for msvc and -std=c++20 for everyone else.
endif()

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

list(APPEND _sources threadpool.hpp threadpool.cpp)

add_executable(demo-app test.cpp ${_sources})
