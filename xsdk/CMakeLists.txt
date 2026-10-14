# Checks glslang on the Xbox 360 through xsdk; README-xsdk.md says how to build it.
cmake_minimum_required(VERSION 3.22.1)
project(glslang_xsdk_checks CXX)

set(GLSLANG_XSDK_SHADERS "" CACHE PATH "The recorded GLSL programs the probe compiles")
set(GLSLANG_XSDK_PACK "" CACHE FILEPATH "A shader pack built from them on the PC, which holds their SPIR-V")
option(GLSLANG_XSDK_BREAK "Target SPIR-V 1.3, so the probe's comparison with the PC's fails on purpose" OFF)

# glslang's optimiser needs SPIRV-Tools and runs only for HLSL or size optimisation, neither of which -V does.
set(ENABLE_OPT OFF CACHE BOOL "" FORCE)
set(ENABLE_HLSL OFF CACHE BOOL "" FORCE)
set(ENABLE_GLSLANG_BINARIES OFF CACHE BOOL "" FORCE)
set(GLSLANG_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_EXTERNAL OFF CACHE BOOL "" FORCE)
set(GLSLANG_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(.. glslang)

file(GLOB GLSLANG_XSDK_PROGRAMS "${GLSLANG_XSDK_SHADERS}/*.vert" "${GLSLANG_XSDK_SHADERS}/*.frag")
add_custom_command(OUTPUT programs.h
    COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/gen_programs.py" "${GLSLANG_XSDK_SHADERS}" "${GLSLANG_XSDK_PACK}"
        "${CMAKE_CURRENT_BINARY_DIR}/programs.h"
    DEPENDS gen_programs.py ${GLSLANG_XSDK_PROGRAMS} "${GLSLANG_XSDK_PACK}"
    VERBATIM)

add_executable(glslang_probe glslang_probe.cpp "${CMAKE_CURRENT_BINARY_DIR}/programs.h")
target_include_directories(glslang_probe PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(glslang_probe PRIVATE glslang glslang-default-resource-limits)
if(GLSLANG_XSDK_BREAK)
    target_compile_definitions(glslang_probe PRIVATE GLSLANG_XSDK_BREAK)
endif()
xsdk_add_xex(glslang_probe)
