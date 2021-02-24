# This file is for development utility only. The published module is built by setuptools.

project(deflate64 LANGUAGES C)
cmake_minimum_required(VERSION 3.17)

find_package (Python REQUIRED COMPONENTS
    Development
)

add_library(infback9 STATIC
    zlib/zutil.c
    zlib/contrib/infback9/infback9.c
    zlib/contrib/infback9/inftree9.c
)
set_property(TARGET infback9
    PROPERTY POSITION_INDEPENDENT_CODE ON # Necessary for STATIC libraries
)
target_include_directories(infback9
    PUBLIC zlib
    PUBLIC zlib/contrib/infback9
)

Python_add_library(deflate64 MODULE WITH_SOABI
        zipfile_deflate64/deflate64/deflate64module.c
)
target_link_libraries(deflate64
    PUBLIC infback9
)
