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

int windowWidth = 1280;
int windowHeight = 720;
const char *kWindowTitle = "Roller Coaster";

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

struct Vertex {
  glm::vec3 position;
  glm::vec4 color;
};

enum Model {
  kModelGround,
  kModelSky,
  kModelCrossbar,
  kModelRails,
  kModel_Count
};

enum Texture { kTextureGround, kTextureSky, kTextureCrossbar, kTexture_Count };

enum Vao { kVaoTextured, kVaoRails, kVao_Count };

static GLuint textures[kTexture_Count];
static GLuint vao_names[kVao_Count];
static GLuint vbo_names[kModel_Count];

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
std::vector<GLuint> rail_indices;

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
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, rail_indices.size() * sizeof(GLuint),
               rail_indices.data(), GL_STATIC_DRAW);

  glBindVertexArray(0);
}

void drawRails() {
  GLintptr offset = 0;
  glDrawElements(GL_TRIANGLES, rail_indices.size() / 2, GL_UNSIGNED_INT,
                 BUFFER_OFFSET(offset));
  offset += rail_indices.size() / 2 * sizeof(GLuint);
  glDrawElements(GL_TRIANGLES, rail_indices.size() / 2, GL_UNSIGNED_INT,
                 BUFFER_OFFSET(offset));
}

/**************** GROUND ********************/

GLuint vaoGround;
GLuint vboGround;

constexpr float kSceneBoxSideLen = 256;

const glm::vec3 ground_vertex_positions[6] = {
    {-kSceneBoxSideLen * 0.5f, 0, kSceneBoxSideLen * 0.5f},
    {-kSceneBoxSideLen * 0.5f, 0, -kSceneBoxSideLen * 0.5f},
    {kSceneBoxSideLen * 0.5f, 0, -kSceneBoxSideLen * 0.5f},
    {-kSceneBoxSideLen * 0.5f, 0, kSceneBoxSideLen * 0.5f},
    {kSceneBoxSideLen * 0.5f, 0, -kSceneBoxSideLen * 0.5f},
    {kSceneBoxSideLen * 0.5f, 0, kSceneBoxSideLen * 0.5f}};

constexpr float kGroundTexUpperLim = kSceneBoxSideLen * 0.5f * 0.25f;
const glm::vec2 ground_tex_coords[6] = {
    {0, 0},
    {0, kGroundTexUpperLim},
    {kGroundTexUpperLim, kGroundTexUpperLim},
    {0, 0},
    {kGroundTexUpperLim, kGroundTexUpperLim},
    {kGroundTexUpperLim, 0}};

// Bounding sphere
glm::vec3 groundCenter(0, 0, 0);
const GLfloat groundRadius = kSceneBoxSideLen * 0.5f;

void makeGround() {
  glGenVertexArrays(1, &vaoGround);
  glBindVertexArray(vaoGround);

  glGenBuffers(1, &vboGround);
  glBindBuffer(GL_ARRAY_BUFFER, vboGround);
  glBufferData(GL_ARRAY_BUFFER,
               sizeof(ground_vertex_positions) + sizeof(ground_tex_coords),
               NULL, GL_STATIC_DRAW);
  GLintptr offset = 0;
  glBufferSubData(GL_ARRAY_BUFFER, offset, sizeof(ground_vertex_positions),
                  ground_vertex_positions);
  offset += sizeof(ground_vertex_positions);
  glBufferSubData(GL_ARRAY_BUFFER, offset, sizeof(ground_tex_coords),
                  ground_tex_coords);

  GLuint program = texProgram->GetProgramHandle();
  GLuint posLoc = glGetAttribLocation(program, "position");
  GLuint texLoc = glGetAttribLocation(program, "texCoord");
  glEnableVertexAttribArray(posLoc);
  glEnableVertexAttribArray(texLoc);
  offset = 0;
  glVertexAttribPointer(posLoc, 3, GL_FLOAT, GL_FALSE, 0,
                        BUFFER_OFFSET(offset));
  offset += sizeof(ground_vertex_positions);
  glVertexAttribPointer(texLoc, 2, GL_FLOAT, GL_FALSE, 0,
                        BUFFER_OFFSET(offset));

  glBindVertexArray(0);
}

void drawGround() { glDrawArrays(GL_TRIANGLES, 0, 6); }

/*********************** SKY ****************************/

GLuint vaoSky;
GLuint vboSky;

