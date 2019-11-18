cmake_minimum_required(VERSION 3.11)

project(MmapDEFLATE VERSION 0.1.2 LANGUAGES C)

option(ENABLE_ZLIB "Build frontends for zlib, mmap-deflate (md) and mmap-inflate (mi)." OFF)
if(ENABLE_ZLIB)
    find_package(ZLIB 1.2 REQUIRED)
else()
    find_package(ZLIB 1.2)
endif()

if(ZLIB_FOUND)
    add_executable(md src/deflate.c)
    target_compile_features(md PRIVATE c_std_99)
    target_link_libraries(md PRIVATE common zlib_common ZLIB::ZLIB)
    set_target_properties(md PROPERTIES
        C_STANDARD_REQUIRED ON
        C_EXTENSIONS OFF
    )

    add_executable(mi src/inflate.c)
    target_compile_features(mi PRIVATE c_std_99)
    target_link_libraries(mi PRIVATE common zlib_common ZLIB::ZLIB)
    set_target_properties(mi PROPERTIES
        C_STANDARD_REQUIRED ON
        C_EXTENSIONS OFF
    )

    add_library(zlib_common src/zlib_common.c)
    target_compile_features(zlib_common PUBLIC c_std_99)
    target_include_directories(zlib_common PUBLIC src)
    target_link_libraries(zlib_common PUBLIC common ZLIB::ZLIB)
    set_target_properties(zlib_common PROPERTIES
        C_STANDARD_REQUIRED ON
        C_EXTENSIONS OFF
    )

    install(TARGETS md mi DESTINATION bin)
endif()

add_library(common src/argparse.c src/common.c)
target_compile_features(common PUBLIC c_std_99)
target_include_directories(common PUBLIC src)
set_target_properties(common PROPERTIES
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF
)
