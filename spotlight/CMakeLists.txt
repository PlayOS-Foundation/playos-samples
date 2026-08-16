cmake_minimum_required(VERSION 3.18)
project(playos-sample-spotlight C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# libplayos (host build against the sibling platform-api tree).
set(PLAYOS_PLATFORM_API_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/../../playos-platform-api"
    CACHE PATH "Path to playos-platform-api source tree")

find_path(PLAYOS_INCLUDE_DIR playos/playos.h
    HINTS "${PLAYOS_PLATFORM_API_DIR}/include"
          "${PLAYOS_PLATFORM_API_DIR}/install/include"
    REQUIRED)

find_library(PLAYOS_LIB playos
    HINTS "${PLAYOS_PLATFORM_API_DIR}/build"
          "${PLAYOS_PLATFORM_API_DIR}/install/lib"
    REQUIRED)

# Raylib (host development build — install raylib dev files, or point
# RAYLIB_DIR at a raylib build tree).
set(RAYLIB_DIR "" CACHE PATH "Path to a raylib build/install tree")
find_path(RAYLIB_INCLUDE_DIR raylib.h
    HINTS "${RAYLIB_DIR}/include" REQUIRED)
find_library(RAYLIB_LIB raylib
    HINTS "${RAYLIB_DIR}/lib" "${RAYLIB_DIR}/src" REQUIRED)

add_executable(game src/main.c)
target_include_directories(game PRIVATE ${PLAYOS_INCLUDE_DIR} ${RAYLIB_INCLUDE_DIR})
target_link_libraries(game PRIVATE ${PLAYOS_LIB} ${RAYLIB_LIB} m)

# The manifest references "bin/game" relative to the installed game dir.
set_target_properties(game PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
