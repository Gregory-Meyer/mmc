cmake_minimum_required(VERSION 3.11)

project(MemoryMappedFileCompression VERSION 0.3.0 LANGUAGES C)

list(INSERT CMAKE_MODULE_PATH 0 ${CMAKE_SOURCE_DIR}/cmake)

option(ENABLE_ZLIB "Build frontends for zlib, mmap-deflate (md) and mmap-inflate (mi)." OFF)
if(ENABLE_ZLIB)
    find_package(ZLIB 1.2 REQUIRED)
else()
    find_package(ZLIB 1.2)
endif()

option(ENABLE_LZ4 "Build frontends for LZ4, mmap-lz4-compress (mlc) and mmap-lz4-decompress (mld)." OFF)
if(ENABLE_LZ4)
    find_package(LZ4 1.8.3 REQUIRED)
else()
    find_package(LZ4 1.8.3)
endif()

option(ENABLE_ZSTD "Build frontends for zstd, mmap-zstd-compress (mzc) and mmap-zstd-decompress (mzd)." OFF)
if(ENABLE_ZSTD)
    find_package(zstd 1.4 REQUIRED)
else()
    find_package(zstd 1.4 REQUIRED)
endif()

add_compile_definitions(_GNU_SOURCE)

if(ZLIB_FOUND)
    add_executable(md src/deflate.c)
    target_compile_features(md PRIVATE c_std_99)
    target_link_libraries(md PRIVATE common ZLIB::ZLIB)
    set_target_properties(md PROPERTIES
        C_STANDARD_REQUIRED ON
        C_EXTENSIONS OFF
    )

    add_executable(mi src/inflate.c)
    target_compile_features(mi PRIVATE c_std_99)
    target_link_libraries(mi PRIVATE common ZLIB::ZLIB)
    set_target_properties(mi PROPERTIES
        C_STANDARD_REQUIRED ON
        C_EXTENSIONS OFF
    )

    install(TARGETS md mi DESTINATION bin)
endif()

if(LZ4_FOUND)
    add_executable(mlc src/lz4_compress.c)
    target_compile_features(mlc PRIVATE c_std_99)
    target_link_libraries(mlc PRIVATE common LZ4::LZ4)
    set_target_properties(mlc PROPERTIES
        C_STANDARD_REQUIRED ON
        C_EXTENSIONS OFF
    )

    add_executable(mld src/lz4_decompress.c)
    target_compile_features(mld PRIVATE c_std_99)
    target_link_libraries(mld PRIVATE common LZ4::LZ4)
    set_target_properties(mld PROPERTIES
        C_STANDARD_REQUIRED ON
        C_EXTENSIONS OFF
    )

    install(TARGETS mlc mld DESTINATION bin)
endif()

if(zstd_FOUND)
    add_executable(mzc src/zstd_compress.c)
    target_compile_features(mzc PRIVATE c_std_99)
    target_link_libraries(mzc PRIVATE common zstd::zstd)
    set_target_properties(mzc PROPERTIES
        C_STANDARD_REQUIRED ON
        C_EXTENSIONS OFF
    )

    add_executable(mzd src/zstd_decompress.c)
    target_compile_features(mzd PRIVATE c_std_99)
    target_link_libraries(mzd PRIVATE common zstd::zstd)
    set_target_properties(mzd PROPERTIES
        C_STANDARD_REQUIRED ON
        C_EXTENSIONS OFF
    )

    install(TARGETS mzc mzd DESTINATION bin)
endif()

add_library(common src/app.c src/argparse.c src/error.c src/file.c src/trie.c)
target_compile_features(common PUBLIC c_std_99)
target_include_directories(common PUBLIC include)
set_target_properties(common PROPERTIES
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF
)
