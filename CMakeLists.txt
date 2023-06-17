cmake_minimum_required(VERSION 3.2)
project(rcoaster LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT LINUX AND NOT APPLE)
    message(FATAL_ERROR "Target system is not supported.")
endif()
 
add_compile_options(-Wall -Wextra -Wpedantic)

add_subdirectory(vendor/glm)

add_library(cli cli.cpp)

add_library(shader shader.cpp)

add_library(meshes meshes.cpp)
target_link_libraries(meshes PUBLIC glm)

add_library(image image.cpp)
target_include_directories(image PRIVATE vendor)

add_library(opengl-renderer opengl_renderer.cpp)
target_link_libraries(opengl-renderer PUBLIC meshes PRIVATE image)
target_include_directories(opengl-renderer PRIVATE vendor)

add_library(scene scene.cpp)
target_link_libraries(scene PUBLIC glm meshes)

add_executable(rcoaster main.cpp)
target_link_libraries(rcoaster PRIVATE glm scene shader meshes cli opengl-renderer image)
target_include_directories(rcoaster PRIVATE vendor)

if(LINUX)
    target_link_libraries(rcoaster PRIVATE -lGLEW -lGL -lglut)
    target_link_libraries(opengl-renderer PRIVATE -lGLEW -lGL -lglut)
elseif(APPLE)
    target_compile_options(shader PRIVATE -Wno-deprecated-declarations)
    target_compile_options(meshes PRIVATE -Wno-deprecated-declarations)

    target_link_libraries(rcoaster PRIVATE "-framework OpenGL" "-framework GLUT")
    target_link_libraries(opengl-renderer PUBLIC "-framework OpenGL" "-framework GLUT")
    target_compile_options(rcoaster PRIVATE -Wno-deprecated-declarations)
    target_compile_options(opengl-renderer PUBLIC -Wno-deprecated-declarations)
endif()