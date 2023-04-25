#include <glm/glm.hpp>
#include <iomanip>
#include <iostream>
#include <vector>

#include "basicPipelineProgram.h"
#include "glutHeader.h"
#include "imageIO.h"
#include "openGLHeader.h"
#include "openGLMatrix.h"
#include "texPipelineProgram.h"

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
std::vector<glm::vec3> camPositions;
std::vector<glm::vec3> camTangents;
std::vector<glm::vec3> camNormals;
std::vector<glm::vec3> camBinormals;

// represents one control point along the spline
struct Point {
  double x;
  double y;
  double z;
};

// spline struct
// contains how many control points the spline has, and an array of control
// points
struct Spline {
  int numControlPoints;
  Point *points;
};

// the spline array
Spline *splines;
// total number of splines
int numSplines;

// Vertex container
struct Vertex {
  glm::vec3 position;
  glm::vec4 color;
};

/************************ SPLINE **********************/

GLuint vaoSpline;
GLuint vboSpline;

std::vector<Vertex> splineVertices;

void makeSpline() {
  glGenVertexArrays(1, &vaoSpline);
  glBindVertexArray(vaoSpline);

  glGenBuffers(1, &vboSpline);
  glBindBuffer(GL_ARRAY_BUFFER, vboSpline);
  glBufferData(GL_ARRAY_BUFFER, splineVertices.size() * sizeof(Vertex),
               &splineVertices[0], GL_STATIC_DRAW);

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

  glBindVertexArray(0);
}

void drawSpline() { glDrawArrays(GL_LINES, 0, splineVertices.size()); }

/********************** RAILS **********************/

GLuint vaoRail;
GLuint vboRail;
GLuint iboRail;

std::vector<Vertex> railVertices;
std::vector<GLuint> railIndices;

