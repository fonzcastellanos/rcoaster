#include <cstdio>
#include <cstdlib>
#include <glm/glm.hpp>
#include <iomanip>
#include <iostream>
#include <limits>
#include <vector>

#include "basicPipelineProgram.h"
#include "glutHeader.h"
#include "imageIO.h"
#include "openGLHeader.h"
#include "openGLMatrix.h"
#include "spline.hpp"
#include "texPipelineProgram.h"
#include "types.hpp"

#ifdef WIN32
#ifdef _DEBUG
#pragma comment(lib, "glew32d.lib")
#else
#pragma comment(lib, "glew32.lib")
#endif
#endif

#ifdef WIN32
char shaderBasePath[1024] = SHADER_BASE_PATH;
#else
char shaderBasePath[1024] = "openGLHelper";
#endif

#define BUFFER_OFFSET(offset) ((GLvoid *)(offset))

enum Status { kStatusOk, kStatusIOError };

GLuint screenshotCount = 0;
GLuint record = 0;

// Window properties
int windowWidth = 1280;
int windowHeight = 720;
char windowTitle[512] = "CSCI 420 homework II";

// Number of rendered frames
unsigned int frameCount = 0;

// Helper matrix object
OpenGLMatrix *matrix;

// Pipeline programs
BasicPipelineProgram *basicProgram;
TexPipelineProgram *texProgram;

// Mouse state
int mousePos[2];            // x,y coordinate of the mouse position
int leftMouseButton = 0;    // 1 if pressed, 0 if not
int middleMouseButton = 0;  // 1 if pressed, 0 if not
int rightMouseButton = 0;   // 1 if pressed, 0 if not

// World operations
typedef enum { ROTATE, TRANSLATE, SCALE } CONTROL_STATE;
CONTROL_STATE controlState = ROTATE;

// World state
float landRotate[3] = {0.0, 0.0, 0.0};
float landTranslate[3] = {0.0, 0.0, 0.0};
float landScale[3] = {1.0, 1.0, 1.0};

// Camera attributes
int camStep = 0;
glm::vec3 camPos(0.0, 0.0, 0.0);
glm::vec3 camDir(0.0, 0.0, 0.0);
glm::vec3 camNorm;
glm::vec3 camBinorm;

struct CameraPathVertices {
  std::vector<glm::vec3> positions;
  std::vector<glm::vec3> normals;
  std::vector<glm::vec3> binormals;
};

static uint Count(const CameraPathVertices *c) { return c->positions.size(); }

static CameraPathVertices camera_path_vertices;

// Vertex container
struct Vertex {
  glm::vec3 position;
  glm::vec4 color;
};

/************************ SPLINE **********************/

// GLuint vaoSpline;
// GLuint vboSpline;

// std::vector<Vertex> splineVertices;

// void makeSpline() {
//   glGenVertexArrays(1, &vaoSpline);
//   glBindVertexArray(vaoSpline);

//   glGenBuffers(1, &vboSpline);
//   glBindBuffer(GL_ARRAY_BUFFER, vboSpline);
//   glBufferData(GL_ARRAY_BUFFER, splineVertices.size() * sizeof(Vertex),
//                &splineVertices[0], GL_STATIC_DRAW);

//   GLuint program = basicProgram->GetProgramHandle();
//   GLuint posLoc = glGetAttribLocation(program, "position");
//   GLuint colorLoc = glGetAttribLocation(program, "color");
//   glEnableVertexAttribArray(posLoc);
//   glEnableVertexAttribArray(colorLoc);
//   GLintptr offset = 0;
//   glVertexAttribPointer(posLoc, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
//                         BUFFER_OFFSET(offset));
//   offset += sizeof(Vertex().position);
//   glVertexAttribPointer(colorLoc, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex),
//                         BUFFER_OFFSET(offset));

//   glBindVertexArray(0);
// }

// void drawSpline() { glDrawArrays(GL_LINES, 0, splineVertices.size()); }

/********************** RAILS **********************/

GLuint vaoRail;
GLuint vboRail;
GLuint iboRail;

static std::vector<Vertex> rail_vertices;

std::vector<GLuint> railIndices;

void makeRails() {
  glGenVertexArrays(1, &vaoRail);
  glBindVertexArray(vaoRail);

  glGenBuffers(1, &vboRail);
  glBindBuffer(GL_ARRAY_BUFFER, vboRail);
  glBufferData(GL_ARRAY_BUFFER, rail_vertices.size() * sizeof(Vertex),
               rail_vertices.data(), GL_STATIC_DRAW);

  GLuint program = basicProgram->GetProgramHandle();
  GLuint posLoc = glGetAttribLocation(program, "position");
  GLuint colorLoc = glGetAttribLocation(program, "color");
  glEnableVertexAttribArray(posLoc);
  glEnableVertexAttribArray(colorLoc);
  GLintptr offset = 0;
  glVertexAttribPointer(posLoc, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                        BUFFER_OFFSET(offset));
  offset += sizeof(Vertex().position);
  glVertexAttribPointer(colorLoc, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                        BUFFER_OFFSET(offset));

  glGenBuffers(1, &iboRail);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, iboRail);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, railIndices.size() * sizeof(GLuint),
               &railIndices[0], GL_STATIC_DRAW);

  glBindVertexArray(0);
}

void drawRails() {
  GLintptr offset = 0;
  glDrawElements(GL_TRIANGLES, railIndices.size() / 2, GL_UNSIGNED_INT,
                 BUFFER_OFFSET(offset));
  offset += railIndices.size() / 2 * sizeof(GLuint);
  glDrawElements(GL_TRIANGLES, railIndices.size() / 2, GL_UNSIGNED_INT,
                 BUFFER_OFFSET(offset));
}

/**************** GROUND ********************/

GLuint vaoGround;
GLuint vboGround;
GLuint texGround;

const GLfloat edgeLen = 256.0;

