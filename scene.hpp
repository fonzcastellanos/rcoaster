#ifndef RCOASTER_SCENE_HPP
#define RCOASTER_SCENE_HPP

#include <glm/vec4.hpp>

#include "models.hpp"
#include "types.hpp"

struct Scene {
  TexturedModel ground;
  TexturedModel sky;
  TexturedModel crossties;
  ColoredModel rails;
};

struct SceneConfig {
  float aabb_side_len;

  glm::vec3 ground_position;
  uint ground_tex_repeat_count;

  glm::vec3 sky_position;
  uint sky_tex_repeat_count;

  glm::vec3 rails_position;
  glm::vec4 rails_color;
  float rails_head_w;
  float rails_head_h;
  float rails_web_w;
  float rails_web_h;
  float rails_gauge;
  float rails_pos_offset_in_campath_norm_dir;

  glm::vec3 crossties_position;
  float crossties_separation_dist;
  float crossties_pos_offset_in_campath_norm_dir;
};

void Make(const SceneConfig* cfg, const CameraPathVertices* campath,
          Scene* scene);

void FreeVertices(Scene* scene);

#endif  // RCOASTER_SCENE_HPP