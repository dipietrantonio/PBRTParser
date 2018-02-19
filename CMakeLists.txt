cmake_minimum_required (VERSION 3.5)

project (graphics17b_hw02)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED on)
# set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_BUILD_TYPE Release)

set(CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# compile flags
if(APPLE)
    add_definitions(-Wno-missing-braces)
endif(APPLE)
if(WIN32)
    add_definitions(/bigobj /D_CRT_SECURE_NO_WARNINGS /wd4018 /wd4244 /wd4305 /wd4800 /wd4267)
endif(WIN32)

add_library(projlib STATIC
    src/ext/json.hpp
    src/ext/stb_image.h
    src/ext/stb_image_write.h
    src/ext/stb_image_resize.h
    src/ext/tinyexr.h
    src/yocto_gltf.h
    src/yocto_gltf.cpp
    src/yocto_gl.h
    src/yocto_gl.cpp)

add_executable(parse src/main.cpp)
target_link_libraries(parse projlib)

