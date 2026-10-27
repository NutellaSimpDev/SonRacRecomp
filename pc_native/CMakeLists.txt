# SonRac - Windows build with the native renderer, against the same SDK fork as the Android build
# (android_native/CMakeLists.txt), to debug the renderer on the PC (RenderDoc, no phone needed).
#
# Configure (Git Bash, VS CMake + Ninja on PATH):
#   cmake -S pc_native -B out/pc-native -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
#     -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
# Run: out/pc-native/sonicracingtransformed.exe with the game in out/pc-game (see jugar_pc.ps1).

cmake_minimum_required(VERSION 3.25)
project(sonrac_pc LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SONRAC_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")
set(REXSDK_DIR "C:/Users/EduPedu/Desktop/LibertyRecompAndroid/glue/rexglue-sdk-main"
    CACHE PATH "rexglue-sdk source tree (the Android-capable fork)")

set(REXGLUE_BUILD_GTA4_RECOMP OFF CACHE BOOL "" FORCE)
set(REXGLUE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(REXGLUE_BUILD_GTA4_NATIVE OFF CACHE BOOL "" FORCE)
set(REXGLUE_USE_VULKAN ON CACHE BOOL "" FORCE)
set(REXGLUE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/rex-out" CACHE PATH "" FORCE)
add_subdirectory("${REXSDK_DIR}" rexglue-sdk)

include("${SONRAC_ROOT}/generated/sources.cmake")
set(SONRAC_GENERATED_SOURCES "")
foreach(src IN LISTS GENERATED_SOURCES)
    if(IS_ABSOLUTE "${src}")
        list(APPEND SONRAC_GENERATED_SOURCES "${src}")
    else()
        list(APPEND SONRAC_GENERATED_SOURCES "${SONRAC_ROOT}/${src}")
    endif()
endforeach()

add_executable(sonicracingtransformed WIN32
    ${SONRAC_ROOT}/src/main.cpp
    ${SONRAC_ROOT}/src/shader_dump.cpp
    ${SONRAC_ROOT}/src/cri_wakeup_probe.cpp
    ${SONRAC_GENERATED_SOURCES}
)
target_include_directories(sonicracingtransformed PRIVATE
    ${SONRAC_ROOT}
    ${SONRAC_ROOT}/src
    ${SONRAC_ROOT}/generated
    ${REXSDK_DIR}/thirdparty/imgui
    ${REXSDK_DIR}/src/ui/shaders
    ${REXSDK_DIR}/thirdparty/xxHash
    ${REXSDK_DIR}/thirdparty/renderdoc
    ${REXSDK_DIR}/thirdparty/vulkan-headers/include
)
target_link_libraries(sonicracingtransformed PRIVATE rex::runtime)
# The SDK's Vulkan backend (off by default on Windows).
target_compile_options(sonicracingtransformed PRIVATE -O2 -Wno-everything)
rexglue_configure_target(sonicracingtransformed GPU_PLUGINS xenos)