glm::vec3 groundVertices[6] = {glm::vec3(-edgeLen / 2.0, 0.0, edgeLen / 2.0),
                               glm::vec3(-edgeLen / 2.0, 0.0, -edgeLen / 2.0),
                               glm::vec3(edgeLen / 2.0, 0.0, -edgeLen / 2.0),
                               glm::vec3(-edgeLen / 2.0, 0.0, edgeLen / 2.0),
                               glm::vec3(edgeLen / 2.0, 0.0, -edgeLen / 2.0),
                               glm::vec3(edgeLen / 2.0, 0.0, edgeLen / 2.0)};

const GLfloat upperLim = edgeLen / 2.0 / 4.0;

glm::vec2 groundTexCoords[6] = {
    glm::vec2(0.0, 0.0),           glm::vec2(0.0, upperLim),
    glm::vec2(upperLim, upperLim), glm::vec2(0.0, 0.0),
    glm::vec2(upperLim, upperLim), glm::vec2(upperLim, 0.0)};

// Bounding sphere
glm::vec3 groundCenter(0.0, 0.0, 0.0);
const GLfloat groundRadius = edgeLen / 2.0;

void makeGround() {
  glGenVertexArrays(1, &vaoGround);
  glBindVertexArray(vaoGround);

  glGenBuffers(1, &vboGround);
  glBindBuffer(GL_ARRAY_BUFFER, vboGround);
  glBufferData(GL_ARRAY_BUFFER,
               sizeof(groundVertices) + sizeof(groundTexCoords), NULL,
               GL_STATIC_DRAW);
  GLintptr offset = 0;
  glBufferSubData(GL_ARRAY_BUFFER, offset, sizeof(groundVertices),
                  groundVertices);
  offset += sizeof(groundVertices);
  glBufferSubData(GL_ARRAY_BUFFER, offset, sizeof(groundTexCoords),
                  groundTexCoords);

  GLuint program = texProgram->GetProgramHandle();
  GLuint posLoc = glGetAttribLocation(program, "position");
  GLuint texLoc = glGetAttribLocation(program, "texCoord");
  glEnableVertexAttribArray(posLoc);
  glEnableVertexAttribArray(texLoc);
  offset = 0;
  glVertexAttribPointer(posLoc, 3, GL_FLOAT, GL_FALSE, 0,
                        BUFFER_OFFSET(offset));
  offset += sizeof(groundVertices);
  glVertexAttribPointer(texLoc, 2, GL_FLOAT, GL_FALSE, 0,
                        BUFFER_OFFSET(offset));

  glBindVertexArray(0);
}

void drawGround() { glDrawArrays(GL_TRIANGLES, 0, 6); }

/*********************** SKY ****************************/

GLuint vaoSky;
GLuint vboSky;
GLuint texSky;

// Skybox
glm::vec3 skyVertices[36] = {
    // x = -1 face
    glm::vec3(-edgeLen / 2.0, -edgeLen / 2.0, -edgeLen / 2.0),
    glm::vec3(-edgeLen / 2.0, edgeLen / 2.0, -edgeLen / 2.0),
    glm::vec3(-edgeLen / 2.0, edgeLen / 2.0, edgeLen / 2.0),
    glm::vec3(-edgeLen / 2.0, -edgeLen / 2.0, -edgeLen / 2.0),
    glm::vec3(-edgeLen / 2.0, edgeLen / 2.0, edgeLen / 2.0),
    glm::vec3(-edgeLen / 2.0, -edgeLen / 2.0, edgeLen / 2.0),

    // x = 1 face
    glm::vec3(edgeLen / 2.0, -edgeLen / 2.0, -edgeLen / 2.0),
    glm::vec3(edgeLen / 2.0, edgeLen / 2.0, -edgeLen / 2.0),
    glm::vec3(edgeLen / 2.0, edgeLen / 2.0, edgeLen / 2.0),
    glm::vec3(edgeLen / 2.0, -edgeLen / 2.0, -edgeLen / 2.0),
    glm::vec3(edgeLen / 2.0, edgeLen / 2.0, edgeLen / 2.0),
    glm::vec3(edgeLen / 2.0, -edgeLen / 2.0, edgeLen / 2.0),

    // y = -1 face
    glm::vec3(-edgeLen / 2.0, -edgeLen / 2.0, -edgeLen / 2.0),
    glm::vec3(-edgeLen / 2.0, -edgeLen / 2.0, edgeLen / 2.0),
    glm::vec3(edgeLen / 2.0, -edgeLen / 2.0, edgeLen / 2.0),
    glm::vec3(-edgeLen / 2.0, -edgeLen / 2.0, -edgeLen / 2.0),
    glm::vec3(edgeLen / 2.0, -edgeLen / 2.0, edgeLen / 2.0),
    glm::vec3(edgeLen / 2.0, -edgeLen / 2.0, -edgeLen / 2.0),

    // y = 1 face
    glm::vec3(-edgeLen / 2.0, edgeLen / 2.0, -edgeLen / 2.0),
    glm::vec3(-edgeLen / 2.0, edgeLen / 2.0, edgeLen / 2.0),
    glm::vec3(edgeLen / 2.0, edgeLen / 2.0, edgeLen / 2.0),
    glm::vec3(-edgeLen / 2.0, edgeLen / 2.0, -edgeLen / 2.0),
    glm::vec3(edgeLen / 2.0, edgeLen / 2.0, edgeLen / 2.0),
    glm::vec3(edgeLen / 2.0, edgeLen / 2.0, -edgeLen / 2.0),

    // z = -1 face
    glm::vec3(-edgeLen / 2.0, -edgeLen / 2.0, -edgeLen / 2.0),
    glm::vec3(-edgeLen / 2.0, edgeLen / 2.0, -edgeLen / 2.0),
    glm::vec3(edgeLen / 2.0, edgeLen / 2.0, -edgeLen / 2.0),
    glm::vec3(-edgeLen / 2.0, -edgeLen / 2.0, -edgeLen / 2.0),
    glm::vec3(edgeLen / 2.0, edgeLen / 2.0, -edgeLen / 2.0),
    glm::vec3(edgeLen / 2.0, -edgeLen / 2.0, -edgeLen / 2.0),

    // z = 1 face
    glm::vec3(-edgeLen / 2.0, -edgeLen / 2.0, edgeLen / 2.0),
    glm::vec3(-edgeLen / 2.0, edgeLen / 2.0, edgeLen / 2.0),
    glm::vec3(edgeLen / 2.0, edgeLen / 2.0, edgeLen / 2.0),
    glm::vec3(-edgeLen / 2.0, -edgeLen / 2.0, edgeLen / 2.0),
    glm::vec3(edgeLen / 2.0, edgeLen / 2.0, edgeLen / 2.0),
    glm::vec3(edgeLen / 2.0, -edgeLen / 2.0, edgeLen / 2.0),
};

