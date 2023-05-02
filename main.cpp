#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <glm/glm.hpp>
#include <iomanip>
#include <iostream>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "basicPipelineProgram.h"
#include "glutHeader.h"
#include "openGLHeader.h"
#include "openGLMatrix.h"
#include "spline.hpp"
#include "stb_image.h"
#include "stb_image_write.h"
#include "texPipelineProgram.h"
#include "types.hpp"

char shaderBasePath[1024] = "openGLHelper";

#define BUFFER_OFFSET(offset) ((GLvoid *)(offset))

#define RAIL_COLOR_RED 0.5
#define RAIL_COLOR_BLUE 0.5
#define RAIL_COLOR_GREEN 0.5
#define RAIL_COLOR_ALPHA 1
#define RAIL_HEAD_W 0.2
#define RAIL_HEAD_H 0.1
#define RAIL_WEB_W 0.1
#define RAIL_WEB_H 0.1
#define RAIL_GAUGE 2

#define CROSSTIE_SEPARATION_DIST 1
#define CROSSTIE_POSITION_OFFSET_IN_CAMERA_PATH_NORMAL_DIR -2

#define SCENE_AABB_SIDE_LEN 256
#define GROUND_VERTEX_COUNT 6
#define SKY_VERTEX_COUNT 36

enum Status { kStatusOk, kStatusIOError };

enum RgbaChannel {
  kRgbaChannel_Red,
  kRgbaChannel_Green,
  kRgbaChannel_Blue,
  kRgbaChannel_Alpha,
  kRgbaChannel__Count
};

enum RgbChannel {
  kRgbChannel_Red,
  kRgbChannel_Green,
  kRgbChannel_Blue,
  kRgbChannel__Count
};

uint screenshot_count = 0;
uint record_video = 0;

uint window_w = 1280;
uint window_h = 720;
const char *kWindowTitle = "Roller Coaster";

#define FOV_Y 45
#define NEAR_Z 0.01
#define FAR_Z 10000

uint frame_count = 0;

// Helper matrix object
OpenGLMatrix *matrix;

// Pipeline programs
BasicPipelineProgram *basicProgram;
TexPipelineProgram *texProgram;

enum Button { kButton_Left, kButton_Middle, kButton_Right, kButton__Count };

struct MouseState {
  glm::ivec2 position;
  int pressed_buttons;
};

static void PressButton(MouseState *s, Button b) {
  assert(s);

  s->pressed_buttons |= 1 << b;
}

static void ReleaseButton(MouseState *s, Button b) {
  assert(s);

  s->pressed_buttons &= ~(1 << b);
}

static int IsButtonPressed(MouseState *s, Button b) {
  assert(s);

  return s->pressed_buttons >> b & 1;
}

static Button FromGlButton(int button) {
  switch (button) {
    case GLUT_LEFT_BUTTON: {
      return kButton_Left;
    }
    case GLUT_MIDDLE_BUTTON: {
      return kButton_Middle;
    }
    case GLUT_RIGHT_BUTTON: {
      return kButton_Right;
    }
    default: {
      assert(false);
    }
  }
}

MouseState mouse_state = {};

enum WorldStateOp {
  kWorldStateOp_Rotate,
  kWorldStateOp_Translate,
  kWorldStateOp_Scale
};
WorldStateOp world_state_op = kWorldStateOp_Rotate;

struct WorldState {
  glm::vec3 rotation;
  glm::vec3 translation;
  glm::vec3 scale;
};

static WorldState world_state = {{}, {}, {1, 1, 1}};

uint camera_path_index = 0;

struct Vertex {
  glm::vec3 position;
  glm::vec4 color;
};

struct CameraPathVertices {
  std::vector<glm::vec3> positions;
  std::vector<glm::vec3> tangents;
  std::vector<glm::vec3> normals;
  std::vector<glm::vec3> binormals;
};

