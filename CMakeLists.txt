cmake_minimum_required(VERSION 3.23)

# set the project name and version
project(MyWebServer VERSION 0.1 LANGUAGES C)
enable_language(C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED True)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)


set(CMAKE_BUILD_TYPE Debug)

add_subdirectory(lib/collectc)
add_subdirectory(src/utils)
add_subdirectory(src/http)
add_subdirectory(src/parser)
add_subdirectory(src/server)

add_executable(output
        src/main.c
)

target_include_directories(output PRIVATE  include lib/collectc/src/include)

target_link_libraries(output PRIVATE
    server
    httplib
    parser
    utilslib
    collectc
    magic
    z
    pthread
)
# alternative cmake -DCMAKE_BUILD_TYPE=Debug <path and other arguments>
# create compile commands for intelli sense
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
