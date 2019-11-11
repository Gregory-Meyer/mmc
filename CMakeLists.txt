cmake_minimum_required(VERSION 3.11)

project(StreamDeflate)

find_package(ZLIB REQUIRED)

add_executable(decompress src/decompress.c)
target_compile_features(decompress PRIVATE c_std_11)
target_link_libraries(decompress PRIVATE common ZLIB::ZLIB)
set_target_properties(decompress PROPERTIES
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF
)

add_executable(mmap-deflate src/mmap_deflate.c)
target_compile_features(mmap-deflate PRIVATE c_std_11)
target_link_libraries(mmap-deflate PRIVATE common ZLIB::ZLIB)
set_target_properties(mmap-deflate PROPERTIES
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF
)

add_library(common src/common.c)
target_compile_features(common PUBLIC c_std_11)
target_include_directories(common PUBLIC src)
target_link_libraries(common PUBLIC ZLIB::ZLIB)
set_target_properties(common PROPERTIES
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF
)