static uint Count(const CameraPathVertices *c) {
  assert(c);
  assert(c->positions.size() == c->tangents.size());
  assert(c->tangents.size() == c->normals.size());
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

enum Texture { kTextureGround, kTextureSky, kTextureCrosstie, kTexture_Count };

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
static TexturedVertices crosstie_vertices;
static SplineVertices spline_vertices;

static GLuint textures[kTexture_Count];
static GLuint vao_names[kVertexFormat_Count];
static GLuint vbo_names[kVbo_Count];

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
                           CameraPathVertices *path) {
  // Initial binormal chosen arbitrarily.
  static const glm::vec3 kInitialBinormal = {0, 1, -0.5};

  assert(spline);
  assert(path);

  uint count = Count(spline);

  assert(count > 0);

  path->positions = spline->positions;
  path->tangents = spline->tangents;

  path->normals.resize(count);
  path->binormals.resize(count);

  path->normals[0] =
      glm::normalize(glm::cross(spline->tangents[0], kInitialBinormal));
  path->binormals[0] =
      glm::normalize(glm::cross(spline->tangents[0], path->normals[0]));

  for (uint i = 1; i < count; ++i) {
    path->normals[i] =
        glm::normalize(glm::cross(path->binormals[i - 1], spline->tangents[i]));
    path->binormals[i] =
        glm::normalize(glm::cross(spline->tangents[i], path->normals[i]));
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
static void MakeRails(const CameraPathVertices *campath, const glm::vec4 *color,
                      float head_w, float head_h, float web_w, float web_h,
                      float gauge, float pos_offset_in_campath_norm_dir,
                      std::vector<Vertex> *vertices,
                      std::vector<GLuint> *indices) {
  assert(campath);
  assert(color);
  assert(vertices);
  assert(indices);

  assert(head_w > 0);
  assert(web_w > 0);
  assert(head_w > web_w);
  assert(gauge > 0);

  static constexpr uint kCrossSectionVertexCount = 8;

  auto &cv_pos = campath->positions;
  auto &cv_binorm = campath->binormals;
  auto &cv_norm = campath->normals;

  auto &rv = *vertices;

  uint cv_count = Count(campath);

  uint rv_count = 2 * cv_count * kCrossSectionVertexCount;
  rv.resize(rv_count);

  for (uint i = 0; i < rv_count; ++i) {
    rv[i].color = *color;
  }

  for (uint i = 0; i < 2; ++i) {
    for (uint j = 0; j < cv_count; ++j) {
      uint k = kCrossSectionVertexCount * (j + i * cv_count);
      // See the comment block immediately above this function definition for
      // the visual index-to-position mapping
      rv[k].position =
          cv_pos[j] - web_h * cv_norm[j] + 0.5f * web_w * cv_binorm[j];
      rv[k + 1].position = cv_pos[j] + 0.5f * web_w * cv_binorm[j];
      rv[k + 2].position = cv_pos[j] + 0.5f * head_w * cv_binorm[j];
      rv[k + 3].position =
          cv_pos[j] + head_h * cv_norm[j] + 0.5f * head_w * cv_binorm[j];
      rv[k + 4].position = rv[k + 4].position =
          cv_pos[j] + head_h * cv_norm[j] - 0.5f * head_w * cv_binorm[j];
      rv[k + 5].position = cv_pos[j] - 0.5f * head_w * cv_binorm[j];
      rv[k + 6].position = cv_pos[j] - 0.5f * web_w * cv_binorm[j];
      rv[k + 7].position =
          cv_pos[j] - web_h * cv_norm[j] - 0.5f * web_w * cv_binorm[j];
    }
  }

  // Set rail pair `gauge` distance apart
  for (uint i = 0; i < cv_count; ++i) {
    uint j = kCrossSectionVertexCount * i;
    for (uint k = 0; k < kCrossSectionVertexCount; ++k) {
      rv[j + k].position += 0.5f * gauge * cv_binorm[i];
    }
  }
  for (uint i = 0; i < cv_count; ++i) {
    uint j = kCrossSectionVertexCount * (i + cv_count);
    for (uint k = 0; k < kCrossSectionVertexCount; ++k) {
      rv[j + k].position -= 0.5f * gauge * cv_binorm[i];
    }
  }

  for (uint i = 0; i < 2; ++i) {
    for (uint j = 0; j < cv_count; ++j) {
      uint k = kCrossSectionVertexCount * (j + i * cv_count);
      for (uint l = 0; l < kCrossSectionVertexCount; ++l) {
        rv[k + l].position += pos_offset_in_campath_norm_dir * cv_norm[j];
      }
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

static void MakeCrossties(const CameraPathVertices *campath,
                          float separation_dist,
                          float pos_offset_in_campath_norm_dir,
                          TexturedVertices *crosstie) {
  static constexpr float kAlpha = 0.1;
  static constexpr float kBeta = 1.5;
  static constexpr int kVertexCount = 8;
  static constexpr float kBarDepth = 0.3;
  static constexpr float kTolerance = 0.00001;

  auto &p_pos = campath->positions;
  auto &p_tan = campath->tangents;
  auto &p_binorm = campath->binormals;
  auto &p_norm = campath->normals;
  uint p_count = Count(campath);

  auto &b_pos = crosstie->positions;
  auto &b_tex_coords = crosstie->tex_coords;

  glm::vec3 v[kVertexCount];
  float dist_moved = 0;
  for (uint i = 1; i < p_count; ++i) {
    dist_moved += glm::length(p_pos[i] - p_pos[i - 1]);

    if (dist_moved < separation_dist + kTolerance) {
      continue;
    }

    v[0] = p_pos[i] + kAlpha * (-kBeta * p_norm[i] + p_binorm[i] * 0.5f) +
           p_binorm[i];
    v[1] = p_pos[i] + kAlpha * (-p_norm[i] + p_binorm[i] * 0.5f) + p_binorm[i];
    v[2] = p_pos[i] + kAlpha * (-kBeta * p_norm[i] + p_binorm[i] * 0.5f) -
           p_binorm[i] - kAlpha * p_binorm[i];
    v[3] = p_pos[i] + kAlpha * (-p_norm[i] + p_binorm[i] * 0.5f) - p_binorm[i] -
           kAlpha * p_binorm[i];

    v[4] = p_pos[i] + kAlpha * (-kBeta * p_norm[i] + p_binorm[i] * 0.5f) +
           p_binorm[i] + kBarDepth * p_tan[i];
    v[5] = p_pos[i] + kAlpha * (-p_norm[i] + p_binorm[i] * 0.5f) + p_binorm[i] +
           kBarDepth * p_tan[i];
    v[6] = p_pos[i] + kAlpha * (-kBeta * p_norm[i] + p_binorm[i] * 0.5f) -
           p_binorm[i] + kBarDepth * p_tan[i] - kAlpha * p_binorm[i];
    v[7] = p_pos[i] + kAlpha * (-p_norm[i] + p_binorm[i] * 0.5f) - p_binorm[i] +
           kBarDepth * p_tan[i] - kAlpha * p_binorm[i];

    for (uint j = 0; j < kVertexCount; ++j) {
      v[j] += pos_offset_in_campath_norm_dir * p_norm[i];
    }

    // top face
    b_pos.push_back(v[6]);
    b_pos.push_back(v[5]);
    b_pos.push_back(v[2]);
    b_pos.push_back(v[5]);
    b_pos.push_back(v[1]);
    b_pos.push_back(v[2]);

    // right face
    b_pos.push_back(v[5]);
    b_pos.push_back(v[4]);
    b_pos.push_back(v[1]);
    b_pos.push_back(v[4]);
    b_pos.push_back(v[0]);
    b_pos.push_back(v[1]);

    // bottom face
    b_pos.push_back(v[4]);
    b_pos.push_back(v[7]);
    b_pos.push_back(v[0]);
    b_pos.push_back(v[7]);
    b_pos.push_back(v[3]);
    b_pos.push_back(v[0]);

    // left face
    b_pos.push_back(v[7]);
    b_pos.push_back(v[6]);
    b_pos.push_back(v[3]);
    b_pos.push_back(v[6]);
    b_pos.push_back(v[2]);
    b_pos.push_back(v[3]);

    // back face
    b_pos.push_back(v[5]);
    b_pos.push_back(v[6]);
    b_pos.push_back(v[4]);
    b_pos.push_back(v[6]);
    b_pos.push_back(v[7]);
    b_pos.push_back(v[4]);

    // front face
    b_pos.push_back(v[2]);
    b_pos.push_back(v[1]);
    b_pos.push_back(v[3]);
    b_pos.push_back(v[1]);
    b_pos.push_back(v[0]);
    b_pos.push_back(v[3]);

    for (uint j = 0; j < 6; ++j) {
      b_tex_coords.emplace_back(0, 1);
      b_tex_coords.emplace_back(1, 1);
      b_tex_coords.emplace_back(0, 0);
      b_tex_coords.emplace_back(1, 1);
      b_tex_coords.emplace_back(1, 0);
      b_tex_coords.emplace_back(0, 0);
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

static Status InitTexture(const char *img_filepath, GLuint texture_name) {
  assert(img_filepath);

  int w;
  int h;
  int channel_count;
  uchar *data =
      stbi_load(img_filepath, &w, &h, &channel_count, kRgbaChannel__Count);
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
  std::vector<uchar> screenshot(window_w * window_h * kRgbChannel__Count);

  glReadPixels(0, 0, window_w, window_h, GL_RGB, GL_UNSIGNED_BYTE,
               screenshot.data());

  stbi_flip_vertically_on_write(1);
  int ret = stbi_write_jpg(filepath, window_w, window_h, kRgbChannel__Count,
                           screenshot.data(), 95);
  if (ret == 0) {
    std::fprintf(stderr, "Could not write data to JPEG file.\n");
    return kStatusIOError;
  }

  return kStatusOk;
}

void timerFunc(int val) {
  if (val) {
    char *temp = new char[512 + strlen(kWindowTitle)];
    // Update title bar info
    sprintf(temp, "%s: %d fps , %d x %d resolution", kWindowTitle,
            frame_count * 30, window_w, window_h);
    glutSetWindowTitle(temp);
    delete[] temp;

    if (record_video) {  // take a screenshot
      temp = new char[8];
      sprintf(temp, "%03d.jpg", screenshot_count);
      SaveScreenshot(temp);
      std::printf("Saved screenshot to file %s.\n", temp);
      delete[] temp;
      ++screenshot_count;
    }
  }

  frame_count = 0;
  glutTimerFunc(33, timerFunc, 1);  //~30 fps
}

static void OnWindowReshape(int w, int h) {
  window_w = w;
  window_h = h;

  glViewport(0, 0, window_w, window_h);

  float aspect = (float)window_w / window_h;

  matrix->SetMatrixMode(OpenGLMatrix::Projection);
  matrix->LoadIdentity();
  matrix->Perspective(FOV_Y, aspect, NEAR_Z, FAR_Z);
  matrix->SetMatrixMode(OpenGLMatrix::ModelView);  // By default in ModelView
}

static void OnPassiveMouseMotion(int x, int y) {
  mouse_state.position.x = x;
  mouse_state.position.y = y;
}

void OnMouseDrag(int x, int y) {
  // the change in mouse position since the last invocation of this function
  glm::vec2 pos_delta = {x - mouse_state.position.x,
                         y - mouse_state.position.y};

  switch (world_state_op) {
    case kWorldStateOp_Translate: {
      if (IsButtonPressed(&mouse_state, kButton_Left)) {
        world_state.translation.x += pos_delta.x * 0.1f;
        world_state.translation.y -= pos_delta.y * 0.1f;
      }
      if (IsButtonPressed(&mouse_state, kButton_Middle)) {
        world_state.translation.z += pos_delta.y;  // * 0.1;
      }
      break;
    }
    case kWorldStateOp_Rotate: {
      if (IsButtonPressed(&mouse_state, kButton_Left)) {
        world_state.rotation.x += pos_delta.y;
        world_state.rotation.y += pos_delta.x;
      }
      if (IsButtonPressed(&mouse_state, kButton_Middle)) {
        world_state.rotation.z += pos_delta.y;
      }
      break;
    }
    case kWorldStateOp_Scale: {
      if (IsButtonPressed(&mouse_state, kButton_Left)) {
        world_state.scale.x *= 1 + pos_delta.x * 0.01f;
        world_state.scale.y *= 1 - pos_delta.y * 0.01f;
      }
      if (IsButtonPressed(&mouse_state, kButton_Middle)) {
        world_state.scale.z *= 1 - pos_delta.y * 0.01f;
      }
      break;
    }
  }

  mouse_state.position.x = x;
  mouse_state.position.y = y;
}

static void OnMousePressOrRelease(int button, int state, int x, int y) {
  Button b = FromGlButton(button);
  switch (state) {
    case GLUT_DOWN: {
      PressButton(&mouse_state, b);
      break;
    }
    case GLUT_UP: {
      ReleaseButton(&mouse_state, b);
      break;
    }
    default: {
      assert(false);
    }
  }

  // keep track of whether CTRL and SHIFT keys are pressed
  switch (glutGetModifiers()) {
    case GLUT_ACTIVE_CTRL: {
      world_state_op = kWorldStateOp_Translate;
      break;
    }
    case GLUT_ACTIVE_SHIFT: {
      world_state_op = kWorldStateOp_Scale;
      break;
    }
      // if CTRL and SHIFT are not pressed, we are in rotate mode
    default: {
      world_state_op = kWorldStateOp_Rotate;
      break;
    }
  }

  mouse_state.position.x = x;
  mouse_state.position.y = y;
}

static void OnKeyPress(uchar key, int x, int y) {
  switch (key) {
    case 27: {  // ESC key
      exit(0);
      break;
    }
    case 'i': {
      SaveScreenshot("screenshot.jpg");
      break;
    }
    case 'v': {
      if (record_video == 1) {
        screenshot_count = 0;
      }
      record_video = !record_video;
      break;
    }
  }
}

void idleFunc() {
  if (camera_path_index < Count(&camera_path_vertices)) {
    camera_path_index += 3;
  }

  // for example, here, you can save the screenshots to disk (to make the
  // animation)

  // make the screen update
  glutPostRedisplay();
}

static void Display() {
  ++frame_count;

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  matrix->LoadIdentity();

  {
    auto i = camera_path_index;
    auto &p = camera_path_vertices.positions;
    auto &t = spline_vertices.tangents;
    auto &n = camera_path_vertices.normals;
    matrix->LookAt(p[i].x, p[i].y, p[i].z, p[i].x + t[i].x, p[i].y + t[i].y,
                   p[i].z + t[i].z, n[i].x, n[i].y, n[i].z);
  }

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

  /* Crossties */

  matrix->GetMatrix(model_view_mat);
  matrix->SetMatrixMode(OpenGLMatrix::Projection);
  matrix->GetMatrix(proj_mat);
  matrix->SetMatrixMode(OpenGLMatrix::ModelView);  // default matrix mode

  glUniformMatrix4fv(model_view_mat_loc, 1, is_row_major, model_view_mat);
  glUniformMatrix4fv(proj_mat_loc, 1, is_row_major, proj_mat);

  glBindTexture(GL_TEXTURE_2D, textures[kTextureCrosstie]);
  for (uint offset = 0; offset < Count(&crosstie_vertices); offset += 36) {
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
                 "<crosstie-texture>\n",
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

  glutInitWindowSize(window_w, window_h);
  glutInitWindowPosition(0, 0);
  glutCreateWindow(kWindowTitle);

  std::printf("OpenGL Info: \n");
  std::printf("  Version: %s\n", glGetString(GL_VERSION));
  std::printf("  Renderer: %s\n", glGetString(GL_RENDERER));
  std::printf("  Shading Language Version: %s\n",
              glGetString(GL_SHADING_LANGUAGE_VERSION));

  glutDisplayFunc(Display);
  // perform animation inside idleFunc
  glutIdleFunc(idleFunc);
  glutMotionFunc(OnMouseDrag);
  glutPassiveMotionFunc(OnPassiveMouseMotion);
  glutMouseFunc(OnMousePressOrRelease);
  glutReshapeFunc(OnWindowReshape);
  glutKeyboardFunc(OnKeyPress);
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
            RAIL_WEB_W, RAIL_WEB_H, RAIL_GAUGE, -2, &rail_vertices,
            &rail_indices);

  MakeCrossties(&camera_path_vertices, CROSSTIE_SEPARATION_DIST,
                CROSSTIE_POSITION_OFFSET_IN_CAMERA_PATH_NORMAL_DIR,
                &crosstie_vertices);

  glClearColor(0, 0, 0, 0);
  glEnable(GL_DEPTH_TEST);

  matrix = new OpenGLMatrix();

  glGenTextures(kTexture_Count, textures);

  InitTexture(argv[2], textures[kTextureGround]);
  InitTexture(argv[3], textures[kTextureSky]);
  InitTexture(argv[4], textures[kTextureCrosstie]);

  initBasicPipelineProgram();
  initTexPipelineProgram();

  glGenBuffers(kVbo_Count, vbo_names);

  assert(SKY_VERTEX_COUNT == Count(&sky_vertices));

  uint vertex_count =
      GROUND_VERTEX_COUNT + SKY_VERTEX_COUNT + Count(&crosstie_vertices);

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
    size = Count(&crosstie_vertices) * sizeof(glm::vec3),
    glBufferSubData(GL_ARRAY_BUFFER, offset, size,
                    crosstie_vertices.positions.data());

    offset += size;
    size = GROUND_VERTEX_COUNT * sizeof(glm::vec2),
    glBufferSubData(GL_ARRAY_BUFFER, offset, size,
                    ground_vertices.tex_coords.data());
    offset += size;
    size = SKY_VERTEX_COUNT * sizeof(glm::vec2),
    glBufferSubData(GL_ARRAY_BUFFER, offset, size,
                    sky_vertices.tex_coords.data());
    offset += size;
    size = Count(&crosstie_vertices) * sizeof(glm::vec2),
    glBufferSubData(GL_ARRAY_BUFFER, offset, size,
                    crosstie_vertices.tex_coords.data());

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
