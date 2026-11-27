cmake_minimum_required(VERSION 3.20)

# Dependencies such as raylib still declare very old minimum versions; CMake 4.x
# refuses those unless a policy floor is given. Older CMake ignores this variable.
if(NOT DEFINED CMAKE_POLICY_VERSION_MINIMUM)
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
endif()

project(VoxelForge VERSION 1.0.0 LANGUAGES C CXX)

if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW) # use extraction time for URL downloads
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(VOXELFORGE_BUILD_GAME  "Build the game executable (fetches raylib)" ON)
option(VOXELFORGE_BUILD_TESTS "Build the unit tests (fetches doctest)"     ON)
option(VOXELFORGE_WERROR      "Treat compiler warnings as errors"         OFF)
option(VOXELFORGE_ASAN        "Build with AddressSanitizer + UBSan"        OFF)
option(VOXELFORGE_TSAN        "Build with ThreadSanitizer"                 OFF)

if(VOXELFORGE_ASAN AND VOXELFORGE_TSAN)
    message(FATAL_ERROR "VOXELFORGE_ASAN and VOXELFORGE_TSAN cannot be combined")
endif()

include(FetchContent)
find_package(Threads REQUIRED)

# ---------------------------------------------------------------------------
# Compiler settings applied only to VoxelForge targets (never to dependencies)
# ---------------------------------------------------------------------------
set(VOXELFORGE_SANITIZER_FLAGS "")
if(VOXELFORGE_ASAN)
    set(VOXELFORGE_SANITIZER_FLAGS -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=undefined)
elseif(VOXELFORGE_TSAN)
    set(VOXELFORGE_SANITIZER_FLAGS -fsanitize=thread -fno-omit-frame-pointer)
endif()

function(voxelforge_configure_target target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
        if(VOXELFORGE_WERROR)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
        if(VOXELFORGE_WERROR)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
        if(VOXELFORGE_SANITIZER_FLAGS)
            target_compile_options(${target} PRIVATE ${VOXELFORGE_SANITIZER_FLAGS})
            target_link_options(${target} PRIVATE ${VOXELFORGE_SANITIZER_FLAGS})
        endif()
    endif()
endfunction()

# ---------------------------------------------------------------------------
# Engine library: platform independent code (world, meshing, physics, io...).
# It has no raylib dependency so it can be unit tested without a GPU.
# ---------------------------------------------------------------------------
add_library(voxelforge_engine STATIC
    src/core/Benchmark.cpp
    src/core/CommandLine.cpp
    src/core/Settings.cpp
    src/physics/Collision.cpp
    src/physics/Raycast.cpp
    src/player/Inventory.cpp
    src/player/Player.cpp
    src/render/Frustum.cpp
    src/render/Mesher.cpp
    src/render/TextureAtlas.cpp
    src/threading/ThreadPool.cpp
    src/world/Block.cpp
    src/world/Chunk.cpp
    src/world/Noise.cpp
    src/world/TerrainGenerator.cpp
    src/world/World.cpp
)
target_include_directories(voxelforge_engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(voxelforge_engine PUBLIC Threads::Threads)
voxelforge_configure_target(voxelforge_engine)

# ---------------------------------------------------------------------------
# Game executable (raylib front-end)
# ---------------------------------------------------------------------------
if(VOXELFORGE_BUILD_GAME)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_GAMES    OFF CACHE BOOL "" FORCE)
    set(OPENGL_VERSION "3.3" CACHE STRING "" FORCE) # macOS tops out at 3.3 core
    FetchContent_Declare(raylib
        GIT_REPOSITORY https://github.com/raysan5/raylib.git
        GIT_TAG        5.5
        GIT_SHALLOW    TRUE
        GIT_PROGRESS   TRUE
    )
    FetchContent_MakeAvailable(raylib)

    # Third-party code is not ours to fix: silence its warnings so the build log only
    # reports VoxelForge diagnostics, and treat raylib headers as system headers.
    foreach(dep raylib glfw)
        if(TARGET ${dep})
            target_compile_options(${dep} PRIVATE $<IF:$<C_COMPILER_ID:MSVC>,/w,-w>)
        endif()
    endforeach()
    get_target_property(RAYLIB_INCLUDE_DIRS raylib INTERFACE_INCLUDE_DIRECTORIES)
    set_target_properties(raylib PROPERTIES INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${RAYLIB_INCLUDE_DIRS}")

    # Shaders are embedded into the binary so the executable runs from any directory.
    set(VOXELFORGE_SHADERS
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/chunk.vs
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/chunk.fs
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/water.fs
    )
    set(VOXELFORGE_SHADER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/generated/EmbeddedShaders.hpp)
    add_custom_command(
        OUTPUT  ${VOXELFORGE_SHADER_HEADER}
        COMMAND ${CMAKE_COMMAND} -DOUTPUT=${VOXELFORGE_SHADER_HEADER} "-DINPUTS=${VOXELFORGE_SHADERS}"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedShaders.cmake
        DEPENDS ${VOXELFORGE_SHADERS} ${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedShaders.cmake
        COMMENT "Embedding GLSL shaders"
        VERBATIM
    )

    add_executable(voxelforge
        src/main.cpp
        src/core/Game.cpp
        src/core/Input.cpp
        src/render/GpuMesh.cpp
        src/render/Renderer.cpp
        src/render/ShaderProgram.cpp
        src/ui/Hud.cpp
        ${VOXELFORGE_SHADER_HEADER}
    )
    target_include_directories(voxelforge PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated)
    target_link_libraries(voxelforge PRIVATE voxelforge_engine raylib)
    voxelforge_configure_target(voxelforge)
    if(APPLE)
        target_link_libraries(voxelforge PRIVATE "-framework IOKit" "-framework Cocoa" "-framework OpenGL")
    endif()
endif()

# ---------------------------------------------------------------------------
# Unit tests
# ---------------------------------------------------------------------------
if(VOXELFORGE_BUILD_TESTS)
    enable_testing()
    FetchContent_Declare(doctest
        URL      https://raw.githubusercontent.com/doctest/doctest/v2.4.11/doctest/doctest.h
        URL_HASH SHA256=44faa038e9c3f9728efbda143748d01124ea0a27f4bf78f35a15d8fab2e039fb
        DOWNLOAD_NO_EXTRACT TRUE
    )
    FetchContent_MakeAvailable(doctest)

    add_executable(voxelforge_tests
        tests/test_main.cpp
        tests/test_atlas.cpp
        tests/test_chunk.cpp
        tests/test_collision.cpp
        tests/test_core.cpp
        tests/test_frustum.cpp
        tests/test_mesher.cpp
        tests/test_noise.cpp
        tests/test_player.cpp
        tests/test_raycast.cpp
        tests/test_threadpool.cpp
        tests/test_world.cpp
        tests/test_terrain.cpp
    )
    target_include_directories(voxelforge_tests SYSTEM PRIVATE ${doctest_SOURCE_DIR})
    target_link_libraries(voxelforge_tests PRIVATE voxelforge_engine)
    voxelforge_configure_target(voxelforge_tests)
    add_test(NAME voxelforge_tests COMMAND voxelforge_tests)
    set_tests_properties(voxelforge_tests PROPERTIES TIMEOUT 600)
endif()