const glm::vec3 sky_vertex_positions[36] = {
    // x = -1 face
    {-kSceneBoxSideLen * 0.5f, -kSceneBoxSideLen * 0.5f,
     -kSceneBoxSideLen * 0.5f},
    {-kSceneBoxSideLen * 0.5f, kSceneBoxSideLen * 0.5f,
     -kSceneBoxSideLen * 0.5f},
    {-kSceneBoxSideLen * 0.5f, kSceneBoxSideLen * 0.5f,
     kSceneBoxSideLen * 0.5f},
    {-kSceneBoxSideLen * 0.5f, -kSceneBoxSideLen * 0.5f,
     -kSceneBoxSideLen * 0.5f},
    {-kSceneBoxSideLen * 0.5f, kSceneBoxSideLen * 0.5f,
     kSceneBoxSideLen * 0.5f},
    {-kSceneBoxSideLen * 0.5f, -kSceneBoxSideLen * 0.5f,
     kSceneBoxSideLen * 0.5f},

    // x = 1 face
    {kSceneBoxSideLen * 0.5f, -kSceneBoxSideLen * 0.5f,
     -kSceneBoxSideLen * 0.5f},
    {kSceneBoxSideLen * 0.5f, kSceneBoxSideLen * 0.5f,
     -kSceneBoxSideLen * 0.5f},
    {kSceneBoxSideLen * 0.5f, kSceneBoxSideLen * 0.5f, kSceneBoxSideLen * 0.5f},
    {kSceneBoxSideLen * 0.5f, -kSceneBoxSideLen * 0.5f,
     -kSceneBoxSideLen * 0.5f},
    {kSceneBoxSideLen * 0.5f, kSceneBoxSideLen * 0.5f, kSceneBoxSideLen * 0.5f},
    {kSceneBoxSideLen * 0.5f, -kSceneBoxSideLen * 0.5f,
     kSceneBoxSideLen * 0.5f},

    // y = -1 face
    {-kSceneBoxSideLen * 0.5f, -kSceneBoxSideLen * 0.5f,
     -kSceneBoxSideLen * 0.5f},
    {-kSceneBoxSideLen * 0.5f, -kSceneBoxSideLen * 0.5f,
     kSceneBoxSideLen * 0.5f},
    {kSceneBoxSideLen * 0.5f, -kSceneBoxSideLen * 0.5f,
     kSceneBoxSideLen * 0.5f},
    {-kSceneBoxSideLen * 0.5f, -kSceneBoxSideLen * 0.5f,
     -kSceneBoxSideLen * 0.5f},
    {kSceneBoxSideLen * 0.5f, -kSceneBoxSideLen * 0.5f,
     kSceneBoxSideLen * 0.5f},
    {kSceneBoxSideLen * 0.5f, -kSceneBoxSideLen * 0.5f,
     -kSceneBoxSideLen * 0.5f},

    // y = 1 face
    {-kSceneBoxSideLen * 0.5f, kSceneBoxSideLen * 0.5f,
     -kSceneBoxSideLen * 0.5f},
    {-kSceneBoxSideLen * 0.5f, kSceneBoxSideLen * 0.5f,
     kSceneBoxSideLen * 0.5f},
    {kSceneBoxSideLen * 0.5f, kSceneBoxSideLen * 0.5f, kSceneBoxSideLen * 0.5f},
    {-kSceneBoxSideLen * 0.5f, kSceneBoxSideLen * 0.5f,
     -kSceneBoxSideLen * 0.5f},
    {kSceneBoxSideLen * 0.5f, kSceneBoxSideLen * 0.5f, kSceneBoxSideLen * 0.5f},
    {kSceneBoxSideLen * 0.5f, kSceneBoxSideLen * 0.5f,
     -kSceneBoxSideLen * 0.5f},

    // z = -1 face
    {-kSceneBoxSideLen * 0.5f, -kSceneBoxSideLen * 0.5f,
     -kSceneBoxSideLen * 0.5f},
    {-kSceneBoxSideLen * 0.5f, kSceneBoxSideLen * 0.5f,
     -kSceneBoxSideLen * 0.5f},
    {kSceneBoxSideLen * 0.5f, kSceneBoxSideLen * 0.5f,
     -kSceneBoxSideLen * 0.5f},
    {-kSceneBoxSideLen * 0.5f, -kSceneBoxSideLen * 0.5f,
     -kSceneBoxSideLen * 0.5f},
    {kSceneBoxSideLen * 0.5f, kSceneBoxSideLen * 0.5f,
     -kSceneBoxSideLen * 0.5f},
    {kSceneBoxSideLen * 0.5f, -kSceneBoxSideLen * 0.5f,
     -kSceneBoxSideLen * 0.5f},

    // z = 1 face
    {-kSceneBoxSideLen * 0.5f, -kSceneBoxSideLen * 0.5f,
     kSceneBoxSideLen * 0.5f},
    {-kSceneBoxSideLen * 0.5f, kSceneBoxSideLen * 0.5f,
     kSceneBoxSideLen * 0.5f},
    {kSceneBoxSideLen * 0.5f, kSceneBoxSideLen * 0.5f, kSceneBoxSideLen * 0.5f},
    {-kSceneBoxSideLen * 0.5f, -kSceneBoxSideLen * 0.5f,
     kSceneBoxSideLen * 0.5f},
    {kSceneBoxSideLen * 0.5f, kSceneBoxSideLen * 0.5f, kSceneBoxSideLen * 0.5f},
    {kSceneBoxSideLen * 0.5f, -kSceneBoxSideLen * 0.5f,
     kSceneBoxSideLen * 0.5f},
};

