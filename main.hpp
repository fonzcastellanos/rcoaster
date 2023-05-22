#ifndef RCOASTER_MAIN_HPP
#define RCOASTER_MAIN_HPP

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "types.hpp"

struct ViewFrustum {
  float fov_y;
  float near_z;
  float far_z;
};

struct Config {
  char* track_filepath;
  char* ground_texture_filepath;
  char* sky_texture_filepath;
  char* crossties_texture_filepath;

  ViewFrustum view_frustum;

  // Camera speed in spline segments per second.
  float camera_speed;
  float max_spline_segment_len;

  int is_verbose;
};

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

enum VertexFormat {
  kVertexFormat_Textured,
  kVertexFormat_Colored,
  kVertexFormat__Count
};

enum Button { kButton_Left, kButton_Middle, kButton_Right, kButton__Count };

struct MouseState {
  glm::ivec2 position;
  int pressed_buttons;
};

enum WorldStateOp {
  kWorldStateOp_Rotate,
  kWorldStateOp_Translate,
  kWorldStateOp_Scale,
  kWorldStateOp__Count
};

struct WorldState {
  glm::vec3 rotation;
  glm::vec3 translation;
  glm::vec3 scale;
};

enum Texture {
  kTexture_Ground,
  kTexture_Sky,
  kTexture_Crossties,
  kTexture__Count
};

enum Vbo {
  kVbo_TexturedVertices,
  kVbo_ColoredVertices,
  kVbo_RailIndices,
  kVbo__Count
};

#endif  // RCOASTER_MAIN_HPP