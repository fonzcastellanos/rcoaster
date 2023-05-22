#include "scene.hpp"

#include <cassert>

#include "models.hpp"

uint Count(const ColoredVertices* v) {
  assert(v);
  assert(v->positions.size() == v->colors.size());

  return v->positions.size();
}

void Make(const SceneConfig* cfg, const CameraPathVertices* campath,
          Scene* scene) {
  constexpr uint kGroundVertexCount = 6;
  constexpr uint kSkyVertexCount = 36;

  assert(cfg);
  assert(campath);
  assert(scene);

  scene->ground_vertices.count = kGroundVertexCount;
  scene->ground_vertices.positions = new glm::vec3[kGroundVertexCount];
  scene->ground_vertices.tex_coords = new glm::vec2[kGroundVertexCount];
  MakeAxisAlignedXzSquarePlane(cfg->aabb_side_len, cfg->ground_tex_repeat_count,
                               scene->ground_vertices.positions,
                               scene->ground_vertices.tex_coords);

  scene->sky_vertices.count = kSkyVertexCount;
  scene->sky_vertices.positions = new glm::vec3[kSkyVertexCount];
  scene->sky_vertices.tex_coords = new glm::vec2[kSkyVertexCount];
  MakeAxisAlignedBox(cfg->aabb_side_len, cfg->sky_tex_repeat_count,
                     scene->sky_vertices.positions,
                     scene->sky_vertices.tex_coords);

  MakeRails(campath, &cfg->rails_color, cfg->rails_head_w, cfg->rails_head_h,
            cfg->rails_web_w, cfg->rails_web_h, cfg->rails_gauge,
            cfg->rails_pos_offset_in_campath_norm_dir,
            &scene->rails_vertices.positions, &scene->rails_vertices.colors,
            &scene->rails_vertex_indices);

  MakeCrossties(campath, cfg->crossties_separation_dist,
                cfg->crossties_pos_offset_in_campath_norm_dir,
                &scene->crossties_vertices.positions,
                &scene->crossties_vertices.tex_coords,
                &scene->crossties_vertices.count);
}

void Free(Scene* scene) {
  delete[] scene->ground_vertices.positions;
  delete[] scene->ground_vertices.tex_coords;

  delete[] scene->sky_vertices.positions;
  delete[] scene->sky_vertices.tex_coords;

  delete[] scene->crossties_vertices.positions;
  delete[] scene->crossties_vertices.tex_coords;
}