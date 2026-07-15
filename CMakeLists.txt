cmake_minimum_required(VERSION 3.23)

# set the project name and version
project(MyWebServer VERSION 0.1 LANGUAGES C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED True)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_BUILD_TYPE Debug)
add_executable(output
        src/main.c
)
add_subdirectory(src/global utils)
add_subdirectory(src/http http)
add_subdirectory(src/parser parser)
add_subdirectory(src/server server)

target_include_directories(output PUBLIC include)
target_link_libraries(output utils http parser server pthread m)
# alternative cmake -DCMAKE_BUILD_TYPE=Debug <path and other arguments>
# create compile commands for intelli sense
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

