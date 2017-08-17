#ifndef _PIPELINE_PROGRAM_H_
#define _PIPELINE_PROGRAM_H_

#include "openGLHeader.h"

/*
  CSCI 420 Computer Graphics, University of Southern California
  Jernej Barbic, Bohan Wang, 2016

  Shader program helper class. 
  It loads shaders and properly initializes them in OpenGL.
*/

/* This class stores a set of shaders that serve as the stages of the rendering pipeline. 
   The stages are:
     (A) vertex shader (OpenGL 2.0 and higher),
     (B) fragment shader (OpenGL 2.0 and higher),
     (C) geometry shader (OpenGL 3.2 and higher),
     (D) tessellation control shader (OpenGL 4.0 and higher),
     (E) tessellation evaluation shader (OpenGL 4.0 and higher)

   Note: this is not the actual order of the stages in the pipeline. This order
   is used for historic reasons and to reflect the popularity of each stage,
   with (A) most popularly used, and (E) least popular.
   The actual pipeline order is:
   (A), (D), (E), (C), (B)
  
   The shaders are provided either by loading them from a file, or from a C string.
*/

class PipelineProgram
{
public:
  PipelineProgram();
  virtual ~PipelineProgram();

  // Loads the shaders, compiles them, and links them into a pipeline program
  // If a shader is not provided, the pointer should be set to NULL on input.
  // The provided shaders must come from one of the following choices:
  // (A), (B)
  // (A), (B), (C)
  // (A), (B), (D), (E)
  // (A), (B), (C), (D), (E)
  //
  // load shaders from a text file
  // all shaders must reside in the same folder, denoted by "filenameBasePath"
  int BuildShadersFromFiles(const char * filenameBasePath,
                            const char * vertexShaderFilename,
                            const char * fragmentShaderFilename,
                            const char * geometryShaderFilename = NULL,
                            const char * tessellationControlShaderFilename = NULL,
                            const char * tessellationEvaluationShaderFilename = NULL);
  // load shaders from a C text string
  int BuildShadersFromStrings(const char * vertexShaderCode,
                              const char * fragmentShaderCode,
                              const char * geometryShaderCode = NULL,
                              const char * tessellationControlShaderCode = NULL,
                              const char * tessellationEvaluationShaderCode = NULL);

  // binds (activates) all the provided shaders 
  void Bind();
  // get program handle
  GLuint GetProgramHandle() { return programHandle; }

protected:
  GLuint programHandle; // the handle to the pipeline program

  // load shader from a file into a string
  int LoadShader(const char * filename, char * code, int len); 

  // compile a shader
  // input:
  //   code: the shader code
  //   shaderType: the type of shader: GL_VERTEX_SHADER, GL_FRAGMENT_SHADER, GL_GEOMETRY_SHADER, GL_TESS_CONTROL_SHADER, GL_TESS_EVALUATION_SHADER
  // output:
  //   shaderHandle: the handle to the compiled shader
  //   return value: 0=success, non-zero: failure
  int CompileShader(const char * code, GLenum shaderType, GLuint & shaderHandle); // compile a shader
  GLint GetShaderVariableHandle(const char * name); // get the handle to the shader variable named "name"

  virtual int SetShaderVariableHandles() = 0; // call the SET_SHADER_VARIABLE_HANDLE macros for your shader variables here
  virtual int PreLink(); // custom operations to perform before linking (can be nothing in some cases)
};

#define SET_SHADER_VARIABLE_HANDLE(shaderVariableName)\
    h_##shaderVariableName = GetShaderVariableHandle(#shaderVariableName)

#endif