const GLfloat texUpperLim = 1.0;

glm::vec2 texSkyCoords[36] = {glm::vec2(0.0, 0.0),
                              glm::vec2(0.0, texUpperLim),
                              glm::vec2(texUpperLim, texUpperLim),
                              glm::vec2(0.0, 0.0),
                              glm::vec2(texUpperLim, texUpperLim),
                              glm::vec2(texUpperLim, 0.0),
                              glm::vec2(0.0, 0.0),
                              glm::vec2(0.0, texUpperLim),
                              glm::vec2(texUpperLim, texUpperLim),
                              glm::vec2(0.0, 0.0),
                              glm::vec2(texUpperLim, texUpperLim),
                              glm::vec2(texUpperLim, 0.0),
                              glm::vec2(0.0, 0.0),
                              glm::vec2(0.0, texUpperLim),
                              glm::vec2(texUpperLim, texUpperLim),
                              glm::vec2(0.0, 0.0),
                              glm::vec2(texUpperLim, texUpperLim),
                              glm::vec2(texUpperLim, 0.0),
                              glm::vec2(0.0, 0.0),
                              glm::vec2(0.0, texUpperLim),
                              glm::vec2(texUpperLim, texUpperLim),
                              glm::vec2(0.0, 0.0),
                              glm::vec2(texUpperLim, texUpperLim),
                              glm::vec2(texUpperLim, 0.0),
                              glm::vec2(0.0, 0.0),
                              glm::vec2(0.0, texUpperLim),
                              glm::vec2(texUpperLim, texUpperLim),
                              glm::vec2(0.0, 0.0),
                              glm::vec2(texUpperLim, texUpperLim),
                              glm::vec2(texUpperLim, 0.0),
                              glm::vec2(0.0, 0.0),
                              glm::vec2(0.0, texUpperLim),
                              glm::vec2(texUpperLim, texUpperLim),
                              glm::vec2(0.0, 0.0),
                              glm::vec2(texUpperLim, texUpperLim),
                              glm::vec2(texUpperLim, 0.0)};

void makeSky() {
  glGenVertexArrays(1, &vaoSky);
  glBindVertexArray(vaoSky);

  glGenBuffers(1, &vboSky);
  glBindBuffer(GL_ARRAY_BUFFER, vboSky);
  glBufferData(GL_ARRAY_BUFFER, sizeof(skyVertices) + sizeof(texSkyCoords),
               NULL, GL_STATIC_DRAW);
  GLintptr offset = 0;
  glBufferSubData(GL_ARRAY_BUFFER, offset, sizeof(skyVertices), skyVertices);
  offset += sizeof(skyVertices);
  glBufferSubData(GL_ARRAY_BUFFER, offset, sizeof(texSkyCoords), texSkyCoords);

  GLuint program = texProgram->GetProgramHandle();
  GLuint posLoc = glGetAttribLocation(program, "position");
  GLuint texLoc = glGetAttribLocation(program, "texCoord");
  glEnableVertexAttribArray(posLoc);
  glEnableVertexAttribArray(texLoc);
  offset = 0;
  glVertexAttribPointer(posLoc, 3, GL_FLOAT, GL_FALSE, 0,
                        BUFFER_OFFSET(offset));
  offset += sizeof(skyVertices);
  glVertexAttribPointer(texLoc, 2, GL_FLOAT, GL_FALSE, 0,
                        BUFFER_OFFSET(offset));

  glBindVertexArray(0);
}

void drawSky() { glDrawArrays(GL_TRIANGLES, 0, 36); }

/*********************** CROSSBARS ******************/

GLuint vaoCrossbar;
GLuint vboCrossbar;
GLuint texCrossbar;

std::vector<glm::vec3> crossbarVertices;
std::vector<glm::vec2> texCrossbarCoords;

void makeCrossbars() {
  glGenVertexArrays(1, &vaoCrossbar);
  glBindVertexArray(vaoCrossbar);

  glGenBuffers(1, &vboCrossbar);
  glBindBuffer(GL_ARRAY_BUFFER, vboCrossbar);
  GLuint vSize = crossbarVertices.size() * sizeof(glm::vec3);
  GLuint tSize = texCrossbarCoords.size() * sizeof(glm::vec2);
  glBufferData(GL_ARRAY_BUFFER, vSize + tSize, NULL, GL_STATIC_DRAW);
  GLintptr offset = 0;
  glBufferSubData(GL_ARRAY_BUFFER, offset, vSize, &crossbarVertices[0]);
  offset += vSize;
  glBufferSubData(GL_ARRAY_BUFFER, offset, tSize, &texCrossbarCoords[0]);

  GLuint program = texProgram->GetProgramHandle();
  GLuint posLoc = glGetAttribLocation(program, "position");
  GLuint texLoc = glGetAttribLocation(program, "texCoord");
  glEnableVertexAttribArray(posLoc);
  glEnableVertexAttribArray(texLoc);
  offset = 0;
  glVertexAttribPointer(posLoc, 3, GL_FLOAT, GL_FALSE, 0,
                        BUFFER_OFFSET(offset));
  offset += vSize;
  glVertexAttribPointer(texLoc, 2, GL_FLOAT, GL_FALSE, 0,
                        BUFFER_OFFSET(offset));

  glBindVertexArray(0);
}

void drawCrossbar() {
  for (int offset = 0; offset < crossbarVertices.size(); offset += 36) {
    glDrawArrays(GL_TRIANGLES, offset, 36);
  }
}

/****************** CATMULL-ROM SPLINE ******************/

static SplineVertices spline_vertices;

// Bounding sphere of spline
glm::vec3 splineCenter;
float splineRadius;
glm::vec3 spline_min_point(std::numeric_limits<float>::max(),
                           std::numeric_limits<float>::max(),
                           std::numeric_limits<float>::max());

