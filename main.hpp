#ifndef RCOASTER_MAIN_HPP
#define RCOASTER_MAIN_HPP

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

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
  kVertexFormat_Untextured,
  kVertexFormat_Textured,
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
  kVbo_UntexturedVertices,
  kVbo_RailIndices,
  kVbo__Count
};

#endif  // RCOASTER_MAIN_HPP