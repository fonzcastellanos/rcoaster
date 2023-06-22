#ifndef RCOASTER_MESHES_HPP
#define RCOASTER_MESHES_HPP

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "types.hpp"

struct Vertex1P1C {
  glm::vec3 position;
  glm::vec4 color;
};

struct Vertex1P1UV {
  glm::vec3 position;
  glm::vec2 uv;
};

struct VertexList1P1T1N1B {
  glm::vec3 *positions;
  glm::vec3 *tangents;
  glm::vec3 *normals;
  glm::vec3 *binormals;
  uint count;
};

enum MeshType {
  kMeshType_1P1C,
  kMeshType_1P1UV,
  kMeshType_1P1T1N1B,
  kMeshType__Count
};

struct Mesh1P1C {
  Vertex1P1C *vertices;
  uint vertex_count;
  uint *indices;
  uint index_count;
};

struct Mesh1P1UV {
  Vertex1P1UV *vertices;
  uint vertex_count;
  uint *indices;
  uint index_count;
};

struct Mesh1P1T1N1B {
  VertexList1P1T1N1B vertices;
  uint *indices;
  uint index_count;
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
                                  Mesh1P1UV *mesh);

void MakeAxisAlignedCube(float side_len, uint tex_repeat_count,
                         Mesh1P1UV *mesh);

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
               Mesh1P1C *left_rail, Mesh1P1C *right_rail);

void MakeCrossties(const VertexList1P1T1N1B *camspl_vertices,
                   float separation_dist, float pos_offset_in_camspl_norm_dir,
                   Vertex1P1UV **vertices, uint *vertex_count);

#endif  // RCOASTER_MESHES_HPP