cmake_minimum_required(VERSION 3.23)

# set the project name and version
project(MyWebServer VERSION 0.1 LANGUAGES C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED True)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_BUILD_TYPE Debug)

add_library(http STATIC http.c )
target_include_directories( http PUBLIC ../../include)