constexpr float kSkyTexUpperLim = 1;
const glm::vec2 sky_tex_coords[36] = {{0, 0},
                                      {0, kSkyTexUpperLim},
                                      {kSkyTexUpperLim, kSkyTexUpperLim},
                                      {0, 0},
                                      {kSkyTexUpperLim, kSkyTexUpperLim},
                                      {kSkyTexUpperLim, 0},
                                      {0, 0},
                                      {0, kSkyTexUpperLim},
                                      {kSkyTexUpperLim, kSkyTexUpperLim},
                                      {0, 0},
                                      {kSkyTexUpperLim, kSkyTexUpperLim},
                                      {kSkyTexUpperLim, 0},
                                      {0, 0},
                                      {0, kSkyTexUpperLim},
                                      {kSkyTexUpperLim, kSkyTexUpperLim},
                                      {0, 0},
                                      {kSkyTexUpperLim, kSkyTexUpperLim},
                                      {kSkyTexUpperLim, 0},
                                      {0, 0},
                                      {0, kSkyTexUpperLim},
                                      {kSkyTexUpperLim, kSkyTexUpperLim},
                                      {0, 0},
                                      {kSkyTexUpperLim, kSkyTexUpperLim},
                                      {kSkyTexUpperLim, 0},
                                      {0, 0},
                                      {0, kSkyTexUpperLim},
                                      {kSkyTexUpperLim, kSkyTexUpperLim},
                                      {0, 0},
                                      {kSkyTexUpperLim, kSkyTexUpperLim},
                                      {kSkyTexUpperLim, 0},
                                      {0, 0},
                                      {0, kSkyTexUpperLim},
                                      {kSkyTexUpperLim, kSkyTexUpperLim},
                                      {0, 0},
                                      {kSkyTexUpperLim, kSkyTexUpperLim},
                                      {kSkyTexUpperLim, 0}};

void makeSky() {
  glGenVertexArrays(1, &vaoSky);
  glBindVertexArray(vaoSky);

  glGenBuffers(1, &vboSky);
  glBindBuffer(GL_ARRAY_BUFFER, vboSky);
  glBufferData(GL_ARRAY_BUFFER,
               sizeof(sky_vertex_positions) + sizeof(sky_tex_coords), NULL,
               GL_STATIC_DRAW);
  GLintptr offset = 0;
  glBufferSubData(GL_ARRAY_BUFFER, offset, sizeof(sky_vertex_positions),
                  sky_vertex_positions);
  offset += sizeof(sky_vertex_positions);
  glBufferSubData(GL_ARRAY_BUFFER, offset, sizeof(sky_tex_coords),
                  sky_tex_coords);

  GLuint program = texProgram->GetProgramHandle();
  GLuint posLoc = glGetAttribLocation(program, "position");
  GLuint texLoc = glGetAttribLocation(program, "texCoord");
  glEnableVertexAttribArray(posLoc);
  glEnableVertexAttribArray(texLoc);
  offset = 0;
  glVertexAttribPointer(posLoc, 3, GL_FLOAT, GL_FALSE, 0,
                        BUFFER_OFFSET(offset));
  offset += sizeof(sky_vertex_positions);
  glVertexAttribPointer(texLoc, 2, GL_FLOAT, GL_FALSE, 0,
                        BUFFER_OFFSET(offset));

  glBindVertexArray(0);
}

