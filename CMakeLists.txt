cmake_minimum_required(VERSION 3.20)
project(playos_samples VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# Resolve a PlayOS dependency from a sibling checkout if present, else fetch.
function(playos_add_dependency name)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../${name}/CMakeLists.txt")
        message(STATUS "PlayOS: using sibling checkout for ${name}")
        add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../${name}"
                         "${CMAKE_BINARY_DIR}/_playos/${name}")
    else()
        message(STATUS "PlayOS: fetching ${name} from GitHub")
        FetchContent_Declare(${name}
            GIT_REPOSITORY "https://github.com/PlayOS-Foundation/${name}.git"
            GIT_TAG main)
        FetchContent_MakeAvailable(${name})
    endif()
endfunction()

# Raylib: use system package on platforms like Alpine where it's provided,
# otherwise fetch raylib 6.0 via FetchContent (default for Windows/macOS).
option(PLAYOS_USE_SYSTEM_RAYLIB "Use system-installed raylib (pkg-config)" OFF)
if(PLAYOS_USE_SYSTEM_RAYLIB)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(RAYLIB REQUIRED raylib)
    message(STATUS "PlayOS samples: using system raylib ${RAYLIB_VERSION}")
    add_library(raylib SHARED IMPORTED)
    set_target_properties(raylib PROPERTIES
        IMPORTED_LOCATION "${RAYLIB_LIBRARY_DIRS}/libraylib.so"
        INTERFACE_INCLUDE_DIRECTORIES "${RAYLIB_INCLUDE_DIRS}"
        INTERFACE_LINK_LIBRARIES "${RAYLIB_LIBRARIES}")
    set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_X11 OFF CACHE BOOL "" FORCE)
else()
    # Raylib must be fetched BEFORE playos-platform-api so the platform-api's
    # CMakeLists.txt can see the raylib target and build its raylib backends.
    FetchContent_Declare(raylib
        GIT_REPOSITORY https://github.com/raysan5/raylib.git
        GIT_TAG 6.0)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_GAMES OFF CACHE BOOL "" FORCE)

    # Optional Wayland build for samples on Linux (see playos-shell for details).
    option(PLAYOS_SHELL_WAYLAND "Build samples for Wayland (Linux)" OFF)
    if(PLAYOS_SHELL_WAYLAND AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set(GLFW_BUILD_WAYLAND ON CACHE BOOL "" FORCE)
        set(GLFW_BUILD_X11 OFF CACHE BOOL "" FORCE)
    endif()

    FetchContent_MakeAvailable(raylib)
endif()

playos_add_dependency(playos-platform-api)

# hello-playos: the reference "your first PlayOS app" sample. Demonstrates the
# intended developer experience: Raylib for rendering + the PlayOS Platform API
# for platform services (input, lifecycle).
add_executable(hello-playos hello-playos/src/main.cpp)
target_link_libraries(hello-playos PRIVATE
    PlayOS::PlatformAPI
    PlayOS::playos-input-raylib
    PlayOS::playos-audio-raylib
    PlayOS::playos-touch-raylib
    raylib)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(hello-playos PRIVATE PlayOS::playos-display-linux)
else()
    target_link_libraries(hello-playos PRIVATE PlayOS::playos-display-raylib)
endif()
if(WIN32 OR CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_link_libraries(hello-playos PRIVATE PlayOS::playos-storage-windows)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(hello-playos PRIVATE PlayOS::playos-storage-linux)
else()
    target_link_libraries(hello-playos PRIVATE PlayOS::playos-storage-null)
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(hello-playos PRIVATE PlayOS::playos-battery-linux)
else()
    target_link_libraries(hello-playos PRIVATE PlayOS::playos-battery-null)
endif()

# space-invaders: quick arcade clone demonstrating input + rendering.
add_executable(space-invaders space-invaders/src/main.cpp)
target_link_libraries(space-invaders PRIVATE
    PlayOS::PlatformAPI
    PlayOS::playos-input-raylib
    PlayOS::playos-audio-raylib
    PlayOS::playos-touch-raylib
    raylib)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(space-invaders PRIVATE PlayOS::playos-display-linux)
else()
    target_link_libraries(space-invaders PRIVATE PlayOS::playos-display-raylib)
endif()
if(WIN32 OR CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_link_libraries(space-invaders PRIVATE PlayOS::playos-storage-windows)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(space-invaders PRIVATE PlayOS::playos-storage-linux)
else()
    target_link_libraries(space-invaders PRIVATE PlayOS::playos-storage-null)
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(space-invaders PRIVATE PlayOS::playos-battery-linux)
else()
    target_link_libraries(space-invaders PRIVATE PlayOS::playos-battery-null)
endif()
