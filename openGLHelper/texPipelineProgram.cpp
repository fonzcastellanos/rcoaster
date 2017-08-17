#include <iostream>
#include <cstring>
#include "openGLHeader.h"
#include "texPipelineProgram.h"
using namespace std;

int TexPipelineProgram::Init(const char * shaderBasePath) 
{
  if (BuildShadersFromFiles(shaderBasePath, "tex.vertexShader.glsl", "tex.fragmentShader.glsl") != 0)
  {
    cout << "Failed to build the texture pipeline program." << endl;
    return 1;
  }

  cout << "Successfully built the texture pipeline program." << endl;
  return 0;
}

void TexPipelineProgram::SetModelViewMatrix(const float * m) 
{
  // pass "m" to the pipeline program, as the modelview matrix
  // students need to implement this
}

void TexPipelineProgram::SetProjectionMatrix(const float * m) 
{
  // pass "m" to the pipeline program, as the projection matrix
  // students need to implement this
}

int TexPipelineProgram::SetShaderVariableHandles() 
{
  // set h_modelViewMatrix and h_projectionMatrix
  // students need to implement this
  return 0;
}

