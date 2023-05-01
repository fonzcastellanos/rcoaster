#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <glm/glm.hpp>
#include <iomanip>
#include <iostream>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "aabb.hpp"
#include "basicPipelineProgram.h"
#include "glutHeader.h"
#include "openGLHeader.h"
#include "openGLMatrix.h"
#include "spline.hpp"
#include "stb_image.h"
#include "stb_image_write.h"
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
uint camStep = 0;
glm::vec3 camPos(0.0, 0.0, 0.0);
glm::vec3 camDir(0.0, 0.0, 0.0);
glm::vec3 camNorm;
glm::vec3 camBinorm;

struct Vertex {
  glm::vec3 position;
  glm::vec4 color;
};

struct CameraPathVertices {
  std::vector<glm::vec3> positions;
  std::vector<glm::vec3> normals;
  std::vector<glm::vec3> binormals;
};

static uint Count(const CameraPathVertices *c) {
  assert(c);
  assert(c->positions.size() == c->normals.size());
  assert(c->normals.size() == c->binormals.size());

  return c->positions.size();
}

struct TexturedVertices {
  std::vector<glm::vec3> positions;
  std::vector<glm::vec2> tex_coords;
};

static uint Count(const TexturedVertices *tv) {
  assert(tv);
  assert(tv->positions.size() == tv->tex_coords.size());

  return tv->positions.size();
}

enum Texture { kTextureGround, kTextureSky, kTextureCrossbar, kTexture_Count };

enum VertexFormat {
  kVertexFormatTextured,
  kVertexFormatUntextured,
  kVertexFormat_Count
};

enum Vbo {
  kVboTexturedVertices,
  kVboUntexturedVertices,
  kVboRailIndices,
  kVbo_Count
};

static CameraPathVertices camera_path_vertices;
static std::vector<GLuint> rail_indices;
static TexturedVertices crossbar_vertices;
static SplineVertices spline_vertices;

static GLuint textures[kTexture_Count];
static GLuint vao_names[kVertexFormat_Count];
static GLuint vbo_names[kVbo_Count];

#define RAIL_COLOR_RED 0.5
#define RAIL_COLOR_BLUE 0.5
#define RAIL_COLOR_GREEN 0.5
#define RAIL_COLOR_ALPHA 1
#define RAIL_HEAD_W 0.2
#define RAIL_HEAD_H 0.1
#define RAIL_WEB_W 0.1
#define RAIL_WEB_H 0.1
#define RAIL_GAUGE 2

#define SCENE_AABB_SIDE_LEN 256
#define GROUND_VERTEX_COUNT 6
#define SKY_VERTEX_COUNT 36

const glm::vec3 scene_aabb_center = {};
const glm::vec3 scene_aabb_size(SCENE_AABB_SIDE_LEN);

static void MakeGround(TexturedVertices *ground) {
  assert(ground);

  static constexpr float kTexUpperLim = SCENE_AABB_SIDE_LEN * 0.5f * 0.25f;

  ground->positions = {{-SCENE_AABB_SIDE_LEN, 0, SCENE_AABB_SIDE_LEN},
                       {-SCENE_AABB_SIDE_LEN, 0, -SCENE_AABB_SIDE_LEN},
                       {SCENE_AABB_SIDE_LEN, 0, -SCENE_AABB_SIDE_LEN},
                       {-SCENE_AABB_SIDE_LEN, 0, SCENE_AABB_SIDE_LEN},
                       {SCENE_AABB_SIDE_LEN, 0, -SCENE_AABB_SIDE_LEN},
                       {SCENE_AABB_SIDE_LEN, 0, SCENE_AABB_SIDE_LEN}};

  ground->tex_coords = {{0, 0},
                        {0, kTexUpperLim},
                        {kTexUpperLim, kTexUpperLim},
                        {0, 0},
                        {kTexUpperLim, kTexUpperLim},
                        {kTexUpperLim, 0}};

  assert(GROUND_VERTEX_COUNT == ground->positions.size());
  assert(GROUND_VERTEX_COUNT == ground->tex_coords.size());

  for (uint i = 0; i < Count(ground); ++i) {
    ground->positions[i] *= 0.5f;
  }
}

