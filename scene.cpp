#include "scene.hpp"

#include <cassert>

void Make(const SceneConfig* cfg, const CameraPathVertices* campath,
          Scene* scene) {
  assert(cfg);
  assert(campath);
  assert(scene);

  MakeAxisAlignedXzSquarePlane(cfg->aabb_side_len, cfg->ground_tex_repeat_count,
                               &scene->ground_vertices);

  MakeAxisAlignedBox(cfg->aabb_side_len, cfg->sky_tex_repeat_count,
                     &scene->sky_vertices);

  MakeRails(campath, &cfg->rails_color, cfg->rails_head_w, cfg->rails_head_h,
            cfg->rails_web_w, cfg->rails_web_h, cfg->rails_gauge,
            cfg->rails_pos_offset_in_campath_norm_dir, &scene->rails_vertices);

  MakeCrossties(campath, cfg->crossties_separation_dist,
                cfg->crossties_pos_offset_in_campath_norm_dir,
                &scene->crossties_vertices);
}

void FreeVertices(Scene* scene) {
  assert(scene);

  delete[] scene->ground_vertices.positions;
  delete[] scene->ground_vertices.tex_coords;

  delete[] scene->sky_vertices.positions;
  delete[] scene->sky_vertices.tex_coords;

  delete[] scene->crossties_vertices.positions;
  delete[] scene->crossties_vertices.tex_coords;

  delete[] scene->rails_vertices.positions;
  delete[] scene->rails_vertices.colors;
}