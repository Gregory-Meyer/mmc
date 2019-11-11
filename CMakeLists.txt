cmake_minimum_required(VERSION 3.11)

project(StreamDecompressor)

find_package(ZLIB REQUIRED)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

add_executable(decompress src/decompress.c)
target_compile_features(decompress PRIVATE c_std_11)
target_link_libraries(decompress PRIVATE common ZLIB::ZLIB)
set_target_properties(decompress PROPERTIES
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS ON
)

add_executable(compress src/compress.c)
target_compile_features(compress PRIVATE c_std_11)
target_link_libraries(compress PRIVATE common ZLIB::ZLIB)
set_target_properties(compress PROPERTIES
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS ON
)

add_library(common src/common.c)
target_compile_features(common PUBLIC c_std_11)
target_include_directories(common PUBLIC src)
target_link_libraries(common PUBLIC ZLIB::ZLIB)
set_target_properties(common PROPERTIES
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS ON
)
