cmake_minimum_required(VERSION 3.16)

project(rasterizer LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Emit compile_commands.json for clangd / editor tooling
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Default to a Debug build when none is specified
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

find_package(SDL2 REQUIRED)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/*.c)

add_executable(rasterizer ${SOURCES})

target_include_directories(rasterizer PRIVATE ${CMAKE_SOURCE_DIR}/include)

# cglm is used header-only; SYSTEM suppresses warnings from its headers
target_include_directories(rasterizer SYSTEM PRIVATE ${CMAKE_SOURCE_DIR}/vendor/cglm/include)

target_link_libraries(rasterizer PRIVATE SDL2::SDL2 m)

target_compile_options(rasterizer PRIVATE
    -Wall -Wextra -Wpedantic
    $<$<CONFIG:Debug>:-g -O0 -fsanitize=address,undefined -fno-omit-frame-pointer>
    $<$<CONFIG:Release>:-O2>
)

target_link_options(rasterizer PRIVATE
    $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)
