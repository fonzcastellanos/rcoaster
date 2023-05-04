#ifndef _OPENGLMATRIX_H_
#define _OPENGLMATRIX_H_

#include <list>
#include <string>
#include <glm/glm.hpp>

/*
  CSCI 420 Computer Graphics, University of Southern California
  Jernej Barbic, Bohan Wang, 2016

  Helper class for modelview and projection OpenGL matrix transformations.
  All matrices in this class are stored in the column-major format.

  This class is implemented using the OpenGL Mathematics (glm) library:
  http://glm.g-truc.net/0.9.7/index.html
*/

class OpenGLMatrix 
{
public:
  OpenGLMatrix();

  // set the model transformation (use in ModelView mode)
  void Translate(float x, float y, float z);
  void Rotate(float angle, float x, float y, float z); // angle is in degrees
  void Scale(float x, float y, float z);

  // set the camera position, aim and orientation (use in ModelView mode)
  void LookAt(float eyeX, float eyeY, float eyeZ, 
              float centerX, float centerY, float centerZ, 
              float upX, float upY, float upZ);

  // set the projection (use in Projection mode)
  void Ortho(float left, float right, float bottom, float top, float zNear, float zFar);
  void Frustum(float left, float right, float bottom, float top, float zNear, float zFar);
  void Perspective(float fovY, float aspect, float zNear, float zFar); // angle is in degrees

  // matrix manipulation
  enum MatrixMode { ModelView = 0, Projection = 1, NumMatrixModes };
  void SetMatrixMode(MatrixMode matrixMode); // either ModelView or Projection
  void LoadIdentity(); // set the current matrix to identity
  void LoadMatrix(const float m[16]); // set the current matrix to "m"
  void MultMatrix(const float m[16]); // multiply the current matrix with "m" (on the right)
  void GetMatrix(float * matrix); // get the current matrix 
  void GetNormalMatrix(float * matrix); // get the current matrix for transformations of normals

  // compute the combined projection-modelview matrix, by multiplying the current Projection and Modelview matrices
  void GetProjectionModelViewMatrix(float * matrix); 

  // the matrix stack
  // there are two separate stacks, one for ModelView and one for Projection mode
  // the stack operations always apply to the current matrix and stack of the current mode
  void PushMatrix(); // push the current matrix 
  void PopMatrix(); // pop the top matrix from the stack, and store it as the current matrix

  // for debugging
  std::string ToString(); // write the current matrix, followed by the stack of matrices, to a text string 

protected:
  MatrixMode matrixMode;
  glm::mat4 currentMatrix[NumMatrixModes]; // the current matrix for each mode
  std::list<glm::mat4> matrixStack[NumMatrixModes]; // the matrix stack for each mode
  void multiplyMatrixToCurrent(const glm::mat4 &m); // low-level multiplication routine
  std::string matrixToString(const glm::mat4 &m, int indent, int precision = 7, bool fixed = false); // helper routine for "ToString"
};

#endif

