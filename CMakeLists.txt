cmake_minimum_required(VERSION 3.23)

# set the project name and version
project(MyWebServer VERSION 0.1 LANGUAGES C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED True)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)


set(CMAKE_BUILD_TYPE Debug)

add_subdirectory(lib/collectc)
add_executable(output
        src/main.c
        src/utils/utils.c
        src/utils/threadpool.c
        src/utils/paths.c
        src/http/http.c
        src/parser/parser.c
        src/parser/chunked.c
        src/parser/encoding.c
        src/server/server.c
        src/server/router.c
        src/server/fshandler.c
)

target_include_directories(output PUBLIC include)
target_include_directories(output PUBLIC lib/collectc/src/include)
target_link_libraries(output PUBLIC pthread collectc magic )
# alternative cmake -DCMAKE_BUILD_TYPE=Debug <path and other arguments>
# create compile commands for intelli sense
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
