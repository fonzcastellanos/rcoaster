#include "scene.hpp"

#include <cassert>
#include <cstdio>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <vector>

static Status LoadSplines(const char *track_filepath,
                          std::vector<std::vector<glm::vec3>> *splines) {
  assert(track_filepath);
  assert(splines);

  auto &splines_ = *splines;

  std::FILE *track_file = std::fopen(track_filepath, "r");
  if (!track_file) {
    std::fprintf(stderr, "Failed to open track file %s.\n", track_filepath);
    return kStatus_IoError;
  }

  uint spline_count;
  int rc = std::fscanf(track_file, "%u", &spline_count);
  if (rc < 1) {
    std::fprintf(stderr, "Failed to read spline count from track file %s.\n",
                 track_filepath);
    std::fclose(track_file);
    return kStatus_IoError;
  }

  splines->resize(spline_count);

  char filepath[4096];
  for (uint i = 0; i < spline_count; ++i) {
    rc = std::fscanf(track_file, "%s", filepath);
    if (rc < 1) {
      std::fprintf(stderr,
                   "Failed to read path of spline file from track file %s.\n",
                   track_filepath);
      std::fclose(track_file);
      return kStatus_IoError;
    }

    FILE *file = std::fopen(filepath, "r");
    if (!file) {
      std::fprintf(stderr, "Failed to open spline file %s.\n", filepath);
      std::fclose(track_file);
      return kStatus_IoError;
    }

    uint ctrl_point_count;
    uint type;
    rc = std::fscanf(file, "%u %u", &ctrl_point_count, &type);
    if (rc < 1) {
      std::fprintf(stderr,
                   "Failed to read control point count and spline type from "
                   "spline file %s.\n",
                   filepath);
      std::fclose(file);
      std::fclose(track_file);
      return kStatus_IoError;
    }

    splines_[i].resize(ctrl_point_count);

    uint j = 0;
    while ((rc = std::fscanf(file, "%f %f %f", &splines_[i][j].x,
                             &splines_[i][j].y, &splines_[i][j].z)) > 0) {
      ++j;
    }
    if (rc == 0) {
      std::fprintf(stderr,
                   "Failed to read control point from spline file %s.\n",
                   filepath);
      std::fclose(file);
      std::fclose(track_file);
      return kStatus_IoError;
    }

    std::fclose(file);
  }

  std::fclose(track_file);

  return kStatus_Ok;
}

Status MakeScene(const SceneConfig *cfg, Scene *scene) {
  assert(cfg);
  assert(scene);

  std::vector<std::vector<glm::vec3>> splines;
  Status status = LoadSplines(cfg->track_filepath, &splines);
  if (status != kStatus_Ok) {
    std::fprintf(stderr, "Could not load splines.\n");
    return status;
  }

  if (cfg->is_verbose) {
    std::printf("Loaded spline count: %lu\n", splines.size());
    for (uint i = 0; i < splines.size(); ++i) {
      std::printf("Control point count in spline %u: %lu\n", i,
                  splines[i].size());
    }
  }

  // TODO: Support for multiple splines.
  assert(splines.size() == 1);
  MakeCameraPath(splines[0].data(), splines[0].size(),
                 cfg->max_spline_segment_len, &scene->camspl.vertices);

  scene->ground.mesh = new Mesh;
  MakeAxisAlignedXzSquarePlane(cfg->aabb_side_len, cfg->ground_tex_repeat_count,
                               &scene->ground.mesh->vl1p1uv);

  scene->ground.world_transform =
      glm::translate(glm::mat4(1), cfg->ground_position);

  scene->sky.mesh = new Mesh;
  MakeAxisAlignedBox(cfg->aabb_side_len, cfg->sky_tex_repeat_count,
                     &scene->sky.mesh->vl1p1uv);
  scene->sky.world_transform = glm::translate(glm::mat4(1), cfg->sky_position);

  scene->left_rail.mesh = new Mesh;
  scene->right_rail.mesh = new Mesh;
  MakeRails(&scene->camspl.vertices, &cfg->rails_color, cfg->rails_head_w,
            cfg->rails_head_h, cfg->rails_web_w, cfg->rails_web_h,
            cfg->rails_gauge, cfg->rails_pos_offset_in_camspl_norm_dir,
            scene->left_rail.mesh, scene->right_rail.mesh);
  scene->left_rail.world_transform =
      glm::translate(glm::mat4(1), cfg->rails_position);
  scene->right_rail.world_transform =
      glm::translate(glm::mat4(1), cfg->rails_position);

  scene->crossties.mesh = new Mesh;
  MakeCrossties(&scene->camspl.vertices, cfg->crossties_separation_dist,
                cfg->crossties_pos_offset_in_camspl_norm_dir,
                &scene->crossties.mesh->vl1p1uv);
  scene->crossties.world_transform =
      glm::translate(glm::mat4(1), cfg->crossties_position);

  return kStatus_Ok;
}

void FreeModelVertices(Scene *scene) {
  assert(scene);

  delete[] scene->ground.mesh->vl1p1uv.positions;
  delete[] scene->ground.mesh->vl1p1uv.uv;

  delete[] scene->sky.mesh->vl1p1uv.positions;
  delete[] scene->sky.mesh->vl1p1uv.uv;

  delete[] scene->crossties.mesh->vl1p1uv.positions;
  delete[] scene->crossties.mesh->vl1p1uv.uv;

  delete[] scene->left_rail.mesh->vl1p1c.positions;
  delete[] scene->left_rail.mesh->vl1p1c.colors;
  delete[] scene->left_rail.mesh->indices;

  delete[] scene->right_rail.mesh->vl1p1c.positions;
  delete[] scene->right_rail.mesh->vl1p1c.colors;
  delete[] scene->right_rail.mesh->indices;
}