glm::vec3 spline_max_point(std::numeric_limits<float>::min(),
                           std::numeric_limits<float>::min(),
                           std::numeric_limits<float>::min());

constexpr float kTolerance = 0.0001;

static void UpdateMaxPoint(glm::vec3 *maxp, const glm::vec3 *p) {
  if (p->x > maxp->x + kTolerance) {
    maxp->x = p->x;
  }
  if (p->y > maxp->y + kTolerance) {
    maxp->y = p->y;
  }
  if (p->z > maxp->z + kTolerance) {
    maxp->z = p->z;
  }
}

static void UpdateMinPoint(glm::vec3 *minp, glm::vec3 *p) {
  if (p->x < minp->x + kTolerance) {
    minp->x = p->x;
  }
  if (p->y < minp->y + kTolerance) {
    minp->y = p->y;
  }
  if (p->z < minp->z + kTolerance) {
    minp->z = p->z;
  }
}

void updateBoundingSphere(glm::vec3 splineMaxPoint, glm::vec3 splineMinPoint) {
  splineCenter = glm::vec3((splineMaxPoint.x + splineMinPoint.x) / 2,
                           (splineMaxPoint.y + splineMinPoint.y) / 2,
                           (splineMaxPoint.z + splineMinPoint.z) / 2);
  splineRadius = sqrt(pow(splineMaxPoint.x - splineCenter.x, 2) +
                      pow(splineMaxPoint.y - splineCenter.y, 2) +
                      pow(splineMaxPoint.z - splineCenter.z, 2));
}

static void MakeCameraPath(const SplineVertices *spline,
                           CameraPathVertices *campath) {
  uint vertex_count = Count(spline);

  campath->positions.resize(vertex_count);
  campath->normals.resize(vertex_count);
  campath->binormals.resize(vertex_count);

  for (uint i = 0; i < vertex_count; ++i) {
    campath->positions[i] = spline->positions[i];
    campath->positions[i] -= splineCenter;
    campath->positions[i].y += splineRadius - groundRadius * 0.5f;

    if (i == 0) {
      campath->normals[i] = glm::normalize(
          glm::cross(spline->tangents[i], glm::vec3(0, 1, -0.5)));
      campath->binormals[i] =
          glm::normalize(glm::cross(spline->tangents[i], campath->normals[i]));
    } else {
      campath->normals[i] = glm::normalize(
          glm::cross(campath->binormals[i - 1], spline->tangents[i]));
      campath->binormals[i] =
          glm::normalize(glm::cross(spline->tangents[i], campath->normals[i]));
    }
  }
}

static void MakeRails(const CameraPathVertices *campath_vertices,
                      std::vector<Vertex> *rail_vertices) {
  static constexpr float kAlpha = 0.1;
  static const glm::vec4 kRailColor(0.5, 0.5, 0.5, 1);
  static constexpr uint kVertexCount = 8;

  const auto &campath = *campath_vertices;
  auto &rail = *rail_vertices;

  uint campath_count = Count(&campath);

  uint rail_count = 2 * campath_count * kVertexCount;
  rail.resize(rail_count);

  for (uint i = 0; i < rail_count; ++i) {
    rail[i].color = kRailColor;
  }

  for (uint i = 0; i < 2; ++i) {
    for (uint j = 0; j < campath_count; ++j) {
      uint k = kVertexCount * (j + i * campath_count);
      rail[k].position =
          campath.positions[j] +
          kAlpha * (-campath.normals[j] + campath.binormals[j] * 0.5f);
      rail[k + 1].position =
          campath.positions[j] + kAlpha * (campath.binormals[j] * 0.5f);
      rail[k + 2].position =
          campath.positions[j] + kAlpha * (campath.binormals[j]);
      rail[k + 3].position =
          campath.positions[j] +
          kAlpha * (campath.normals[j] + campath.binormals[j]);
      rail[k + 4].position =
          campath.positions[j] +
          kAlpha * (campath.normals[j] - campath.binormals[j]);
      rail[k + 5].position =
          campath.positions[j] + kAlpha * (-campath.binormals[j]);
      rail[k + 6].position =
          campath.positions[j] + kAlpha * (-campath.binormals[j] * 0.5f);
      rail[k + 7].position =
          campath.positions[j] +
          kAlpha * (-campath.normals[j] - campath.binormals[j] * 0.5f);

      for (uint l = 0; l < kVertexCount; ++l) {
        if (i == 0) {
          rail[k + l].position += campath.binormals[j];
        } else {
          rail[k + l].position -= campath.binormals[j];
        }
      }
    }
  }

  for (uint i = 0; i < 2; ++i) {
    for (uint j = i * rail_count / 2; j + kVertexCount < rail_count / (2 - i);
         j += kVertexCount) {
      // top face
      railIndices.push_back(j + 4);
      railIndices.push_back(j + 12);
      railIndices.push_back(j + 3);
      railIndices.push_back(j + 12);
      railIndices.push_back(j + 11);
      railIndices.push_back(j + 3);

      // top right right face
      railIndices.push_back(j + 3);
      railIndices.push_back(j + 11);
      railIndices.push_back(j + 2);
      railIndices.push_back(j + 11);
      railIndices.push_back(j + 10);
      railIndices.push_back(j + 2);

      // top right bottom face
      railIndices.push_back(j + 2);
      railIndices.push_back(j + 10);
      railIndices.push_back(j + 1);
      railIndices.push_back(j + 10);
      railIndices.push_back(j + 9);
      railIndices.push_back(j + 1);

      // bottom right face
      railIndices.push_back(j + 1);
      railIndices.push_back(j + 9);
      railIndices.push_back(j);
      railIndices.push_back(j + 9);
      railIndices.push_back(j + 8);
      railIndices.push_back(j);

      // bottom face
      railIndices.push_back(j);
      railIndices.push_back(j + 8);
      railIndices.push_back(j + 7);
      railIndices.push_back(j + 8);
      railIndices.push_back(j + 15);
      railIndices.push_back(j + 7);

      // bottom left face
      railIndices.push_back(j + 14);
      railIndices.push_back(j + 6);
      railIndices.push_back(j + 15);
      railIndices.push_back(j + 6);
      railIndices.push_back(j + 7);
      railIndices.push_back(j + 15);

      // top left bottom face
      railIndices.push_back(j + 13);
      railIndices.push_back(j + 5);
      railIndices.push_back(j + 14);
      railIndices.push_back(j + 5);
      railIndices.push_back(j + 6);
      railIndices.push_back(j + 14);

      // top left left face
      railIndices.push_back(j + 12);
      railIndices.push_back(j + 4);
      railIndices.push_back(j + 13);
      railIndices.push_back(j + 4);
      railIndices.push_back(j + 5);
      railIndices.push_back(j + 13);
    }
  }
}

