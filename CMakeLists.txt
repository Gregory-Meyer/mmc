cmake_minimum_required(VERSION 3.11)

project(StreamDeflate VERSION 0.1.0 LANGUAGES C)

find_package(ZLIB REQUIRED)

add_executable(mi src/mmap_inflate.c)
target_compile_features(mi PRIVATE c_std_99)
target_link_libraries(mi PRIVATE common ZLIB::ZLIB)
set_target_properties(mi PROPERTIES
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF
)

add_executable(md src/mmap_deflate.c)
target_compile_features(md PRIVATE c_std_99)
target_link_libraries(md PRIVATE common ZLIB::ZLIB)
set_target_properties(md PROPERTIES
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF
)

add_library(common src/common.c)
target_compile_features(common PUBLIC c_std_99)
target_include_directories(common PUBLIC src)
target_link_libraries(common PUBLIC ZLIB::ZLIB)
set_target_properties(common PROPERTIES
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF
)
