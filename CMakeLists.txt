cmake_minimum_required(VERSION 3.16)
project(GravitySim VERSION 0.4.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Release build by default for performance
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS_RELEASE "-O2")

# ── Core library with shared physics ─────────────────────────────────────────
add_library(gravity_core
    src/Body.cpp
    src/Simulation.cpp
)

target_include_directories(gravity_core PRIVATE src)
target_include_directories(gravity_core PUBLIC src)

# ── Terminal version ─────────────────────────────────────────────────────────
add_executable(gravity_sim
    src/main.cpp
)

target_link_libraries(gravity_sim PRIVATE gravity_core)

# ── OpenGL version (using FetchContent for dependencies) ───────────────────
include(FetchContent)

# Fetch GLFW
FetchContent_Declare(glfw
    URL https://github.com/glfw/glfw/releases/download/3.4/glfw-3.4.zip
    DOWNLOAD_EXTRACT_TIMESTAMP OFF
)

# Fetch GLAD (using WebAssembly GL headers from Khronos)
FetchContent_Declare(glad
    URL https://github.com/Dav1dde/glad/archive/refs/tags/v0.1.36.zip
    DOWNLOAD_EXTRACT_TIMESTAMP OFF
)

FetchContent_MakeAvailable(glfw glad)

# Configure GLAD include directory
set(GLAD_INCLUDE_DIR "${glad_SOURCE_DIR}/include")

# Build GLAD as a library
add_library(glad_lib
    "${glad_SOURCE_DIR}/src/gl.c"
)
target_include_directories(glad_lib PUBLIC "${GLAD_INCLUDE_DIR}")

# OpenGL version executable
add_executable(gravity_gl_3d
    main_gl.cpp
)

target_include_directories(gravity_gl_3d PRIVATE ${GLAD_INCLUDE_DIR})
target_link_libraries(gravity_gl_3d PRIVATE
    glfw
    glad_lib
    opengl32
)

# ── Info ─────────────────────────────────────────────────────────────────────
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "Building: gravity_sim (terminal) + gravity_gl_3d (OpenGL)")