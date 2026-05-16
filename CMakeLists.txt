cmake_minimum_required(VERSION 3.20)

project(Game LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

find_package(Python COMPONENTS Interpreter REQUIRED)
execute_process(
    COMMAND ${Python_EXECUTABLE} -c "import jinja2"
    RESULT_VARIABLE JINJA2_IMPORT_RESULT
    OUTPUT_QUIET
    ERROR_QUIET
)
if(NOT JINJA2_IMPORT_RESULT EQUAL 0)
    message(FATAL_ERROR
        "GLAD needs the Python package Jinja2 to generate the OpenGL loader.\n"
        "Install it for CMake's Python with:\n"
        "  ${Python_EXECUTABLE} -m pip install --user \"Jinja2>=2.7,<4\"")
endif()

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
    GIT_SHALLOW TRUE
)

FetchContent_Declare(
    glad
    GIT_REPOSITORY https://github.com/Dav1dde/glad.git
    GIT_TAG v2.0.8
    GIT_SHALLOW TRUE
    SOURCE_SUBDIR cmake
)

FetchContent_MakeAvailable(glfw glad)

glad_add_library(
    glad_gl_core_43
    STATIC
    REPRODUCIBLE
    LOADER
    API gl:core=4.3
)

add_executable(Game main.cpp)

target_link_libraries(Game PRIVATE glfw glad_gl_core_43)

if(MSVC)
    target_compile_options(Game PRIVATE /W4 /permissive-)
else()
    target_compile_options(Game PRIVATE -Wall -Wextra -Wpedantic)
endif()
