#ifndef RCOASTER_SCENE_HPP
#define RCOASTER_SCENE_HPP

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "meshes.hpp"
#include "status.hpp"
#include "types.hpp"

struct Entity {
  Mesh* mesh;
  glm::mat4 world_transform;
};

struct Scene {
  Entity camspl;
  Entity ground;
  Entity sky;
  Entity crossties;
  Entity left_rail;
  Entity right_rail;
};

struct SceneConfig {
  const char* track_filepath;
  float max_spline_segment_len;
  int is_verbose;

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
  float rails_pos_offset_in_camspl_norm_dir;

  glm::vec3 crossties_position;
  float crossties_separation_dist;
  float crossties_pos_offset_in_camspl_norm_dir;
};

Status MakeScene(const SceneConfig* cfg, Scene* scene);

void FreeModelVertices(Scene* scene);

#endif  // RCOASTER_SCENE_HPP