void setupCrossbars(const CameraPathVertices *campath,
                    const std::vector<glm::vec3> *spline_tangents) {
  static constexpr float alpha = 0.1;
  static constexpr float beta = 1.5;
  static constexpr int num_vertices = 8;
  static constexpr float bar2barDist = 1.0;
  static constexpr float barDepth = 0.3;

  const auto &spl_tangents = *spline_tangents;

  float distMoved = 0.0;
  glm::vec3 v[num_vertices];
  for (int j = 1; j < campath->positions.size(); ++j) {
    distMoved += glm::length(campath->positions[j] - campath->positions[j - 1]);
    if (distMoved > bar2barDist) {
      v[0] =
          campath->positions[j] +
          alpha * (-beta * campath->normals[j] + campath->binormals[j] * 0.5f) +
          campath->binormals[j];
      v[1] = campath->positions[j] +
             alpha * (-campath->normals[j] + campath->binormals[j] * 0.5f) +
             campath->binormals[j];
      v[2] =
          campath->positions[j] +
          alpha * (-beta * campath->normals[j] + campath->binormals[j] * 0.5f) -
          campath->binormals[j] - alpha * campath->binormals[j];
      v[3] = campath->positions[j] +
             alpha * (-campath->normals[j] + campath->binormals[j] * 0.5f) -
             campath->binormals[j] - alpha * campath->binormals[j];

      v[4] =
          campath->positions[j] +
          alpha * (-beta * campath->normals[j] + campath->binormals[j] * 0.5f) +
          campath->binormals[j] + barDepth * spl_tangents[j];
      v[5] = campath->positions[j] +
             alpha * (-campath->normals[j] + campath->binormals[j] * 0.5f) +
             campath->binormals[j] + barDepth * spl_tangents[j];
      v[6] =
          campath->positions[j] +
          alpha * (-beta * campath->normals[j] + campath->binormals[j] * 0.5f) -
          campath->binormals[j] + barDepth * spl_tangents[j] -
          alpha * campath->binormals[j];
      v[7] = campath->positions[j] +
             alpha * (-campath->normals[j] + campath->binormals[j] * 0.5f) -
             campath->binormals[j] + barDepth * spl_tangents[j] -
             alpha * campath->binormals[j];

      // top face
      crossbarVertices.push_back(v[6]);
      crossbarVertices.push_back(v[5]);
      crossbarVertices.push_back(v[2]);
      crossbarVertices.push_back(v[5]);
      crossbarVertices.push_back(v[1]);
      crossbarVertices.push_back(v[2]);

      // right face
      crossbarVertices.push_back(v[5]);
      crossbarVertices.push_back(v[4]);
      crossbarVertices.push_back(v[1]);
      crossbarVertices.push_back(v[4]);
      crossbarVertices.push_back(v[0]);
      crossbarVertices.push_back(v[1]);

      // bottom face
      crossbarVertices.push_back(v[4]);
      crossbarVertices.push_back(v[7]);
      crossbarVertices.push_back(v[0]);
      crossbarVertices.push_back(v[7]);
      crossbarVertices.push_back(v[3]);
      crossbarVertices.push_back(v[0]);

      // left face
      crossbarVertices.push_back(v[7]);
      crossbarVertices.push_back(v[6]);
      crossbarVertices.push_back(v[3]);
      crossbarVertices.push_back(v[6]);
      crossbarVertices.push_back(v[2]);
      crossbarVertices.push_back(v[3]);

      // back face
      crossbarVertices.push_back(v[5]);
      crossbarVertices.push_back(v[6]);
      crossbarVertices.push_back(v[4]);
      crossbarVertices.push_back(v[6]);
      crossbarVertices.push_back(v[7]);
      crossbarVertices.push_back(v[4]);

      // front face
      crossbarVertices.push_back(v[2]);
      crossbarVertices.push_back(v[1]);
      crossbarVertices.push_back(v[3]);
      crossbarVertices.push_back(v[1]);
      crossbarVertices.push_back(v[0]);
      crossbarVertices.push_back(v[3]);

      for (int i = 0; i < 6; ++i) {
        texCrossbarCoords.push_back(glm::vec2(0.0, 1.0));
        texCrossbarCoords.push_back(glm::vec2(1.0, 1.0));
        texCrossbarCoords.push_back(glm::vec2(0.0, 0.0));
        texCrossbarCoords.push_back(glm::vec2(1.0, 1.0));
        texCrossbarCoords.push_back(glm::vec2(1.0, 0.0));
        texCrossbarCoords.push_back(glm::vec2(0.0, 0.0));
      }

      distMoved = 0.0;
    }
  }
}

