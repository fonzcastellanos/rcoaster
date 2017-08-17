#include <list>
#include <string>
#include <iomanip>
#include <sstream>
#include <cstring>

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "openGLMatrix.h"

using namespace std;

OpenGLMatrix::OpenGLMatrix()
{
  this->matrixMode = ModelView;
}

void OpenGLMatrix::Rotate(float angle, float x, float y, float z)
{
  glm::mat4 R = glm::rotate(glm::radians(angle), glm::vec3(x, y, z));
  multiplyMatrixToCurrent(R);
}

void OpenGLMatrix::Translate(float x, float y, float z)
{
  glm::mat4 T = glm::translate(glm::vec3(x, y, z));
  multiplyMatrixToCurrent(T);
}

void OpenGLMatrix::Scale(float x, float y, float z)
{
  glm::mat4 S = glm::scale(glm::vec3(x, y, z));
  multiplyMatrixToCurrent(S);
}

void OpenGLMatrix::LookAt(float eyeX, float eyeY, float eyeZ, float centerX, float centerY, float centerZ, float upX, float upY, float upZ)
{
  glm::mat4 V = glm::lookAt(glm::vec3(eyeX, eyeY, eyeZ), 
                            glm::vec3(centerX, centerY, centerZ), 
                            glm::vec3(upX, upY, upZ));
  multiplyMatrixToCurrent(V);
}

void OpenGLMatrix::Ortho(float left, float right, float bottom, float top, float zNear, float zFar)
{
  glm::mat4 O = glm::ortho(left, right, bottom, top, zNear, zFar);
  multiplyMatrixToCurrent(O);
}

void OpenGLMatrix::Frustum(float left, float right, float bottom, float top, float zNear, float zFar)
{
  glm::mat4 F = glm::frustum(left, right, bottom, top, zNear, zFar);
  multiplyMatrixToCurrent(F);
}

void OpenGLMatrix::Perspective(float fovY, float aspect, float zNear, float zFar)
{
  glm::mat4 P = glm::perspective(glm::radians(fovY), aspect, zNear, zFar);
  multiplyMatrixToCurrent(P);
}

void OpenGLMatrix::SetMatrixMode(MatrixMode matrixMode) 
{ 
  this->matrixMode = matrixMode; 
}

void OpenGLMatrix::LoadIdentity()
{
  currentMatrix[(int)matrixMode] = glm::mat4(1.0f);
}

void OpenGLMatrix::LoadMatrix(const float * matrix)
{
  currentMatrix[(int)matrixMode] = glm::make_mat4(matrix);
}

void OpenGLMatrix::MultMatrix(const float * matrix)
{
  glm::mat4 M = glm::make_mat4(matrix);
  multiplyMatrixToCurrent(M);
}

void OpenGLMatrix::PushMatrix()
{
  matrixStack[(int)matrixMode].push_back(currentMatrix[(int)matrixMode]);
}

void OpenGLMatrix::PopMatrix()
{
  if (matrixStack[(int)matrixMode].size() >= 1) // cannot pop from an empty stack
  {
    currentMatrix[(int)matrixMode] = matrixStack[(int)matrixMode].back();
    matrixStack[(int)matrixMode].pop_back();
  }
}

void OpenGLMatrix::GetMatrix(float * matrix)
{
  memcpy(matrix, glm::value_ptr(currentMatrix[(int)matrixMode]), sizeof(float) * 16);
}

void OpenGLMatrix::GetNormalMatrix(float * matrix)
{
  glm::mat4 MV(currentMatrix[(int)matrixMode]);
  glm::mat4 NMV = glm::transpose(glm::inverse(MV));
  memcpy(matrix, glm::value_ptr(NMV), sizeof(float) * 16);
}

void OpenGLMatrix::multiplyMatrixToCurrent(const glm::mat4 & matrix)
{
  currentMatrix[(int)matrixMode] = currentMatrix[(int)matrixMode] * matrix;
}

void OpenGLMatrix::GetProjectionModelViewMatrix(float * matrix)
{
  glm::mat4 PM = currentMatrix[Projection] * currentMatrix[ModelView];
  memcpy(matrix, glm::value_ptr(PM), sizeof(float) * 16);
}

string OpenGLMatrix::matrixToString(const glm::mat4 &m, int indent, int precision, bool fixed)
{
  ostringstream oss;
  const float * mdata = glm::value_ptr(m);
  oss << setprecision(precision);
  if (fixed)
    oss << std::fixed;

  for (int i = 0; i < indent; i++)
    oss << ' ';
  oss << '[' << mdata[0] << ", " << mdata[4] << ", " << mdata[8] << ", " << mdata[12] << ";\n";
  for (int i = 0; i < indent; i++)
    oss << ' ';
  oss << ' ' << mdata[1] << ", " << mdata[5] << ", " << mdata[9] << ", " << mdata[13] << ";\n";
  for (int i = 0; i < indent; i++)
    oss << ' ';
  oss << ' ' << mdata[2] << ", " << mdata[6] << ", " << mdata[10] << ", " << mdata[14] << ";\n";
  for (int i = 0; i < indent; i++)
    oss << ' ';
  oss << ' ' << mdata[3] << ", " << mdata[7] << ", " << mdata[11] << ", " << mdata[15] << "]\n";

  return oss.str();
}

string OpenGLMatrix::ToString()
{
  string stateHeader[2] = { "ModelView", "Projection" };
  ostringstream oss;
  for (int i = 0; i < (int)NumMatrixModes; i++) {
    oss << stateHeader[i] << ":\n";
    oss << "  Current matrix:\n" << matrixToString(currentMatrix[i], 4);
    oss << "  Stack (# matrices = " << matrixStack[i].size() << "):\n";
    
    typedef list<glm::mat4>::reverse_iterator MIter;
    int lvl = (int)matrixStack[i].size() - 1;
    for (MIter itt = matrixStack[i].rbegin(); itt != matrixStack[i].rend(); itt++) {
      oss << "    M (" << lvl << "):\n" << matrixToString(*itt, 6);
      lvl--;
    }
  }
  oss << endl;
  return oss.str();
}

