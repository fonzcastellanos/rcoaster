#ifndef RCOASTER_MODELS_HPP
#define RCOASTER_MODELS_HPP

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "types.hpp"

enum VertexListType {
  kVertexListType_1P1C,
  kVertexListType_1P1UV,
  kVertexListType_1P1T1N1B
};

struct VertexList1P1C {
  glm::vec3 *positions;
  glm::vec4 *colors;
  uint count;
};

struct VertexList1P1UV {
  glm::vec3 *positions;
  glm::vec2 *uv;
  uint count;
};

struct VertexList1P1T1N1B {
  glm::vec3 *positions;
  glm::vec3 *tangents;
  glm::vec3 *normals;
  glm::vec3 *binormals;
  uint count;
};

struct Mesh {
  VertexListType vertex_list_type;
  union {
    VertexList1P1C vl1p1c;
    VertexList1P1UV vl1p1uv;
    VertexList1P1T1N1B vl1p1t1n1b;
  };
  uint *indices;
  uint index_count;
};

struct MeshInstance {
  Mesh *mesh;
  glm::mat4 world_transform;
};

void EvalCatmullRomSpline(const glm::vec3 *control_points,
                          uint control_point_count, float max_segment_len,
                          glm::vec3 **positions, glm::vec3 **tangents,
                          uint *vertices_count);

void CalcCameraOrientation(const glm::vec3 *tangents, uint vertex_count,
                           glm::vec3 *normals, glm::vec3 *binormals);

void MakeCameraPath(const glm::vec3 *control_points, uint control_point_count,
                    float max_segment_len, VertexList1P1T1N1B *vertices);

void MakeAxisAlignedXzSquarePlane(float side_len, uint tex_repeat_count,
                                  VertexList1P1UV *vertices);

void MakeAxisAlignedBox(float side_len, uint tex_repeat_count,
                        VertexList1P1UV *vertices);
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

  `o` marks the camera spline vertex, which is used as the origin.
  The gauge is the distance between points `c` and `d`.

                        gauge
          |-------------------------------|
  4 --------------- 3             4 --------------- 3
  |                 |             |                 |
  |                 |             |                 |
  5 -- 6   c   1 -- 2      o      5 -- 6   d   1 -- 2
       |       |                       |       |
       |       |                       |       |
       7 ----- 0                       7 ----- 0
*/
void MakeRails(const VertexList1P1T1N1B *camspl_vertices,
               const glm::vec4 *color, float head_w, float head_h, float web_w,
               float web_h, float gauge, float pos_offset_in_camspl_norm_dir,
               Mesh *left_rail, Mesh *right_rail);

void MakeCrossties(const VertexList1P1T1N1B *camspl_vertices,
                   float separation_dist, float pos_offset_in_camspl_norm_dir,
                   VertexList1P1UV *vertices);

#endif  // RCOASTER_MODELS_HPP