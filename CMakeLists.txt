cmake_minimum_required(VERSION 3.16)
project(GravitySim VERSION 0.4.0 LANGUAGES CXX C)

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

# ── OpenGL version with external dependencies ──────────────────────────────
include(FetchContent)

# Fetch GLFW
FetchContent_Declare(glfw
    URL https://github.com/glfw/glfw/releases/download/3.4/glfw-3.4.zip
    DOWNLOAD_EXTRACT_TIMESTAMP OFF
)

# Fetch GLAD source  
FetchContent_Declare(glad
    URL https://github.com/Dav1dde/glad/archive/refs/tags/v0.1.36.zip
    DOWNLOAD_EXTRACT_TIMESTAMP OFF
)

# Fetch glm for math utilities (optional, might be useful)
FetchContent_Declare(glm
    URL https://github.com/g-truc/glm/releases/download/0.9.9.8/glm-0.9.9.8.zip
    DOWNLOAD_EXTRACT_TIMESTAMP OFF
)

FetchContent_MakeAvailable(glfw glad glm)

# Build GLAD as a simple library from source
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/glad_config.h" "")
add_library(glad_lib STATIC
    "${glad_SOURCE_DIR}/src/gl.c"
)
target_include_directories(glad_lib PUBLIC "${glad_SOURCE_DIR}/include")

# OpenGL version executable
add_executable(gravity_gl_3d
    main_gl.cpp
)

# Link against GLFW, GLAD, and OpenGL
if(MSVC)
    target_link_libraries(gravity_gl_3d PRIVATE
        glfw
        glad_lib
        opengl32
    )
else()
    # MinGW/Linux
    target_link_libraries(gravity_gl_3d PRIVATE
        glfw
        glad_lib
        opengl32
    )
endif()

target_include_directories(gravity_gl_3d PRIVATE "${glad_SOURCE_DIR}/include")

# ── Info ─────────────────────────────────────────────────────────────────────
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "Targets: gravity_sim (terminal) + gravity_gl_3d (OpenGL)")