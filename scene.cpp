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
  scene->camspl.mesh_1p1t1n1b = new Mesh1P1T1N1B;
  MakeCameraPath(splines[0].data(), splines[0].size(),
                 cfg->max_spline_segment_len,
                 &scene->camspl.mesh_1p1t1n1b->vertices);

  scene->mesh_1p1uv_count = 2;
  scene->meshes_1p1uv = new Mesh1P1UV[scene->mesh_1p1uv_count];

  scene->ground.mesh_1p1uv = &scene->meshes_1p1uv[0];
  MakeAxisAlignedXzSquarePlane(cfg->aabb_side_len, cfg->ground_tex_repeat_count,
                               scene->ground.mesh_1p1uv);
  scene->ground.world_transform =
      glm::translate(glm::mat4(1), cfg->ground_position);

  scene->sky.mesh_1p1uv = &scene->meshes_1p1uv[1];
  MakeAxisAlignedCube(cfg->aabb_side_len, cfg->sky_tex_repeat_count,
                      scene->sky.mesh_1p1uv);
  scene->sky.world_transform = glm::translate(glm::mat4(1), cfg->sky_position);

  scene->mesh_1p1c_count = 2;
  scene->meshes_1p1c = new Mesh1P1C[scene->mesh_1p1c_count];

  scene->left_rail.mesh_1p1c = &scene->meshes_1p1c[0];
  scene->right_rail.mesh_1p1c = &scene->meshes_1p1c[1];
  MakeRails(&scene->camspl.mesh_1p1t1n1b->vertices, &cfg->rails_color,
            cfg->rails_head_w, cfg->rails_head_h, cfg->rails_web_w,
            cfg->rails_web_h, cfg->rails_gauge,
            cfg->rails_pos_offset_in_camspl_norm_dir,
            scene->left_rail.mesh_1p1c, scene->right_rail.mesh_1p1c);
  scene->left_rail.world_transform =
      glm::translate(glm::mat4(1), cfg->rails_position);
  scene->right_rail.world_transform =
      glm::translate(glm::mat4(1), cfg->rails_position);

  scene->crossties.mesh_1p1uv = new Mesh1P1UV;
  MakeCrossties(&scene->camspl.mesh_1p1t1n1b->vertices,
                cfg->crossties_separation_dist,
                cfg->crossties_pos_offset_in_camspl_norm_dir,
                &scene->crossties.mesh_1p1uv->vertices);
  scene->crossties.world_transform =
      glm::translate(glm::mat4(1), cfg->crossties_position);

  return kStatus_Ok;
}

void FreeModelVertices(Scene *scene) {
  assert(scene);

  delete[] scene->meshes_1p1uv;

  // delete[] scene->ground.mesh_1p1uv->vertices.positions;
  // delete[] scene->ground.mesh_1p1uv->vertices.uv;
  delete[] scene->ground.mesh_1p1uv->indices;

  // delete[] scene->sky.mesh_1p1uv->vertices.positions;
  // delete[] scene->sky.mesh_1p1uv->vertices.uv;
  delete[] scene->sky.mesh_1p1uv->indices;

  delete[] scene->crossties.mesh_1p1uv->vertices.positions;
  delete[] scene->crossties.mesh_1p1uv->vertices.uv;

  delete[] scene->left_rail.mesh_1p1c->vertices.positions;
  delete[] scene->left_rail.mesh_1p1c->vertices.colors;
  delete[] scene->left_rail.mesh_1p1c->indices;

  delete[] scene->right_rail.mesh_1p1c->vertices.positions;
  delete[] scene->right_rail.mesh_1p1c->vertices.colors;
  delete[] scene->right_rail.mesh_1p1c->indices;
}