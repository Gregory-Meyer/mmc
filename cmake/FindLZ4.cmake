find_package(PkgConfig)
pkg_check_modules(PC_LZ4 QUIET liblz4)

find_path(LZ4_INCLUDE_DIR
    NAMES lz4frame.h
    PATHS ${PC_LZ4_INCLUDE_DIRS}
)

set(LZ4_VERSION ${PC_LZ4_VERSION})

mark_as_advanced(LZ4_FOUND LZ4_INCLUDE_DIR LZ4_VERSION)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LZ4
    REQUIRED_VARS LZ4_INCLUDE_DIR PC_LZ4_LINK_LIBRARIES
    VERSION_VAR LZ4_VERSION
)

if(LZ4_FOUND)
    get_filename_component(LZ4_INCLUDE_DIRS ${LZ4_INCLUDE_DIR} DIRECTORY)
endif()

if(LZ4_FOUND AND NOT TARGET LZ4::LZ4)
    add_library(LZ4::LZ4 INTERFACE IMPORTED)
    target_include_directories(LZ4::LZ4 INTERFACE ${LZ4_INCLUDE_DIRS})
    target_link_libraries(LZ4::LZ4 INTERFACE ${PC_LZ4_LINK_LIBRARIES})

    set(LZ4_LIBRARIES LZ4::LZ4)
endif()