static void MakeSky(TexturedVertices *sky) {
  assert(sky);

  static constexpr float kTexUpperLim = 1;

  sky->positions = {
      // x = -1 face
      {-SCENE_AABB_SIDE_LEN, -SCENE_AABB_SIDE_LEN, -SCENE_AABB_SIDE_LEN},
      {-SCENE_AABB_SIDE_LEN, SCENE_AABB_SIDE_LEN, -SCENE_AABB_SIDE_LEN},
      {-SCENE_AABB_SIDE_LEN, SCENE_AABB_SIDE_LEN, SCENE_AABB_SIDE_LEN},
      {-SCENE_AABB_SIDE_LEN, -SCENE_AABB_SIDE_LEN, -SCENE_AABB_SIDE_LEN},
      {-SCENE_AABB_SIDE_LEN, SCENE_AABB_SIDE_LEN, SCENE_AABB_SIDE_LEN},
      {-SCENE_AABB_SIDE_LEN, -SCENE_AABB_SIDE_LEN, SCENE_AABB_SIDE_LEN},

      // x = 1 face
      {SCENE_AABB_SIDE_LEN, -SCENE_AABB_SIDE_LEN, -SCENE_AABB_SIDE_LEN},
      {SCENE_AABB_SIDE_LEN, SCENE_AABB_SIDE_LEN, -SCENE_AABB_SIDE_LEN},
      {SCENE_AABB_SIDE_LEN, SCENE_AABB_SIDE_LEN, SCENE_AABB_SIDE_LEN},
      {SCENE_AABB_SIDE_LEN, -SCENE_AABB_SIDE_LEN, -SCENE_AABB_SIDE_LEN},
      {SCENE_AABB_SIDE_LEN, SCENE_AABB_SIDE_LEN, SCENE_AABB_SIDE_LEN},
      {SCENE_AABB_SIDE_LEN, -SCENE_AABB_SIDE_LEN, SCENE_AABB_SIDE_LEN},

      // y = -1 face
      {-SCENE_AABB_SIDE_LEN, -SCENE_AABB_SIDE_LEN, -SCENE_AABB_SIDE_LEN},
      {-SCENE_AABB_SIDE_LEN, -SCENE_AABB_SIDE_LEN, SCENE_AABB_SIDE_LEN},
      {SCENE_AABB_SIDE_LEN, -SCENE_AABB_SIDE_LEN, SCENE_AABB_SIDE_LEN},
      {-SCENE_AABB_SIDE_LEN, -SCENE_AABB_SIDE_LEN, -SCENE_AABB_SIDE_LEN},
      {SCENE_AABB_SIDE_LEN, -SCENE_AABB_SIDE_LEN, SCENE_AABB_SIDE_LEN},
      {SCENE_AABB_SIDE_LEN, -SCENE_AABB_SIDE_LEN, -SCENE_AABB_SIDE_LEN},

      // y = 1 face
      {-SCENE_AABB_SIDE_LEN, SCENE_AABB_SIDE_LEN, -SCENE_AABB_SIDE_LEN},
      {-SCENE_AABB_SIDE_LEN, SCENE_AABB_SIDE_LEN, SCENE_AABB_SIDE_LEN},
      {SCENE_AABB_SIDE_LEN, SCENE_AABB_SIDE_LEN, SCENE_AABB_SIDE_LEN},
      {-SCENE_AABB_SIDE_LEN, SCENE_AABB_SIDE_LEN, -SCENE_AABB_SIDE_LEN},
      {SCENE_AABB_SIDE_LEN, SCENE_AABB_SIDE_LEN, SCENE_AABB_SIDE_LEN},
      {SCENE_AABB_SIDE_LEN, SCENE_AABB_SIDE_LEN, -SCENE_AABB_SIDE_LEN},

      // z = -1 face
      {-SCENE_AABB_SIDE_LEN, -SCENE_AABB_SIDE_LEN, -SCENE_AABB_SIDE_LEN},
      {-SCENE_AABB_SIDE_LEN, SCENE_AABB_SIDE_LEN, -SCENE_AABB_SIDE_LEN},
      {SCENE_AABB_SIDE_LEN, SCENE_AABB_SIDE_LEN, -SCENE_AABB_SIDE_LEN},
      {-SCENE_AABB_SIDE_LEN, -SCENE_AABB_SIDE_LEN, -SCENE_AABB_SIDE_LEN},
      {SCENE_AABB_SIDE_LEN, SCENE_AABB_SIDE_LEN, -SCENE_AABB_SIDE_LEN},
      {SCENE_AABB_SIDE_LEN, -SCENE_AABB_SIDE_LEN, -SCENE_AABB_SIDE_LEN},

      // z = 1 face
      {-SCENE_AABB_SIDE_LEN, -SCENE_AABB_SIDE_LEN, SCENE_AABB_SIDE_LEN},
      {-SCENE_AABB_SIDE_LEN, SCENE_AABB_SIDE_LEN, SCENE_AABB_SIDE_LEN},
      {SCENE_AABB_SIDE_LEN, SCENE_AABB_SIDE_LEN, SCENE_AABB_SIDE_LEN},
      {-SCENE_AABB_SIDE_LEN, -SCENE_AABB_SIDE_LEN, SCENE_AABB_SIDE_LEN},
      {SCENE_AABB_SIDE_LEN, SCENE_AABB_SIDE_LEN, SCENE_AABB_SIDE_LEN},
      {SCENE_AABB_SIDE_LEN, -SCENE_AABB_SIDE_LEN, SCENE_AABB_SIDE_LEN}};

  sky->tex_coords = {{0, 0},
                     {0, kTexUpperLim},
                     {kTexUpperLim, kTexUpperLim},
                     {0, 0},
                     {kTexUpperLim, kTexUpperLim},
                     {kTexUpperLim, 0},
                     {0, 0},
                     {0, kTexUpperLim},
                     {kTexUpperLim, kTexUpperLim},
                     {0, 0},
                     {kTexUpperLim, kTexUpperLim},
                     {kTexUpperLim, 0},
                     {0, 0},
                     {0, kTexUpperLim},
                     {kTexUpperLim, kTexUpperLim},
                     {0, 0},
                     {kTexUpperLim, kTexUpperLim},
                     {kTexUpperLim, 0},
                     {0, 0},
                     {0, kTexUpperLim},
                     {kTexUpperLim, kTexUpperLim},
                     {0, 0},
                     {kTexUpperLim, kTexUpperLim},
                     {kTexUpperLim, 0},
                     {0, 0},
                     {0, kTexUpperLim},
                     {kTexUpperLim, kTexUpperLim},
                     {0, 0},
                     {kTexUpperLim, kTexUpperLim},
                     {kTexUpperLim, 0},
                     {0, 0},
                     {0, kTexUpperLim},
                     {kTexUpperLim, kTexUpperLim},
                     {0, 0},
                     {kTexUpperLim, kTexUpperLim},
                     {kTexUpperLim, 0}};

  assert(SKY_VERTEX_COUNT == sky->positions.size());
  assert(SKY_VERTEX_COUNT == sky->tex_coords.size());

  for (uint i = 0; i < Count(sky); ++i) {
    sky->positions[i] *= 0.5f;
  }
}