void makeRails() {
  glGenVertexArrays(1, &vaoRail);
  glBindVertexArray(vaoRail);

  glGenBuffers(1, &vboRail);
  glBindBuffer(GL_ARRAY_BUFFER, vboRail);
  glBufferData(GL_ARRAY_BUFFER, railVertices.size() * sizeof(Vertex),
               &railVertices[0], GL_STATIC_DRAW);

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

// Basis matrix
const GLfloat s = 0.5;
const glm::mat4x4 basisMatrix(glm::vec4(-s, 2 - s, s - 2, s),
                              glm::vec4(2 * s, s - 3, 3 - 2 * s, -s),
                              glm::vec4(-s, 0.0, s, 0.0),
                              glm::vec4(0, 1.0, 0.0, 0.0));

// Bounding sphere of spline
glm::vec3 splineCenter;
GLfloat splineRadius;
glm::vec3 splineMinPoint(0.0, 0.0, 0.0);
glm::vec3 splineMaxPoint(0.0, 0.0, 0.0);

const GLfloat tolerance = 0.0001;

glm::vec3 catmullRom(GLfloat u, glm::mat4x3 *controlMatrix) {
  glm::vec4 parameterVector(u * u * u, u * u, u, 1.0);
  return (*controlMatrix) * basisMatrix * parameterVector;
}

glm::vec3 tangentCatmullRom(GLfloat u, glm::mat4x3 *controlMatrix) {
  glm::vec4 parameterVector(3 * u * u, 2 * u, 1.0, 0.0);
  return (*controlMatrix) * basisMatrix * parameterVector;
}

void updateMaxMinPoints(glm::vec3 p) {
  static bool firstCall = true;
  if (firstCall) {
    splineMinPoint = p;
    splineMaxPoint = p;
    firstCall = false;
  } else {
    if (p.x < splineMinPoint.x + tolerance) {
      splineMinPoint.x = p.x;
    }
    if (p.x > splineMaxPoint.x + tolerance) {
      splineMaxPoint.x = p.x;
    }
    if (p.y < splineMinPoint.y + tolerance) {
      splineMinPoint.y = p.y;
    }
    if (p.y > splineMaxPoint.y + tolerance) {
      splineMaxPoint.y = p.y;
    }
    if (p.z < splineMinPoint.z + tolerance) {
      splineMinPoint.y = p.z;
    }
    if (p.z > splineMaxPoint.z + tolerance) {
      splineMaxPoint.y = p.z;
    }
  }
}

void updateBoundingSphere(glm::vec3 splineMaxPoint, glm::vec3 splineMinPoint) {
  splineCenter = glm::vec3((splineMaxPoint.x + splineMinPoint.x) / 2.0,
                           (splineMaxPoint.y + splineMinPoint.y) / 2.0,
                           (splineMaxPoint.z + splineMinPoint.z) / 2.0);
  splineRadius = sqrt(pow(splineMaxPoint.x - splineCenter.x, 2) +
                      pow(splineMaxPoint.y - splineCenter.y, 2) +
                      pow(splineMaxPoint.z - splineCenter.z, 2));
}

void subdivide(GLfloat u0, GLfloat u1, GLfloat maxLineLength,
               glm::mat4x3 *controlMatrix) {
  const static glm::vec4 vColor(1.0, 0.0, 0.0, 1.0);

  // Points
  glm::vec3 p0 = catmullRom(u0, controlMatrix);
  glm::vec3 p1 = catmullRom(u1, controlMatrix);

  if (glm::length(p1 - p0) > maxLineLength) {
    GLfloat umid = (u0 + u1) / 2.0;
    subdivide(u0, umid, maxLineLength, controlMatrix);
    subdivide(umid, u1, maxLineLength, controlMatrix);
  } else {
    // Vertices
    Vertex v0 = {p0, vColor};
    Vertex v1 = {p1, vColor};

    // Tangents
    glm::vec3 t0 = tangentCatmullRom(u0, controlMatrix);
    glm::vec3 t1 = tangentCatmullRom(u1, controlMatrix);

    updateMaxMinPoints(p0);
    updateMaxMinPoints(p1);

    splineVertices.push_back(v0);
    splineVertices.push_back(v1);
    camTangents.push_back(glm::normalize(t0));
    camTangents.push_back(glm::normalize(t1));
  }
}

void setupCamPath() {
  int camInd = 0;
  for (int i = 0; i < splineVertices.size(); i += 1) {
    camPositions.push_back(splineVertices[i].position);
    camPositions[camInd] -= splineCenter;
    camPositions[camInd].y += splineRadius - groundRadius / 2.0;
    if (camInd == 0) {
      camNormals.push_back(glm::normalize(
          glm::cross(camTangents[i], glm::vec3(0.0, 1.0, -0.5))));
      camBinormals.push_back(
          glm::normalize(glm::cross(camTangents[i], camNormals[camInd])));
    } else {
      camNormals.push_back(
          glm::normalize(glm::cross(camBinormals[camInd - 1], camTangents[i])));
      camBinormals.push_back(
          glm::normalize(glm::cross(camTangents[i], camNormals[camInd])));
    }
    ++camInd;
  }
}

void setupRails() {
  const static float alpha = 0.1;
  const static glm::vec4 railColor(0.5, 0.5, 0.5, 1.0);
  const static int num_vertices = 8;
  Vertex v[num_vertices];
  for (int i = 0; i < num_vertices; ++i) {
    v[i].color = railColor;
  }
  for (int i = 0; i < 2; ++i) {
    for (int j = 0; j < camPositions.size(); ++j) {
      v[0].position =
          camPositions[j] + alpha * (-camNormals[j] + camBinormals[j] * 0.5f);
      v[1].position = camPositions[j] + alpha * (camBinormals[j] * 0.5f);
      v[2].position = camPositions[j] + alpha * (camBinormals[j]);
      v[3].position =
          camPositions[j] + alpha * (camNormals[j] + camBinormals[j]);
      v[4].position =
          camPositions[j] + alpha * (camNormals[j] - camBinormals[j]);
      v[5].position = camPositions[j] + alpha * (-camBinormals[j]);
      v[6].position = camPositions[j] + alpha * (-camBinormals[j] * 0.5f);
      v[7].position =
          camPositions[j] + alpha * (-camNormals[j] - camBinormals[j] * 0.5f);

      for (int k = 0; k < num_vertices; ++k) {
        if (i == 0) {
          v[k].position += camBinormals[j];
        } else {
          v[k].position -= camBinormals[j];
        }
        railVertices.push_back(v[k]);
      }
    }
  }

  for (int i = 0; i < 2; ++i) {
    for (int j = i * railVertices.size() / 2;
         j < railVertices.size() / (2 - i) - num_vertices; j += num_vertices) {
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

void setupCrossbars() {
  const static float alpha = 0.1;
  const static float beta = 1.5;
  const static int num_vertices = 8;
  const static float bar2barDist = 1.0;
  const static float barDepth = 0.3;
  float distMoved = 0.0;
  glm::vec3 v[num_vertices];
  for (int j = 1; j < camPositions.size(); ++j) {
    distMoved += glm::length(camPositions[j] - camPositions[j - 1]);
    if (distMoved > bar2barDist) {
      v[0] = camPositions[j] +
             alpha * (-beta * camNormals[j] + camBinormals[j] * 0.5f) +
             camBinormals[j];
      v[1] = camPositions[j] +
             alpha * (-camNormals[j] + camBinormals[j] * 0.5f) +
             camBinormals[j];
      v[2] = camPositions[j] +
             alpha * (-beta * camNormals[j] + camBinormals[j] * 0.5f) -
             camBinormals[j] - alpha * camBinormals[j];
      v[3] = camPositions[j] +
             alpha * (-camNormals[j] + camBinormals[j] * 0.5f) -
             camBinormals[j] - alpha * camBinormals[j];

      v[4] = camPositions[j] +
             alpha * (-beta * camNormals[j] + camBinormals[j] * 0.5f) +
             camBinormals[j] + barDepth * camTangents[j];
      v[5] = camPositions[j] +
             alpha * (-camNormals[j] + camBinormals[j] * 0.5f) +
             camBinormals[j] + barDepth * camTangents[j];
      v[6] = camPositions[j] +
             alpha * (-beta * camNormals[j] + camBinormals[j] * 0.5f) -
             camBinormals[j] + barDepth * camTangents[j] -
             alpha * camBinormals[j];
      v[7] =
          camPositions[j] + alpha * (-camNormals[j] + camBinormals[j] * 0.5f) -
          camBinormals[j] + barDepth * camTangents[j] - alpha * camBinormals[j];

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

void catmullRomSpline(Spline *spline) {
  const static float maxLineLength = 0.5;
  glm::vec3 controlRows[4];
  glm::mat4x3 controlMatrix;
  for (int i = 1; i < spline->numControlPoints - 2; ++i) {
    controlRows[0] = glm::vec3(spline->points[i - 1].x, spline->points[i - 1].y,
                               spline->points[i - 1].z);
    controlRows[1] = glm::vec3(spline->points[i].x, spline->points[i].y,
                               spline->points[i].z);
    controlRows[2] = glm::vec3(spline->points[i + 1].x, spline->points[i + 1].y,
                               spline->points[i + 1].z);
    controlRows[3] = glm::vec3(spline->points[i + 2].x, spline->points[i + 2].y,
                               spline->points[i + 2].z);
    controlMatrix = glm::mat4x3(controlRows[0], controlRows[1], controlRows[2],
                                controlRows[3]);

    subdivide(0.0, 1.0, maxLineLength, &controlMatrix);
  }

  updateBoundingSphere(splineMaxPoint, splineMinPoint);

  setupCamPath();
  setupRails();
  setupCrossbars();
}

int loadSplines(char *argv) {
  char *cName = (char *)malloc(128 * sizeof(char));
  FILE *fileList;
  FILE *fileSpline;
  int iType, i = 0, j, iLength;

  // load the track file
  fileList = fopen(argv, "r");
  if (fileList == NULL) {
    printf("can't open file\n");
    exit(1);
  }

  // stores the number of splines in a global variable
  fscanf(fileList, "%d", &numSplines);

  splines = (Spline *)malloc(numSplines * sizeof(Spline));

  // reads through the spline files
  for (j = 0; j < numSplines; j++) {
    i = 0;
    fscanf(fileList, "%s", cName);
    fileSpline = fopen(cName, "r");

    if (fileSpline == NULL) {
      printf("can't open file\n");
      exit(1);
    }

    // gets length for spline file
    fscanf(fileSpline, "%d %d", &iLength, &iType);

    // allocate memory for all the points
    splines[j].points = (Point *)malloc(iLength * sizeof(Point));
    splines[j].numControlPoints = iLength;

    // saves the data to the struct
    while (fscanf(fileSpline, "%lf %lf %lf", &splines[j].points[i].x,
                  &splines[j].points[i].y, &splines[j].points[i].z) != EOF) {
      i++;
    }
  }

  free(cName);

  return 0;
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
  if (camStep < camPositions.size()) {
    camPos = camPositions[camStep] + camNormals[camStep] * 2.0f;
    camDir = camTangents[camStep];
    camNorm = camNormals[camStep];
    camBinorm = camBinormals[camStep];
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

void init(int argc, char *argv[]) {
  glClearColor(0.0, 0.0, 0.0, 0.0);
  glEnable(GL_DEPTH_TEST);

  matrix = new OpenGLMatrix();

  loadSplines(argv[1]);
  printf("Loaded %d spline(s).\n", numSplines);
  for (int i = 0; i < numSplines; i++) {
    printf("Num control points in spline %d: %d.\n", i,
           splines[i].numControlPoints);
  }

  catmullRomSpline(splines);

  glGenTextures(1, &texGround);
  initTexture(argv[2], texGround);

  glGenTextures(1, &texSky);
  initTexture(argv[3], texSky);

  glGenTextures(1, &texCrossbar);
  initTexture(argv[4], texCrossbar);

  initBasicPipelineProgram();
  initTexPipelineProgram();

  makeSpline();
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

  // do initialization
  init(argc, argv);

  // sink forever into the glut loop
  glutMainLoop();
}
