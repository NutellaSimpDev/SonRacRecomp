
# sonicracingtransformed - ReXGlue Recompiled Project
#
# This file is yours to edit. 'rexglue migrate' will NOT overwrite it.
# SDK boilerplate lives in generated/rexglue.cmake.

cmake_minimum_required(VERSION 3.25)
project(sonicracingtransformed LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(generated/rexglue.cmake)

# Sources
set(SONICRACINGTRANSFORMED_SOURCES
    src/main.cpp
)

if(WIN32)
    add_executable(sonicracingtransformed WIN32 ${SONICRACINGTRANSFORMED_SOURCES})
else()
    add_executable(sonicracingtransformed ${SONICRACINGTRANSFORMED_SOURCES})
endif()

rexglue_setup_target(sonicracingtransformed)
