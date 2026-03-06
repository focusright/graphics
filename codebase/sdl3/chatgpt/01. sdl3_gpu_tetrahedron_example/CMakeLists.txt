cmake_minimum_required(VERSION 3.24)
project(sdl3_gpu_tetrahedron_example LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

# SDL 3.4.2 is the current stable release at the time this project was packaged.
FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.4.2
    GIT_SHALLOW TRUE
)

set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(SDL3)

add_executable(sdl3_gpu_tetrahedron_example
    src/main.cpp
)

target_compile_definitions(sdl3_gpu_tetrahedron_example PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(sdl3_gpu_tetrahedron_example PRIVATE SDL3::SDL3 d3dcompiler)

if (MSVC)
    target_compile_options(sdl3_gpu_tetrahedron_example PRIVATE /W4 /EHsc /utf-8)
else()
    target_compile_options(sdl3_gpu_tetrahedron_example PRIVATE -Wall -Wextra -Wpedantic)
endif()

if (WIN32)
    add_custom_command(TARGET sdl3_gpu_tetrahedron_example POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:sdl3_gpu_tetrahedron_example>
            $<TARGET_FILE_DIR:sdl3_gpu_tetrahedron_example>
        COMMAND_EXPAND_LISTS
    )
endif()