void drawSky() { glDrawArrays(GL_TRIANGLES, 0, 36); }

/*********************** CROSSBARS ******************/

GLuint vaoCrossbar;
GLuint vboCrossbar;

std::vector<glm::vec3> crossbar_positions;
std::vector<glm::vec2> crossbar_tex_coords;

void makeCrossbars() {
  glGenVertexArrays(1, &vaoCrossbar);
  glBindVertexArray(vaoCrossbar);

  glGenBuffers(1, &vboCrossbar);
  glBindBuffer(GL_ARRAY_BUFFER, vboCrossbar);
  GLuint vSize = crossbar_positions.size() * sizeof(glm::vec3);
  GLuint tSize = crossbar_tex_coords.size() * sizeof(glm::vec2);
  glBufferData(GL_ARRAY_BUFFER, vSize + tSize, NULL, GL_STATIC_DRAW);
  GLintptr offset = 0;
  glBufferSubData(GL_ARRAY_BUFFER, offset, vSize, crossbar_positions.data());
  offset += vSize;
  glBufferSubData(GL_ARRAY_BUFFER, offset, tSize, crossbar_tex_coords.data());

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
  for (int offset = 0; offset < crossbar_positions.size(); offset += 36) {
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
                      std::vector<Vertex> *rail_vertices,
                      std::vector<GLuint> *rail_indices) {
  static constexpr float kAlpha = 0.1;
  static const glm::vec4 kRailColor(0.5, 0.5, 0.5, 1);
  static constexpr uint kVertexCount = 8;

  auto &cpv_pos = campath_vertices->positions;
  auto &cpv_binorm = campath_vertices->binormals;
  auto &cpv_norm = campath_vertices->normals;

  auto &rv = *rail_vertices;

  uint cpv_count = Count(campath_vertices);

  uint rv_count = 2 * cpv_count * kVertexCount;
  rv.resize(rv_count);

  for (uint i = 0; i < rv_count; ++i) {
    rv[i].color = kRailColor;
  }

  for (uint i = 0; i < 2; ++i) {
    for (uint j = 0; j < cpv_count; ++j) {
      uint k = kVertexCount * (j + i * cpv_count);
      rv[k].position =
          cpv_pos[j] + kAlpha * (-cpv_norm[j] + cpv_binorm[j] * 0.5f);
      rv[k + 1].position = cpv_pos[j] + kAlpha * (cpv_binorm[j] * 0.5f);
      rv[k + 2].position = cpv_pos[j] + kAlpha * (cpv_binorm[j]);
      rv[k + 3].position = cpv_pos[j] + kAlpha * (cpv_norm[j] + cpv_binorm[j]);
      rv[k + 4].position = cpv_pos[j] + kAlpha * (cpv_norm[j] - cpv_binorm[j]);
      rv[k + 5].position = cpv_pos[j] + kAlpha * (-cpv_binorm[j]);
      rv[k + 6].position = cpv_pos[j] + kAlpha * (-cpv_binorm[j] * 0.5f);
      rv[k + 7].position =
          cpv_pos[j] + kAlpha * (-cpv_norm[j] - cpv_binorm[j] * 0.5f);

      for (uint l = 0; l < kVertexCount; ++l) {
        if (i == 0) {
          rv[k + l].position += cpv_binorm[j];
        } else {
          rv[k + l].position -= cpv_binorm[j];
        }
      }
    }
  }

  auto &ri = *rail_indices;

  for (uint i = 0; i < 2; ++i) {
    for (uint j = i * rv_count / 2; j + kVertexCount < rv_count / (2 - i);
         j += kVertexCount) {
      // top face
      ri.push_back(j + 4);
      ri.push_back(j + 12);
      ri.push_back(j + 3);
      ri.push_back(j + 12);
      ri.push_back(j + 11);
      ri.push_back(j + 3);

      // top right right face
      ri.push_back(j + 3);
      ri.push_back(j + 11);
      ri.push_back(j + 2);
      ri.push_back(j + 11);
      ri.push_back(j + 10);
      ri.push_back(j + 2);

      // top right bottom face
      ri.push_back(j + 2);
      ri.push_back(j + 10);
      ri.push_back(j + 1);
      ri.push_back(j + 10);
      ri.push_back(j + 9);
      ri.push_back(j + 1);

      // bottom right face
      ri.push_back(j + 1);
      ri.push_back(j + 9);
      ri.push_back(j);
      ri.push_back(j + 9);
      ri.push_back(j + 8);
      ri.push_back(j);

      // bottom face
      ri.push_back(j);
      ri.push_back(j + 8);
      ri.push_back(j + 7);
      ri.push_back(j + 8);
      ri.push_back(j + 15);
      ri.push_back(j + 7);

      // bottom left face
      ri.push_back(j + 14);
      ri.push_back(j + 6);
      ri.push_back(j + 15);
      ri.push_back(j + 6);
      ri.push_back(j + 7);
      ri.push_back(j + 15);

      // top left bottom face
      ri.push_back(j + 13);
      ri.push_back(j + 5);
      ri.push_back(j + 14);
      ri.push_back(j + 5);
      ri.push_back(j + 6);
      ri.push_back(j + 14);

      // top left left face
      ri.push_back(j + 12);
      ri.push_back(j + 4);
      ri.push_back(j + 13);
      ri.push_back(j + 4);
      ri.push_back(j + 5);
      ri.push_back(j + 13);
    }
  }
}

static void MakeCrossbars(const CameraPathVertices *cam_path,
                          const std::vector<glm::vec3> *spline_tangents,
                          std::vector<glm::vec3> *crossbar_positions,
                          std::vector<glm::vec2> *crossbar_tex_coords) {
  static constexpr float kAlpha = 0.1;
  static constexpr float kBeta = 1.5;
  static constexpr int kVertexCount = 8;
  static constexpr float kBarToBarDist = 1.0;
  static constexpr float kBarDepth = 0.3;

  const auto &s_tan = *spline_tangents;

  auto &cp_pos = cam_path->positions;
  auto &cp_binorm = cam_path->binormals;
  auto &cp_norm = cam_path->normals;
  uint cp_count = Count(cam_path);

  auto &cb_pos = *crossbar_positions;
  auto &cb_tex_coords = *crossbar_tex_coords;

  glm::vec3 v[kVertexCount];
  float dist_moved = 0;
  for (uint j = 1; j < cp_count; ++j) {
    dist_moved += glm::length(cp_pos[j] - cp_pos[j - 1]);

    if (dist_moved <= kBarToBarDist) {
      continue;
    }

    v[0] = cp_pos[j] + kAlpha * (-kBeta * cp_norm[j] + cp_binorm[j] * 0.5f) +
           cp_binorm[j];
    v[1] =
        cp_pos[j] + kAlpha * (-cp_norm[j] + cp_binorm[j] * 0.5f) + cp_binorm[j];
    v[2] = cp_pos[j] + kAlpha * (-kBeta * cp_norm[j] + cp_binorm[j] * 0.5f) -
           cp_binorm[j] - kAlpha * cp_binorm[j];
    v[3] = cp_pos[j] + kAlpha * (-cp_norm[j] + cp_binorm[j] * 0.5f) -
           cp_binorm[j] - kAlpha * cp_binorm[j];

    v[4] = cp_pos[j] + kAlpha * (-kBeta * cp_norm[j] + cp_binorm[j] * 0.5f) +
           cp_binorm[j] + kBarDepth * s_tan[j];
    v[5] = cp_pos[j] + kAlpha * (-cp_norm[j] + cp_binorm[j] * 0.5f) +
           cp_binorm[j] + kBarDepth * s_tan[j];
    v[6] = cp_pos[j] + kAlpha * (-kBeta * cp_norm[j] + cp_binorm[j] * 0.5f) -
           cp_binorm[j] + kBarDepth * s_tan[j] - kAlpha * cp_binorm[j];
    v[7] = cp_pos[j] + kAlpha * (-cp_norm[j] + cp_binorm[j] * 0.5f) -
           cp_binorm[j] + kBarDepth * s_tan[j] - kAlpha * cp_binorm[j];

    // top face
    cb_pos.push_back(v[6]);
    cb_pos.push_back(v[5]);
    cb_pos.push_back(v[2]);
    cb_pos.push_back(v[5]);
    cb_pos.push_back(v[1]);
    cb_pos.push_back(v[2]);

    // right face
    cb_pos.push_back(v[5]);
    cb_pos.push_back(v[4]);
    cb_pos.push_back(v[1]);
    cb_pos.push_back(v[4]);
    cb_pos.push_back(v[0]);
    cb_pos.push_back(v[1]);

    // bottom face
    cb_pos.push_back(v[4]);
    cb_pos.push_back(v[7]);
    cb_pos.push_back(v[0]);
    cb_pos.push_back(v[7]);
    cb_pos.push_back(v[3]);
    cb_pos.push_back(v[0]);

    // left face
    cb_pos.push_back(v[7]);
    cb_pos.push_back(v[6]);
    cb_pos.push_back(v[3]);
    cb_pos.push_back(v[6]);
    cb_pos.push_back(v[2]);
    cb_pos.push_back(v[3]);

    // back face
    cb_pos.push_back(v[5]);
    cb_pos.push_back(v[6]);
    cb_pos.push_back(v[4]);
    cb_pos.push_back(v[6]);
    cb_pos.push_back(v[7]);
    cb_pos.push_back(v[4]);

    // front face
    cb_pos.push_back(v[2]);
    cb_pos.push_back(v[1]);
    cb_pos.push_back(v[3]);
    cb_pos.push_back(v[1]);
    cb_pos.push_back(v[0]);
    cb_pos.push_back(v[3]);

    for (uint i = 0; i < 6; ++i) {
      cb_tex_coords.emplace_back(0, 1);
      cb_tex_coords.emplace_back(1, 1);
      cb_tex_coords.emplace_back(0, 0);
      cb_tex_coords.emplace_back(1, 1);
      cb_tex_coords.emplace_back(1, 0);
      cb_tex_coords.emplace_back(0, 0);
    }

    dist_moved = 0;
  }
}

static Status LoadSplines(const char *track_filepath,
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

int InitTexture(const char *img_filepath, GLuint texture_name) {
  // read the texture image
  ImageIO img;
  ImageIO::fileFormatType img_format;
  ImageIO::errorType err = img.load(img_filepath, &img_format);

  if (err != ImageIO::OK) {
    std::printf("Loading texture from %s failed.\n", img_filepath);
    return -1;
  }

  // check that the number of bytes is a multiple of 4
  if (img.getWidth() * img.getBytesPerPixel() % 4) {
    std::fprintf(
        stderr,
        "Error (%s): The width*numChannels in the loaded image must be a "
        "multiple of 4.\n",
        img_filepath);
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

  glBindTexture(GL_TEXTURE_2D, texture_name);

  // initialize the texture
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, pixelsRGBA);

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
    char *temp = new char[512 + strlen(kWindowTitle)];
    // Update title bar info
    sprintf(temp, "%s: %d fps , %d x %d resolution", kWindowTitle,
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

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  matrix->LoadIdentity();
  matrix->LookAt(camPos[0], camPos[1], camPos[2], camPos[0] + camDir[0],
                 camPos[1] + camDir[1], camPos[2] + camDir[2], camNorm[0],
                 camNorm[1], camNorm[2]);

  float model_view_mat[16];
  float proj_mat[16];
  GLboolean is_row_major = GL_FALSE;

  /* Rails */

  GLuint prog = basicProgram->GetProgramHandle();
  GLint model_view_mat_loc = glGetUniformLocation(prog, "modelViewMatrix");
  GLint proj_mat_loc = glGetUniformLocation(prog, "projectionMatrix");

  glUseProgram(prog);

  matrix->GetMatrix(model_view_mat);
  matrix->SetMatrixMode(OpenGLMatrix::Projection);
  matrix->GetMatrix(proj_mat);
  matrix->SetMatrixMode(OpenGLMatrix::ModelView);

  glUniformMatrix4fv(model_view_mat_loc, 1, is_row_major, model_view_mat);
  glUniformMatrix4fv(proj_mat_loc, 1, is_row_major, proj_mat);

  glBindVertexArray(vaoRail);
  drawRails();
  glBindVertexArray(0);

  /* Textured models */

  prog = texProgram->GetProgramHandle();
  model_view_mat_loc = glGetUniformLocation(prog, "modelViewMatrix");
  proj_mat_loc = glGetUniformLocation(prog, "projectionMatrix");

  glUseProgram(prog);

  /* Crossbars */

  matrix->GetMatrix(model_view_mat);
  matrix->SetMatrixMode(OpenGLMatrix::Projection);
  matrix->GetMatrix(proj_mat);
  matrix->SetMatrixMode(OpenGLMatrix::ModelView);  // default matrix mode

  glUniformMatrix4fv(model_view_mat_loc, 1, is_row_major, model_view_mat);
  glUniformMatrix4fv(proj_mat_loc, 1, is_row_major, proj_mat);

  glBindVertexArray(vaoCrossbar);
  glBindTexture(GL_TEXTURE_2D, textures[kTextureCrossbar]);
  drawCrossbar();
  glBindVertexArray(0);

  /* Ground textures */

  // Set up transformations
  matrix->PushMatrix();
  matrix->Translate(0.0, -groundRadius / 2.0, 0.0);
  matrix->Translate(-groundCenter.x, -groundCenter.y, -groundCenter.z);

  matrix->GetMatrix(model_view_mat);
  matrix->SetMatrixMode(OpenGLMatrix::Projection);
  matrix->GetMatrix(proj_mat);
  matrix->SetMatrixMode(OpenGLMatrix::ModelView);  // default matrix mode
  matrix->PopMatrix();

  glUniformMatrix4fv(model_view_mat_loc, 1, is_row_major, model_view_mat);
  glUniformMatrix4fv(proj_mat_loc, 1, is_row_major, proj_mat);

  glBindVertexArray(vaoGround);
  glBindTexture(GL_TEXTURE_2D, textures[kTextureGround]);
  drawGround();
  glBindVertexArray(0);

  /* Sky */

  // Set up transformations
  matrix->PushMatrix();
  matrix->Translate(-groundCenter.x, -groundCenter.y, -groundCenter.z);

  matrix->GetMatrix(model_view_mat);
  matrix->SetMatrixMode(OpenGLMatrix::Projection);
  matrix->GetMatrix(proj_mat);
  matrix->SetMatrixMode(OpenGLMatrix::ModelView);  // default matrix mode
  matrix->PopMatrix();

  glUniformMatrix4fv(model_view_mat_loc, 1, is_row_major, model_view_mat);
  glUniformMatrix4fv(proj_mat_loc, 1, is_row_major, proj_mat);

  glBindVertexArray(vaoSky);
  glBindTexture(GL_TEXTURE_2D, textures[kTextureSky]);
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

int main(int argc, char **argv) {
  if (argc < 5) {
    std::fprintf(stderr,
                 "usage: %s <track-file> <ground-texture> <sky-texture> "
                 "<crossbar-texture>\n",
                 argv[0]);
    return EXIT_FAILURE;
  }

  glutInit(&argc, argv);

#ifdef __APPLE__
  glutInitDisplayMode(GLUT_3_2_CORE_PROFILE | GLUT_DOUBLE | GLUT_RGB |
                      GLUT_DEPTH | GLUT_STENCIL);
#else
  glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH | GLUT_STENCIL);
#endif

  glutInitWindowSize(windowWidth, windowHeight);
  glutInitWindowPosition(0, 0);
  glutCreateWindow(kWindowTitle);

  std::printf("OpenGL Version: %s\n", glGetString(GL_VERSION));
  std::printf("OpenGL Renderer: %s\n", glGetString(GL_RENDERER));
  std::printf("OpenGL Shading Language Version: %s\n",
              glGetString(GL_SHADING_LANGUAGE_VERSION));

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
  MakeRails(&camera_path_vertices, &rail_vertices, &rail_indices);
  MakeCrossbars(&camera_path_vertices, &spline_vertices.tangents,
                &crossbar_positions, &crossbar_tex_coords);

  glClearColor(0, 0, 0, 0);
  glEnable(GL_DEPTH_TEST);

  matrix = new OpenGLMatrix();

  glGenTextures(kTexture_Count, textures);

  InitTexture(argv[2], textures[kTextureGround]);
  InitTexture(argv[3], textures[kTextureSky]);
  InitTexture(argv[4], textures[kTextureCrossbar]);

  initBasicPipelineProgram();
  initTexPipelineProgram();

  // glGenBuffers(kModel_Count, vbo_names);
  // glGenVertexArrays(kVao_Count, vao_names);

  // makeSpline();
  makeRails();
  makeCrossbars();
  makeGround();
  makeSky();

  // sink forever into the glut loop
  glutMainLoop();
}