Status LoadSplines(const char *track_filepath,
                   std::vector<std::vector<Point>> *splines) {
  auto &splines_ = *splines;

  std::FILE *track_file = std::fopen(track_filepath, "r");
  if (!track_file) {
    std::fprintf(stderr, "Could not open track file %s\n", track_filepath);
    return kStatusIOError;
  }

  uint spline_count;
  int ret = std::fscanf(track_file, "%u", &spline_count);
  if (ret < 1) {
    std::fprintf(stderr, "Could not read spline count from track file %s\n",
                 track_filepath);
    std::fclose(track_file);
    return kStatusIOError;
  }

  splines->resize(spline_count);

  char filepath[4096];
  for (uint j = 0; j < spline_count; ++j) {
    int ret = std::fscanf(track_file, "%s", filepath);
    if (ret < 1) {
      std::fprintf(stderr,
                   "Could not read spline filename from track file %s\n",
                   track_filepath);
      std::fclose(track_file);
      return kStatusIOError;
    }

    FILE *file = std::fopen(filepath, "r");
    if (!file) {
      std::fprintf(stderr, "Could not open spline file %s\n", filepath);
      std::fclose(track_file);
      return kStatusIOError;
    }

    uint ctrl_point_count;
    uint type;
    ret = std::fscanf(file, "%u %u", &ctrl_point_count, &type);
    if (ret < 1) {
      std::fprintf(
          stderr,
          "Could not read control point count and type from spline file %s\n",
          filepath);
      std::fclose(file);
      std::fclose(track_file);
      return kStatusIOError;
    }

    splines_[j].resize(ctrl_point_count);

    uint i = 0;
    while ((ret = std::fscanf(file, "%f %f %f", &splines_[j][i].x,
                              &splines_[j][i].y, &splines_[j][i].z)) > 0) {
      ++i;
    }
    if (ret == 0) {
      std::fprintf(stderr, "Could not read control point from spline file %s\n",
                   filepath);
      std::fclose(file);
      std::fclose(track_file);
      return kStatusIOError;
    }

    std::fclose(file);
  }

  std::fclose(track_file);

  return kStatusOk;
}

int initTexture(const char *imageFilename, GLuint textureHandle) {
  // read the texture image
  ImageIO img;
  ImageIO::fileFormatType imgFormat;
  ImageIO::errorType err = img.load(imageFilename, &imgFormat);

  if (err != ImageIO::OK) {
    printf("Loading texture from %s failed.\n", imageFilename);
    return -1;
  }

  // check that the number of bytes is a multiple of 4
  if (img.getWidth() * img.getBytesPerPixel() % 4) {
    printf(
        "Error (%s): The width*numChannels in the loaded image must be a "
        "multiple of 4.\n",
        imageFilename);
    return -1;
  }

  // allocate space for an array of pixels
  int width = img.getWidth();
  int height = img.getHeight();
  unsigned char *pixelsRGBA =
      new unsigned char[4 * width *
                        height];  // we will use 4 bytes per pixel, i.e., RGBA

  // fill the pixelsRGBA array with the image pixels
  memset(pixelsRGBA, 0, 4 * width * height);  // set all bytes to 0
  for (int h = 0; h < height; h++)
    for (int w = 0; w < width; w++) {
      // assign some default byte values (for the case where
      // img.getBytesPerPixel() < 4)
      pixelsRGBA[4 * (h * width + w) + 0] = 0;    // red
      pixelsRGBA[4 * (h * width + w) + 1] = 0;    // green
      pixelsRGBA[4 * (h * width + w) + 2] = 0;    // blue
      pixelsRGBA[4 * (h * width + w) + 3] = 255;  // alpha channel; fully opaque

      // set the RGBA channels, based on the loaded image
      int numChannels = img.getBytesPerPixel();
      for (int c = 0; c < numChannels;
           c++)  // only set as many channels as are available in the loaded
                 // image; the rest get the default value
        pixelsRGBA[4 * (h * width + w) + c] = img.getPixel(w, h, c);
    }

  // bind the texture
  glBindTexture(GL_TEXTURE_2D, textureHandle);

  // initialize the texture
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, pixelsRGBA);

  // generate the mipmaps for this texture
  glGenerateMipmap(GL_TEXTURE_2D);

  // set the texture parameters
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                  GL_LINEAR_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  // query support for anisotropic texture filtering
  GLfloat fLargest;
  glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &fLargest);
  printf("Max available anisotropic samples: %f\n", fLargest);
  // set anisotropic texture filtering
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT,
                  0.5f * fLargest);

  // query for any errors
  GLenum errCode = glGetError();
  if (errCode != 0) {
    printf("Texture initialization error. Error code: %d.\n", errCode);
    return -1;
  }

  // de-allocate the pixel array -- it is no longer needed
  delete[] pixelsRGBA;

  return 0;
}

// write a screenshot to the specified filename
void saveScreenshot(const char *filename) {
  unsigned char *screenshotData =
      new unsigned char[windowWidth * windowHeight * 3];
  glReadPixels(0, 0, windowWidth, windowHeight, GL_RGB, GL_UNSIGNED_BYTE,
               screenshotData);

  ImageIO screenshotImg(windowWidth, windowHeight, 3, screenshotData);

  if (screenshotImg.save(filename, ImageIO::FORMAT_JPEG) == ImageIO::OK) {
    std::cout << "File " << filename << " saved successfully." << std::endl;
  } else {
    std::cout << "Failed to save file " << filename << '.' << std::endl;
  }

  delete[] screenshotData;
}

void timerFunc(int val) {
  if (val) {
    char *temp = new char[512 + strlen(windowTitle)];
    // Update title bar info
    sprintf(temp, "%s: %d fps , %d x %d resolution", windowTitle,
            frameCount * 30, windowWidth, windowHeight);
    glutSetWindowTitle(temp);
    delete[] temp;

    if (record) {  // take a screenshot
      temp = new char[8];
      sprintf(temp, "%03d.jpg", screenshotCount);
      saveScreenshot(temp);
      delete[] temp;
      ++screenshotCount;
    }
  }

  frameCount = 0;
  glutTimerFunc(33, timerFunc, 1);  //~30 fps
}

void reshapeFunc(int w, int h) {
  windowWidth = w;
  windowHeight = h;
  glViewport(0, 0, windowWidth, windowHeight);
  GLfloat aspect = (GLfloat)windowWidth / windowHeight;
  matrix->SetMatrixMode(OpenGLMatrix::Projection);
  matrix->LoadIdentity();
  matrix->Perspective(45.0, aspect, 0.01, 10000.0);
  matrix->SetMatrixMode(OpenGLMatrix::ModelView);  // By default in ModelView
}

