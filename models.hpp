#ifndef RCOASTER_MODELS_HPP
#define RCOASTER_MODELS_HPP

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <vector>

#include "types.hpp"

struct CameraPathVertices {
  std::vector<glm::vec3> positions;
  std::vector<glm::vec3> tangents;
  std::vector<glm::vec3> normals;
  std::vector<glm::vec3> binormals;
};

uint Count(const CameraPathVertices *c);

void EvalCatmullRomSpline(const std::vector<glm::vec3> *control_points,
                          float max_line_len, std::vector<glm::vec3> *positions,
                          std::vector<glm::vec3> *tangents);

/*
Makes an axis-aligned xz square plane centered at the origin.

6 positions and 6 texture coordinates will be produced.
*/
void MakeAxisAlignedXzSquarePlane(float side_len, uint tex_repeat_count,
                                  glm::vec3 *positions, glm::vec2 *tex_coords);

/*
Makes an axis-aligned box centered at the origin.

36 positions and 36 texture coordinates will be produced.
*/
void MakeAxisAlignedBox(float side_len, uint tex_repeat_count,
                        glm::vec3 *positions, glm::vec2 *tex_coords);

void CameraOrientation(const std::vector<glm::vec3> *tangents,
                       std::vector<glm::vec3> *normals,
                       std::vector<glm::vec3> *binormals);

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
void MakeRails(const CameraPathVertices *campath, const glm::vec4 *color,
               float head_w, float head_h, float web_w, float web_h,
               float gauge, float pos_offset_in_campath_norm_dir,
               std::vector<glm::vec3> *positions,
               std::vector<glm::vec4> *colors, std::vector<uint> *indices);

void MakeCrossties(const CameraPathVertices *campath, float separation_dist,
                   float pos_offset_in_campath_norm_dir, glm::vec3 **positions,
                   glm::vec2 **tex_coords, uint *count);

#endif  // RCOASTER_MODELS_HPP