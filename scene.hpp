#ifndef RCOASTER_SCENE_HPP
#define RCOASTER_SCENE_HPP

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <vector>

#include "models.hpp"
#include "types.hpp"

struct ColoredVertices {
  std::vector<glm::vec3> positions;
  std::vector<glm::vec4> colors;
};

uint Count(const ColoredVertices* v);

struct TexturedVertices {
  glm::vec3* positions;
  glm::vec2* tex_coords;
  uint count;
};

struct Scene {
  TexturedVertices ground_vertices;
  TexturedVertices sky_vertices;
  TexturedVertices crossties_vertices;
  ColoredVertices rails_vertices;
  std::vector<uint> rails_vertex_indices;
};

struct SceneConfig {
  float aabb_side_len;

  uint ground_tex_repeat_count;
  uint sky_tex_repeat_count;

  glm::vec4 rails_color;
  float rails_head_w;
  float rails_head_h;
  float rails_web_w;
  float rails_web_h;
  float rails_gauge;
  float rails_pos_offset_in_campath_norm_dir;

  float crossties_separation_dist;
  float crossties_pos_offset_in_campath_norm_dir;
};

void Make(const SceneConfig* cfg, const CameraPathVertices* campath,
          Scene* scene);

void Free(Scene* scene);

#endif  // RCOASTER_SCENE_HPP