void mouseMotionDragFunc(int x, int y) {
  // mouse has moved and one of the mouse buttons is pressed (dragging)

  // the change in mouse position since the last invocation of this function
  int mousePosDelta[2] = {x - mousePos[0], y - mousePos[1]};

  switch (controlState) {
    // translate the landscape
    case TRANSLATE:
      if (leftMouseButton) {
        // control x,y translation via the left mouse button
        landTranslate[0] += mousePosDelta[0] * 0.1;
        landTranslate[1] -= mousePosDelta[1] * 0.1;
      }
      if (middleMouseButton) {
        // control z translation via the middle mouse button
        landTranslate[2] += mousePosDelta[1];  // * 0.1;
      }
      break;

      // rotate the landscape
    case ROTATE:
      if (leftMouseButton) {
        // control x,y rotation via the left mouse button
        landRotate[0] += mousePosDelta[1];
        landRotate[1] += mousePosDelta[0];
      }
      if (middleMouseButton) {
        // control z rotation via the middle mouse button
        landRotate[2] += mousePosDelta[1];
      }
      break;

      // scale the landscape
    case SCALE:
      if (leftMouseButton) {
        // control x,y scaling via the left mouse button
        landScale[0] *= 1.0 + mousePosDelta[0] * 0.01;
        landScale[1] *= 1.0 - mousePosDelta[1] * 0.01;
      }
      if (middleMouseButton) {
        // control z scaling via the middle mouse button
        landScale[2] *= 1.0 - mousePosDelta[1] * 0.01;
      }
      break;
  }

  // store the new mouse position
  mousePos[0] = x;
  mousePos[1] = y;
}

void mouseMotionFunc(int x, int y) {
  // mouse has moved
  // store the new mouse position
  mousePos[0] = x;
  mousePos[1] = y;
}

void mouseButtonFunc(int button, int state, int x, int y) {
  // a mouse button has has been pressed or depressed

  // keep track of the mouse button state, in leftMouseButton,
  // middleMouseButton, rightMouseButton variables
  switch (button) {
    case GLUT_LEFT_BUTTON:
      leftMouseButton = (state == GLUT_DOWN);
      break;

    case GLUT_MIDDLE_BUTTON:
      middleMouseButton = (state == GLUT_DOWN);
      break;

    case GLUT_RIGHT_BUTTON:
      rightMouseButton = (state == GLUT_DOWN);
      break;
  }

  // keep track of whether CTRL and SHIFT keys are pressed
  switch (glutGetModifiers()) {
    case GLUT_ACTIVE_CTRL:
      controlState = TRANSLATE;
      break;

    case GLUT_ACTIVE_SHIFT:
      controlState = SCALE;
      break;

      // if CTRL and SHIFT are not pressed, we are in rotate mode
    default:
      controlState = ROTATE;
      break;
  }

  // store the new mouse position
  mousePos[0] = x;
  mousePos[1] = y;
}

void keyboardFunc(unsigned char key, int x, int y) {
  switch (key) {
    case 27:    // ESC key
      exit(0);  // exit the program
      break;

    case ' ':
      std::cout << "You pressed the spacebar." << std::endl;
      break;

      /*
  case 'x':
      // take a screenshot
      saveScreenshot("screenshot.jpg");
      break;
      */

    case 'x':
      // toggle video record
      if (record == 1) {
        screenshotCount = 0;
      }
      record = !record;
      break;
  }
}

void idleFunc() {
  // do some stuff...
  if (camStep < camera_path_vertices.positions.size()) {
    camPos = camera_path_vertices.positions[camStep] +
             camera_path_vertices.normals[camStep] * 2.0f;
    camDir = spline_vertices.tangents[camStep];
    camNorm = camera_path_vertices.normals[camStep];
    camBinorm = camera_path_vertices.binormals[camStep];
    camStep += 3;
  }

  // for example, here, you can save the screenshots to disk (to make the
  // animation)

  // make the screen update
  glutPostRedisplay();
}

void displayFunc() {
  ++frameCount;

  glClear(GL_COLOR_BUFFER_BIT |
          GL_DEPTH_BUFFER_BIT);  // Clear back and depth buffer

  matrix->LoadIdentity();
  matrix->LookAt(camPos[0], camPos[1], camPos[2], camPos[0] + camDir[0],
                 camPos[1] + camDir[1], camPos[2] + camDir[2], camNorm[0],
                 camNorm[1], camNorm[2]);

  /************ RAILS ********************/

  basicProgram->Bind();
  GLuint program = basicProgram->GetProgramHandle();

  float m[16];
  float p[16];
  matrix->GetMatrix(m);
  matrix->SetMatrixMode(OpenGLMatrix::Projection);
  matrix->GetMatrix(p);
  matrix->SetMatrixMode(OpenGLMatrix::ModelView);  // default matrix mode

  GLboolean isRowMajor = GL_FALSE;
  GLint h_modelViewMatrix = glGetUniformLocation(program, "modelViewMatrix");
  GLint h_projectionMatrix = glGetUniformLocation(program, "projectionMatrix");
  glUniformMatrix4fv(h_modelViewMatrix, 1, isRowMajor, m);
  glUniformMatrix4fv(h_projectionMatrix, 1, isRowMajor, p);

  glBindVertexArray(vaoRail);
  drawRails();
  glBindVertexArray(0);

  /************* CROSSBAR *****************/

  texProgram->Bind();
  program = basicProgram->GetProgramHandle();

  matrix->GetMatrix(m);
  matrix->SetMatrixMode(OpenGLMatrix::Projection);
  matrix->GetMatrix(p);
  matrix->SetMatrixMode(OpenGLMatrix::ModelView);  // default matrix mode

  isRowMajor = GL_FALSE;
  h_modelViewMatrix = glGetUniformLocation(program, "modelViewMatrix");
  h_projectionMatrix = glGetUniformLocation(program, "projectionMatrix");
  glUniformMatrix4fv(h_modelViewMatrix, 1, isRowMajor, m);
  glUniformMatrix4fv(h_projectionMatrix, 1, isRowMajor, p);

  glBindVertexArray(vaoCrossbar);
  glBindTexture(GL_TEXTURE_2D, texCrossbar);
  drawCrossbar();
  glBindVertexArray(0);

  /*********** GROUND TEXTURE *************/

  // Set up transformations
  matrix->PushMatrix();
  matrix->Translate(0.0, -groundRadius / 2.0, 0.0);
  matrix->Translate(-groundCenter.x, -groundCenter.y, -groundCenter.z);

  matrix->GetMatrix(m);
  matrix->SetMatrixMode(OpenGLMatrix::Projection);
  matrix->GetMatrix(p);
  matrix->SetMatrixMode(OpenGLMatrix::ModelView);  // default matrix mode
  matrix->PopMatrix();

  texProgram->Bind();
  program = texProgram->GetProgramHandle();

  h_modelViewMatrix = glGetUniformLocation(program, "modelViewMatrix");
  h_projectionMatrix = glGetUniformLocation(program, "projectionMatrix");

  isRowMajor = GL_FALSE;
  glUniformMatrix4fv(h_modelViewMatrix, 1, isRowMajor, m);
  glUniformMatrix4fv(h_projectionMatrix, 1, isRowMajor, p);

  glBindVertexArray(vaoGround);
  glBindTexture(GL_TEXTURE_2D, texGround);
  drawGround();
  glBindVertexArray(0);

  /********** SKY TEXTURE *************/

  // Set up transformations
  matrix->PushMatrix();
  matrix->Translate(-groundCenter.x, -groundCenter.y, -groundCenter.z);

  matrix->GetMatrix(m);
  matrix->SetMatrixMode(OpenGLMatrix::Projection);
  matrix->GetMatrix(p);
  matrix->SetMatrixMode(OpenGLMatrix::ModelView);  // default matrix mode
  matrix->PopMatrix();

  texProgram->Bind();
  program = texProgram->GetProgramHandle();

  h_modelViewMatrix = glGetUniformLocation(program, "modelViewMatrix");
  h_projectionMatrix = glGetUniformLocation(program, "projectionMatrix");

  isRowMajor = GL_FALSE;
  glUniformMatrix4fv(h_modelViewMatrix, 1, isRowMajor, m);
  glUniformMatrix4fv(h_projectionMatrix, 1, isRowMajor, p);

  glBindVertexArray(vaoSky);
  glBindTexture(GL_TEXTURE_2D, texSky);
  drawSky();
  glBindVertexArray(0);

  glutSwapBuffers();
}

