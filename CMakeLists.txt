cmake_minimum_required(VERSION 3.0)
project(suggestdpi LANGUAGES C)
set(CMAKE_C_STANDARD 99)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -pedantic")

find_package(PkgConfig REQUIRED)
pkg_check_modules(XCB REQUIRED xcb xcb-atom xcb-randr)

add_executable(suggestdpi
        buffer.c
        buffer.h
        config.c
        config.h
        format.c
        format.h
        log.c
        log.h
        main.c
        screen_info.c
        screen_info.h
)
target_compile_options(suggestdpi PUBLIC ${XCB_CFLAGS})
target_link_options(suggestdpi PUBLIC ${XCB_LDFLAGS})
target_link_libraries(suggestdpi PUBLIC m)
