cmake_minimum_required(VERSION 3.21 FATAL_ERROR)

project(threadpool LANGUAGES CXX)

set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

list(APPEND _sources threadpool.hpp threadpool.cpp)

add_executable(demo-app test.cpp ${_sources})

set_target_properties(demo-app
  PROPERTIES
    CXX_STANDARD 20
    CXX_EXTENSIONS OFF
    CXX_STANDARD_REQUIRED ON
)