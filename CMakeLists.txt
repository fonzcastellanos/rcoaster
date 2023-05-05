cmake_minimum_required(VERSION 3.2)
project(rcoaster LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT LINUX AND NOT APPLE)
    message(FATAL_ERROR "Target system is not supported.")
endif()
 
add_compile_options(-Wall -Wextra -Wpedantic)

add_subdirectory(vendor/glm)

add_library(shader shader.cpp)
add_library(models models.cpp)
target_link_libraries(models PUBLIC glm)

add_executable(rcoaster main.cpp)
target_link_libraries(rcoaster PRIVATE glm shader models)
target_include_directories(rcoaster PRIVATE vendor)

if(LINUX)
    target_compile_options(rcoaster PRIVATE -lGLEW -lGL -lglut)
elseif(APPLE)
    target_compile_options(shader PRIVATE -Wno-deprecated-declarations)
    target_compile_options(models PRIVATE -Wno-deprecated-declarations)

    target_link_libraries(rcoaster PRIVATE "-framework OpenGL" "-framework GLUT")
    target_compile_options(rcoaster PRIVATE -Wno-deprecated-declarations)
endif()