cmake_minimum_required(VERSION 3.12)
project(psflib)

set(SOURCES psf2fs.c
            psflib.c)

set(CMAKE_POSITION_INDEPENDENT_CODE 1)
add_library(psflib STATIC ${SOURCES})
