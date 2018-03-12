project(vio2sf)

enable_language(C)
enable_language(CXX)

cmake_minimum_required(VERSION 2.6)

set(SOURCES src/vio2sf/desmume/armcpu.c
            src/vio2sf/desmume/arm_instructions.c
            src/vio2sf/desmume/barray.c
            src/vio2sf/desmume/bios.c
            src/vio2sf/desmume/cp15.c
            src/vio2sf/desmume/FIFO.c
            src/vio2sf/desmume/GPU.c
            src/vio2sf/desmume/isqrt.c
            src/vio2sf/desmume/matrix.c
            src/vio2sf/desmume/mc.c
            src/vio2sf/desmume/MMU.c
            src/vio2sf/desmume/NDSSystem.c
            src/vio2sf/desmume/resampler.c
            src/vio2sf/desmume/state.c
            src/vio2sf/desmume/thumb_instructions.c
            src/vio2sf/desmume/SPU.cpp)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")

add_library(vio2sf STATIC ${SOURCES})
