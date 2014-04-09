project(psflib)

enable_language(C)

cmake_minimum_required(VERSION 2.6)

set(SOURCES psf2fs.c
            psflib.c)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")

add_library(psflib STATIC ${SOURCES})
