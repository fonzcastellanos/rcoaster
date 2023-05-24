#ifndef RCOASTER_MODELS_HPP
#define RCOASTER_MODELS_HPP

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "types.hpp"

struct VertexList {
  glm::vec3 *positions;
  glm::vec3 *tangents;
  glm::vec3 *normals;
  glm::vec3 *binormals;
  glm::vec4 *colors;
  glm::vec2 *tex_coords;
  uint count;
};

struct Mesh {
  VertexList vertices;
  uint *indices;
  uint index_count;
};

struct Model {
  Mesh mesh;
  glm::vec3 position;
};

void EvalCatmullRomSpline(const glm::vec3 *control_points,
                          uint control_point_count, float max_segment_len,
                          glm::vec3 **positions, glm::vec3 **tangents,
                          uint *vertices_count);

void CalcCameraOrientation(const glm::vec3 *tangents, uint vertex_count,
                           glm::vec3 *normals, glm::vec3 *binormals);

void MakeCameraPath(const glm::vec3 *control_points, uint control_point_count,
                    float max_segment_len, VertexList *vertices);

void MakeAxisAlignedXzSquarePlane(float side_len, uint tex_repeat_count,
                                  VertexList *vertices);

void MakeAxisAlignedBox(float side_len, uint tex_repeat_count,
                        VertexList *vertices);
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
void MakeRails(const VertexList *campath_vertices, const glm::vec4 *color,
               float head_w, float head_h, float web_w, float web_h,
               float gauge, float pos_offset_in_campath_normal_dir,
               Mesh *left_rail, Mesh *right_rail);

void MakeCrossties(const VertexList *campath_vertices, float separation_dist,
                   float pos_offset_in_campath_normal_dir,
                   VertexList *vertices);

#endif  // RCOASTER_MODELS_HPP