add_library(pipelineProgram pipelineProgram.cpp)
if(LINUX)
    target_compile_options(pipelineProgram PUBLIC -lGL -lglut)
elseif(APPLE)
    target_link_libraries(pipelineProgram PUBLIC "-framework OpenGL" "-framework GLUT")
    target_compile_options(pipelineProgram PRIVATE -Wno-deprecated-declarations)
endif()

add_library(openglMatrix openGLMatrix.cpp)
target_link_libraries(openglMatrix PUBLIC glm)

add_library(basicPipelineProgram basicPipelineProgram.cpp)
target_link_libraries(basicPipelineProgram PUBLIC pipelineProgram)

add_library(texPipelineProgram texPipelineProgram.cpp)
target_link_libraries(texPipelineProgram PUBLIC pipelineProgram)