static void MakeCameraPath(const SplineVertices *spline,
                           CameraPathVertices *campath) {
  // Initial binormal chosen arbitrarily
  static const glm::vec3 kInitialBinormal = {0, 1, -0.5};

  assert(spline);
  assert(campath);

  assert(campath->positions.empty());
  assert(campath->normals.empty());
  assert(campath->binormals.empty());

  uint vertex_count = Count(spline);

  assert(vertex_count > 0);

  campath->positions.resize(vertex_count);
  campath->normals.resize(vertex_count);
  campath->binormals.resize(vertex_count);

  for (uint i = 0; i < vertex_count; ++i) {
    campath->positions[i] = spline->positions[i];
  }

  campath->normals[0] =
      glm::normalize(glm::cross(spline->tangents[0], kInitialBinormal));
  campath->binormals[0] =
      glm::normalize(glm::cross(spline->tangents[0], campath->normals[0]));

  for (uint i = 1; i < vertex_count; ++i) {
    campath->normals[i] = glm::normalize(
        glm::cross(campath->binormals[i - 1], spline->tangents[i]));
    campath->binormals[i] =
        glm::normalize(glm::cross(spline->tangents[i], campath->normals[i]));
  }
}

/*
Gauge is the distance between the two rails.

Rail cross section:

  Rectangle defined by vertices [1, 6] is called the head.
  Rectangle defined by vertices {0, 1, 6, 7} is called the web.

  Width is along horizontal dimension (<-------->).

  4 ------------------------ 3
  |                          |
  |                          |
  |                          |
  5 ----- 6          1 ----- 2
          |          |
          |          |
          |          |
          |          |
          |          |
          7 -------- 0
*/
static void MakeRails(const CameraPathVertices *campath_vertices,
                      const glm::vec4 *color, float head_w, float head_h,
                      float web_w, float web_h, float gauge,
                      std::vector<Vertex> *vertices,
                      std::vector<GLuint> *indices) {
  assert(campath_vertices);
  assert(color);
  assert(vertices);
  assert(indices);

  assert(head_w > 0);
  assert(web_w > 0);
  assert(head_w > web_w);
  assert(gauge > 0);

  static constexpr uint kCrossSectionVertexCount = 8;

  auto &cpv_pos = campath_vertices->positions;
  auto &cpv_binorm = campath_vertices->binormals;
  auto &cpv_norm = campath_vertices->normals;

  auto &rv = *vertices;

  uint cpv_count = Count(campath_vertices);

  uint rv_count = 2 * cpv_count * kCrossSectionVertexCount;
  rv.resize(rv_count);

  for (uint i = 0; i < rv_count; ++i) {
    rv[i].color = *color;
  }

  for (uint i = 0; i < 2; ++i) {
    for (uint j = 0; j < cpv_count; ++j) {
      uint k = kCrossSectionVertexCount * (j + i * cpv_count);
      // See the comment block immediately above this function definition for
      // the visual index-to-position mapping
      rv[k].position =
          cpv_pos[j] - web_h * cpv_norm[j] + 0.5f * web_w * cpv_binorm[j];
      rv[k + 1].position = cpv_pos[j] + 0.5f * web_w * cpv_binorm[j];
      rv[k + 2].position = cpv_pos[j] + 0.5f * head_w * cpv_binorm[j];
      rv[k + 3].position =
          cpv_pos[j] + head_h * cpv_norm[j] + 0.5f * head_w * cpv_binorm[j];
      rv[k + 4].position = rv[k + 4].position =
          cpv_pos[j] + head_h * cpv_norm[j] - 0.5f * head_w * cpv_binorm[j];
      rv[k + 5].position = cpv_pos[j] - 0.5f * head_w * cpv_binorm[j];
      rv[k + 6].position = cpv_pos[j] - 0.5f * web_w * cpv_binorm[j];
      rv[k + 7].position =
          cpv_pos[j] - web_h * cpv_norm[j] - 0.5f * web_w * cpv_binorm[j];
    }
  }

  // Set rail pair `gauge` distance apart
  for (uint i = 0; i < cpv_count; ++i) {
    uint j = kCrossSectionVertexCount * i;
    for (uint k = 0; k < kCrossSectionVertexCount; ++k) {
      rv[j + k].position += 0.5f * gauge * cpv_binorm[i];
    }
  }
  for (uint i = 0; i < cpv_count; ++i) {
    uint j = kCrossSectionVertexCount * (i + cpv_count);
    for (uint k = 0; k < kCrossSectionVertexCount; ++k) {
      rv[j + k].position -= 0.5f * gauge * cpv_binorm[i];
    }
  }

  auto &ri = *indices;

  for (uint i = 0; i < 2; ++i) {
    for (uint j = i * rv_count / 2;
         j + kCrossSectionVertexCount < rv_count / (2 - i);
         j += kCrossSectionVertexCount) {
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
                          TexturedVertices *crossbar) {
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

  auto &cb_pos = crossbar->positions;
  auto &cb_tex_coords = crossbar->tex_coords;

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

enum RgbaChannel {
  kRgbaChannelRed,
  kRgbaChannelGreen,
  kRgbaChannelBlue,
  kRgbaChannelAlpha,
  kRgbaChannel_Count
};

static Status InitTexture(const char *img_filepath, GLuint texture_name) {
  assert(img_filepath);

  int w;
  int h;
  int channel_count;
  uchar *data =
      stbi_load(img_filepath, &w, &h, &channel_count, kRgbaChannel_Count);
  if (!data) {
    std::fprintf(stderr, "Could not load texture from file %s.\n",
                 img_filepath);
    return kStatusIOError;
  }

  glBindTexture(GL_TEXTURE_2D, texture_name);

  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               data);

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
    std::fprintf(stderr, "Texture initialization error. Error code: %d.\n",
                 errCode);
    stbi_image_free(data);
    return kStatusIOError;
  }

  stbi_image_free(data);

  return kStatusOk;
}

Status SaveScreenshot(const char *filepath) {
  std::vector<uchar> screenshot(windowWidth * windowHeight * 3);
  glReadPixels(0, 0, windowWidth, windowHeight, GL_RGB, GL_UNSIGNED_BYTE,
               screenshot.data());

  stbi_flip_vertically_on_write(1);
  int ret = stbi_write_jpg(filepath, windowWidth, windowHeight, 3,
                           screenshot.data(), 95);
  if (ret == 0) {
    std::fprintf(stderr, "Could not write data to JPEG file.\n");
    return kStatusIOError;
  }

  std::printf("Saved screenshot to file %s.\n", filepath);

  // ImageIO screenshotImg(windowWidth, windowHeight, 3, screenshotData);

  // if (screenshotImg.save(filename, ImageIO::FORMAT_JPEG) == ImageIO::OK)
  // {
  //   std::cout << "File " << filename << " saved successfully." <<
  //   std::endl;
  // } else {
  //   std::cout << "Failed to save file " << filename << '.' << std::endl;
  // }
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
      SaveScreenshot(temp);
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

  /**************************
   * Untextured models
   ***************************/

  GLuint prog = basicProgram->GetProgramHandle();
  GLint model_view_mat_loc = glGetUniformLocation(prog, "modelViewMatrix");
  GLint proj_mat_loc = glGetUniformLocation(prog, "projectionMatrix");

  glUseProgram(prog);

  /* Rails */

  matrix->GetMatrix(model_view_mat);
  matrix->SetMatrixMode(OpenGLMatrix::Projection);
  matrix->GetMatrix(proj_mat);
  matrix->SetMatrixMode(OpenGLMatrix::ModelView);

  glUniformMatrix4fv(model_view_mat_loc, 1, is_row_major, model_view_mat);
  glUniformMatrix4fv(proj_mat_loc, 1, is_row_major, proj_mat);

  glBindVertexArray(vao_names[kVertexFormatUntextured]);

  glDrawElements(GL_TRIANGLES, rail_indices.size() / 2, GL_UNSIGNED_INT,
                 BUFFER_OFFSET(0));
  glDrawElements(GL_TRIANGLES, rail_indices.size() / 2, GL_UNSIGNED_INT,
                 BUFFER_OFFSET(rail_indices.size() / 2 * sizeof(GLuint)));

  glBindVertexArray(0);

  /**********************
   * Textured models
   ***********************/

  prog = texProgram->GetProgramHandle();
  model_view_mat_loc = glGetUniformLocation(prog, "modelViewMatrix");
  proj_mat_loc = glGetUniformLocation(prog, "projectionMatrix");

  glUseProgram(prog);

  glBindVertexArray(vao_names[kVertexFormatTextured]);

  GLint first = 0;

  /* Ground */

  matrix->PushMatrix();

  float scene_aabb_half_side_len = glm::length(scene_aabb_size) * 0.5f;
  matrix->Translate(0, -scene_aabb_half_side_len * 0.5f, 0);
  matrix->Translate(-scene_aabb_center.x, -scene_aabb_center.y,
                    -scene_aabb_center.z);

  matrix->GetMatrix(model_view_mat);
  matrix->SetMatrixMode(OpenGLMatrix::Projection);
  matrix->GetMatrix(proj_mat);
  matrix->SetMatrixMode(OpenGLMatrix::ModelView);  // default matrix mode

  matrix->PopMatrix();

  glUniformMatrix4fv(model_view_mat_loc, 1, is_row_major, model_view_mat);
  glUniformMatrix4fv(proj_mat_loc, 1, is_row_major, proj_mat);

  glBindTexture(GL_TEXTURE_2D, textures[kTextureGround]);
  glDrawArrays(GL_TRIANGLES, first, GROUND_VERTEX_COUNT);
  first += GROUND_VERTEX_COUNT;

  /* Sky */

  matrix->PushMatrix();

  matrix->Translate(-scene_aabb_center.x, -scene_aabb_center.y,
                    -scene_aabb_center.z);

  matrix->GetMatrix(model_view_mat);
  matrix->SetMatrixMode(OpenGLMatrix::Projection);
  matrix->GetMatrix(proj_mat);
  matrix->SetMatrixMode(OpenGLMatrix::ModelView);  // default matrix mode

  matrix->PopMatrix();

  glUniformMatrix4fv(model_view_mat_loc, 1, is_row_major, model_view_mat);
  glUniformMatrix4fv(proj_mat_loc, 1, is_row_major, proj_mat);

  glBindTexture(GL_TEXTURE_2D, textures[kTextureSky]);
  glDrawArrays(GL_TRIANGLES, first, SKY_VERTEX_COUNT);
  first += SKY_VERTEX_COUNT;

  /* Crossbars */

  matrix->GetMatrix(model_view_mat);
  matrix->SetMatrixMode(OpenGLMatrix::Projection);
  matrix->GetMatrix(proj_mat);
  matrix->SetMatrixMode(OpenGLMatrix::ModelView);  // default matrix mode

  glUniformMatrix4fv(model_view_mat_loc, 1, is_row_major, model_view_mat);
  glUniformMatrix4fv(proj_mat_loc, 1, is_row_major, proj_mat);

  glBindTexture(GL_TEXTURE_2D, textures[kTextureCrossbar]);
  for (uint offset = 0; offset < Count(&crossbar_vertices); offset += 36) {
    glDrawArrays(GL_TRIANGLES, first + offset, 36);
  }

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

  TexturedVertices ground_vertices;
  MakeGround(&ground_vertices);

  TexturedVertices sky_vertices;
  MakeSky(&sky_vertices);

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

  MakeCameraPath(&spline_vertices, &camera_path_vertices);

  glm::vec4 rail_color(RAIL_COLOR_RED, RAIL_COLOR_GREEN, RAIL_COLOR_BLUE,
                       RAIL_COLOR_ALPHA);
  std::vector<Vertex> rail_vertices;
  MakeRails(&camera_path_vertices, &rail_color, RAIL_HEAD_W, RAIL_HEAD_H,
            RAIL_WEB_W, RAIL_WEB_H, RAIL_GAUGE, &rail_vertices, &rail_indices);

  MakeCrossbars(&camera_path_vertices, &spline_vertices.tangents,
                &crossbar_vertices);

  glClearColor(0, 0, 0, 0);
  glEnable(GL_DEPTH_TEST);

  matrix = new OpenGLMatrix();

  glGenTextures(kTexture_Count, textures);

  InitTexture(argv[2], textures[kTextureGround]);
  InitTexture(argv[3], textures[kTextureSky]);
  InitTexture(argv[4], textures[kTextureCrossbar]);

  initBasicPipelineProgram();
  initTexPipelineProgram();

  glGenBuffers(kVbo_Count, vbo_names);

  assert(SKY_VERTEX_COUNT == Count(&sky_vertices));

  uint vertex_count =
      GROUND_VERTEX_COUNT + SKY_VERTEX_COUNT + Count(&crossbar_vertices);

  // Buffer textured vertices
  {
    glBindBuffer(GL_ARRAY_BUFFER, vbo_names[kVboTexturedVertices]);

    uint buffer_size = vertex_count * (sizeof(glm::vec3) + sizeof(glm::vec2));
    glBufferData(GL_ARRAY_BUFFER, buffer_size, NULL, GL_STATIC_DRAW);

    uint offset = 0;
    uint size = GROUND_VERTEX_COUNT * sizeof(glm::vec3);
    glBufferSubData(GL_ARRAY_BUFFER, offset, size,
                    ground_vertices.positions.data());
    offset += size;
    size = SKY_VERTEX_COUNT * sizeof(glm::vec3),
    glBufferSubData(GL_ARRAY_BUFFER, offset, size,
                    sky_vertices.positions.data());
    offset += size;
    size = Count(&crossbar_vertices) * sizeof(glm::vec3),
    glBufferSubData(GL_ARRAY_BUFFER, offset, size,
                    crossbar_vertices.positions.data());

    offset += size;
    size = GROUND_VERTEX_COUNT * sizeof(glm::vec2),
    glBufferSubData(GL_ARRAY_BUFFER, offset, size,
                    ground_vertices.tex_coords.data());
    offset += size;
    size = SKY_VERTEX_COUNT * sizeof(glm::vec2),
    glBufferSubData(GL_ARRAY_BUFFER, offset, size,
                    sky_vertices.tex_coords.data());
    offset += size;
    size = Count(&crossbar_vertices) * sizeof(glm::vec2),
    glBufferSubData(GL_ARRAY_BUFFER, offset, size,
                    crossbar_vertices.tex_coords.data());

    glBindBuffer(GL_ARRAY_BUFFER, 0);
  }

  // Buffer untextured vertices
  {
    glBindBuffer(GL_ARRAY_BUFFER, vbo_names[kVboUntexturedVertices]);
    glBufferData(GL_ARRAY_BUFFER, rail_vertices.size() * sizeof(Vertex),
                 rail_vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
  }

  glGenVertexArrays(kVertexFormat_Count, vao_names);

  // Setup textured VAO
  {
    GLuint prog = texProgram->GetProgramHandle();
    GLuint pos_loc = glGetAttribLocation(prog, "position");
    GLuint tex_coord_loc = glGetAttribLocation(prog, "texCoord");

    glBindVertexArray(vao_names[kVertexFormatTextured]);

    glBindBuffer(GL_ARRAY_BUFFER, vbo_names[kVboTexturedVertices]);
    glVertexAttribPointer(pos_loc, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3),
                          BUFFER_OFFSET(0));
    glVertexAttribPointer(tex_coord_loc, 2, GL_FLOAT, GL_FALSE,
                          sizeof(glm::vec2),
                          BUFFER_OFFSET(vertex_count * sizeof(glm::vec3)));
    glEnableVertexAttribArray(pos_loc);
    glEnableVertexAttribArray(tex_coord_loc);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
  }

  // Setup untextured VAO
  {
    GLuint prog = basicProgram->GetProgramHandle();
    GLuint pos_loc = glGetAttribLocation(prog, "position");
    GLuint color_loc = glGetAttribLocation(prog, "color");

    glBindVertexArray(vao_names[kVertexFormatUntextured]);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_names[kVboUntexturedVertices]);

    glVertexAttribPointer(pos_loc, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          BUFFER_OFFSET(0));
    glVertexAttribPointer(color_loc, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          BUFFER_OFFSET(sizeof(glm::vec3)));

    glEnableVertexAttribArray(pos_loc);
    glEnableVertexAttribArray(color_loc);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo_names[kVboRailIndices]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, rail_indices.size() * sizeof(GLuint),
                 rail_indices.data(), GL_STATIC_DRAW);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
  }

  // sink forever into the glut loop
  glutMainLoop();
}