void initBasicPipelineProgram() {
  basicProgram = new BasicPipelineProgram();
  basicProgram->Init("openGLHelper");
}

void initTexPipelineProgram() {
  texProgram = new TexPipelineProgram();
  texProgram->Init("openGLHelper");
}

void init(char *argv[]) {
  glClearColor(0.0, 0.0, 0.0, 0.0);
  glEnable(GL_DEPTH_TEST);

  matrix = new OpenGLMatrix();

  glGenTextures(1, &texGround);
  initTexture(argv[2], texGround);

  glGenTextures(1, &texSky);
  initTexture(argv[3], texSky);

  glGenTextures(1, &texCrossbar);
  initTexture(argv[4], texCrossbar);

  initBasicPipelineProgram();
  initTexPipelineProgram();

  // makeSpline();
  makeRails();
  makeCrossbars();
  makeGround();
  makeSky();
}

int main(int argc, char **argv) {
  if (argc < 5) {
    printf(
        "usage: %s <trackfile> <groundtexture> <skytexture> "
        "<crossbartexture>\n",
        argv[0]);
    exit(0);
  }

  std::cout << "Initializing GLUT..." << std::endl;
  glutInit(&argc, argv);

  std::cout << "Initializing OpenGL..." << std::endl;

#ifdef __APPLE__
  glutInitDisplayMode(GLUT_3_2_CORE_PROFILE | GLUT_DOUBLE | GLUT_RGB |
                      GLUT_DEPTH | GLUT_STENCIL);
#else
  glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH | GLUT_STENCIL);
#endif

  glutInitWindowSize(windowWidth, windowHeight);
  glutInitWindowPosition(0, 0);
  glutCreateWindow(windowTitle);

  std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;
  std::cout << "OpenGL Renderer: " << glGetString(GL_RENDERER) << std::endl;
  std::cout << "Shading Language Version: "
            << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;

  // tells glut to use a particular display function to redraw
  glutDisplayFunc(displayFunc);
  // perform animation inside idleFunc
  glutIdleFunc(idleFunc);
  // callback for mouse drags
  glutMotionFunc(mouseMotionDragFunc);
  // callback for idle mouse movement
  glutPassiveMotionFunc(mouseMotionFunc);
  // callback for mouse button changes
  glutMouseFunc(mouseButtonFunc);
  // callback for resizing the window
  glutReshapeFunc(reshapeFunc);
  // callback for pressing the keys on the keyboard
  glutKeyboardFunc(keyboardFunc);
  // callback for timer
  glutTimerFunc(0, timerFunc, 0);

// init glew
#ifdef __APPLE__
  // nothing is needed on Apple
#else
  // Windows, Linux
  GLint result = glewInit();
  if (result != GLEW_OK) {
    std::cout << "error: " << glewGetErrorString(result) << std::endl;
    exit(EXIT_FAILURE);
  }
#endif

  std::vector<std::vector<Point>> splines;
  Status status = LoadSplines(argv[1], &splines);
  if (status != kStatusOk) {
    std::fprintf(stderr, "Could not load splines.\n");
  }
  std::printf("Loaded spline count: %lu\n", splines.size());

  for (uint i = 0; i < splines.size(); ++i) {
    std::printf("Control point count in spline %u: %lu\n", i,
                splines[i].size());
  }

  EvalCatmullRomSpline(&splines[0], &spline_vertices);

  for (uint i = 0; i < Count(&spline_vertices); ++i) {
    UpdateMaxPoint(&spline_max_point, &spline_vertices.positions[i]);
    UpdateMinPoint(&spline_min_point, &spline_vertices.positions[i]);
  }
  updateBoundingSphere(spline_max_point, spline_min_point);

  MakeCameraPath(&spline_vertices, &camera_path_vertices);
  MakeRails(&camera_path_vertices, &rail_vertices);
  setupCrossbars(&camera_path_vertices, &spline_vertices.tangents);

  init(argv);

  // sink forever into the glut loop
  glutMainLoop();
}
