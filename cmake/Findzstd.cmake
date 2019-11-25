find_package(PkgConfig)
pkg_check_modules(PC_zstd QUIET libzstd)

find_path(zstd_INCLUDE_DIR
    NAMES zstd.h
    PATHS ${PC_zstd_INCLUDE_DIRS}
)

set(zstd_VERSION ${PC_zstd_VERSION})

mark_as_advanced(zstd_FOUND zstd_INCLUDE_DIR zstd_VERSION)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(zstd
    REQUIRED_VARS zstd_INCLUDE_DIR PC_zstd_LINK_LIBRARIES
    VERSION_VAR zstd_VERSION
)

if(zstd_FOUND)
    get_filename_component(zstd_INCLUDE_DIRS ${zstd_INCLUDE_DIR} DIRECTORY)
endif()

if(zstd_FOUND AND NOT TARGET zstd::zstd)
    add_library(zstd::zstd INTERFACE IMPORTED)
    target_include_directories(zstd::zstd INTERFACE ${zstd_INCLUDE_DIRS})
    target_link_libraries(zstd::zstd INTERFACE ${PC_zstd_LINK_LIBRARIES})

    set(zstd_LIBRARIES zstd::zstd)
endif()
