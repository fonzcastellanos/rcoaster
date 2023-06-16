#include "meshes.hpp"

#include <cassert>
#include <cstring>
#include <glm/glm.hpp>
#include <vector>

constexpr float kTension = 0.5;
const glm::mat4x4 kCatmullRomBasis(-kTension, 2 - kTension, kTension - 2,
                                   kTension, 2 * kTension, kTension - 3,
                                   3 - 2 * kTension, -kTension, -kTension, 0,
                                   kTension, 0, 0, 1, 0, 0);

static glm::vec3 CatmullRomSplinePosition(float u, const glm::mat4x3 *control) {
  assert(control);

  glm::vec4 parameters(u * u * u, u * u, u, 1);
  return *control * kCatmullRomBasis * parameters;
}

static glm::vec3 CatmullRomSplineTangent(float u, const glm::mat4x3 *control) {
  assert(control);

  glm::vec4 parameters(3 * u * u, 2 * u, 1, 0);
  return *control * kCatmullRomBasis * parameters;
}

static void Subdivide(float u0, float u1, float max_segment_len,
                      const glm::mat4x3 *control,
                      std::vector<glm::vec3> *positions,
                      std::vector<glm::vec3> *tangents) {
  assert(control);
  assert(positions);
  assert(tangents);

#ifndef NDEBUG
  static constexpr float kTolerance = 0.00001;
#endif
  assert(max_segment_len + kTolerance > 0);

  glm::vec3 p0 = CatmullRomSplinePosition(u0, control);
  glm::vec3 p1 = CatmullRomSplinePosition(u1, control);

  if (glm::length(p1 - p0) > max_segment_len) {
    float umid = (u0 + u1) * 0.5f;

    Subdivide(u0, umid, max_segment_len, control, positions, tangents);
    Subdivide(umid, u1, max_segment_len, control, positions, tangents);
  } else {
    positions->push_back(p0);
    positions->push_back(p1);

    glm::vec3 t0 = CatmullRomSplineTangent(u0, control);
    glm::vec3 t1 = CatmullRomSplineTangent(u1, control);

    glm::vec3 t0_norm = glm::normalize(t0);
    glm::vec3 t1_norm = glm::normalize(t1);

    tangents->push_back(t0_norm);
    tangents->push_back(t1_norm);
  }
}

void EvalCatmullRomSpline(const glm::vec3 *control_points,
                          uint control_point_count, float max_segment_len,
                          glm::vec3 **positions, glm::vec3 **tangents,
                          uint *vertex_count) {
  assert(control_points);
  assert(positions);
  assert(tangents);
  assert(vertex_count);

#ifndef NDEBUG
  static constexpr float kTolerance = 0.00001;
#endif
  assert(max_segment_len + kTolerance > 0);

  std::vector<glm::vec3> positions_vec;
  std::vector<glm::vec3> tangents_vec;

  const glm::vec3 *cp = control_points;
  for (uint i = 1; i + 2 < control_point_count; ++i) {
    // clang-format off
    glm::mat4x3 control(
      cp[i - 1].x, cp[i - 1].y, cp[i - 1].z,
      cp[i].x, cp[i].y, cp[i].z,
      cp[i + 1].x, cp[i + 1].y, cp[i + 1].z,
      cp[i + 2].x, cp[i + 2].y, cp[i + 2].z
    );
    // clang-format on
    Subdivide(0, 1, max_segment_len, &control, &positions_vec, &tangents_vec);
  }

  assert(positions_vec.size() == tangents_vec.size());

  *vertex_count = positions_vec.size();
  *positions = new glm::vec3[*vertex_count];
  *tangents = new glm::vec3[*vertex_count];

  for (uint i = 0; i < *vertex_count; ++i) {
    (*positions)[i] = positions_vec[i];
    (*tangents)[i] = tangents_vec[i];
  }
}

void CalcCameraOrientation(const glm::vec3 *tangents, uint vertex_count,
                           glm::vec3 *normals, glm::vec3 *binormals) {
  assert(tangents);
  assert(normals);
  assert(binormals);
  assert(vertex_count != 0);

  // Initial binormal chosen arbitrarily.
  static const glm::vec3 kInitialBinormal = {0, 1, -0.5};

  normals[0] = glm::normalize(glm::cross(tangents[0], kInitialBinormal));
  binormals[0] = glm::normalize(glm::cross(tangents[0], normals[0]));

  for (uint i = 1; i < vertex_count; ++i) {
    normals[i] = glm::normalize(glm::cross(binormals[i - 1], tangents[i]));
    binormals[i] = glm::normalize(glm::cross(tangents[i], normals[i]));
  }
}

void MakeCameraPath(const glm::vec3 *control_points, uint control_point_count,
                    float max_segment_len, VertexList1P1T1N1B *vertices) {
#ifndef NDEBUG
  static constexpr float kTolerance = 0.00001;
#endif

  assert(control_points);
  assert(max_segment_len + kTolerance > 0);
  assert(vertices);

  EvalCatmullRomSpline(control_points, control_point_count, max_segment_len,
                       &vertices->positions, &vertices->tangents,
                       &vertices->count);

  vertices->normals = new glm::vec3[vertices->count];
  vertices->binormals = new glm::vec3[vertices->count];
  CalcCameraOrientation(vertices->tangents, vertices->count, vertices->normals,
                        vertices->binormals);
}

void MakeAxisAlignedXzSquarePlane(float side_len, uint tex_repeat_count,
                                  Mesh *mesh) {
  enum Corner { kBl, kTl, kTr, kBr, kCornerCount };

  static constexpr uint kVertexCountPerTriangle = 3;
  static constexpr uint kTriangleCount = 2;
  static constexpr uint kIndexCount = kVertexCountPerTriangle * kTriangleCount;

  assert(side_len > 0);
  assert(tex_repeat_count == 1 || tex_repeat_count % 2 == 0);
  assert(mesh);

  mesh->vertex_list_type = kVertexListType_1P1UV;
  mesh->vl1p1uv.count = kCornerCount;
  mesh->vl1p1uv.positions = new glm::vec3[kCornerCount];
  mesh->vl1p1uv.uv = new glm::vec2[kCornerCount];

  glm::vec3 *pos = mesh->vl1p1uv.positions;
  pos[kBl] = {-side_len, 0, -side_len};
  pos[kTl] = {-side_len, 0, side_len};
  pos[kTr] = {side_len, 0, side_len};
  pos[kBr] = {side_len, 0, -side_len};
  for (int i = 0; i < kCornerCount; ++i) {
    pos[i] *= 0.5f;
  }

  glm::vec2 *uv = mesh->vl1p1uv.uv;
  uv[kBl] = {0, 0};
  uv[kTl] = {0, tex_repeat_count};
  uv[kTr] = {tex_repeat_count, tex_repeat_count};
  uv[kBr] = {tex_repeat_count, 0};

  mesh->index_count = kIndexCount;
  mesh->indices = new uint[kIndexCount];

  uint *indices = mesh->indices;
  indices[0] = kBl;
  indices[1] = kBr;
  indices[2] = kTl;
  indices[3] = kBr;
  indices[4] = kTr;
  indices[5] = kTl;
}

void MakeAxisAlignedCube(float side_len, uint tex_repeat_count, Mesh *mesh) {
  enum CubeCorner {
    kFbl,
    kFtl,
    kFtr,
    kFbr,
    kBbl,
    kBtl,
    kBtr,
    kBbr,
    kCubeCornerCount
  };

  enum FaceCorner { kBl, kTl, kBr, kTr, kFaceCornerCount };

  static constexpr uint kFaceCount = 6;
  static constexpr uint kVertexCount = kFaceCount * kFaceCornerCount;

  static constexpr uint kTrianglesPerFace = 2;
  static constexpr uint kIndicesPerTriangle = 3;
  static constexpr uint kIndicesPerFace =
      kIndicesPerTriangle * kTrianglesPerFace;
  static constexpr uint kIndexCount = kFaceCount * kIndicesPerFace;

  assert(side_len > 0);
  assert(tex_repeat_count == 1 || tex_repeat_count % 2 == 0);
  assert(mesh);

  mesh->vertex_list_type = kVertexListType_1P1UV;
  mesh->vl1p1uv.count = kVertexCount;
  mesh->vl1p1uv.positions = new glm::vec3[kVertexCount];
  mesh->vl1p1uv.uv = new glm::vec2[kVertexCount];

  glm::vec3 uniq_pos[kCubeCornerCount];
  uniq_pos[kFbl] = {-0.5, -0.5, 0.5};
  uniq_pos[kFtl] = {-0.5, 0.5, 0.5};
  uniq_pos[kFtr] = {0.5, 0.5, 0.5};
  uniq_pos[kFbr] = {0.5, -0.5, 0.5};
  uniq_pos[kBbl] = {-0.5, -0.5, -0.5};
  uniq_pos[kBtl] = {-0.5, 0.5, -0.5};
  uniq_pos[kBtr] = {0.5, 0.5, -0.5};
  uniq_pos[kBbr] = {0.5, -0.5, -0.5};

  glm::vec3 pos[kVertexCount] = {
      // Back face
      uniq_pos[kBbl], uniq_pos[kBbr], uniq_pos[kBtr], uniq_pos[kBtl],

      // Front face
      uniq_pos[kFbl], uniq_pos[kFbr], uniq_pos[kFtr], uniq_pos[kFtl],

      // Left face
      uniq_pos[kBbl], uniq_pos[kFbl], uniq_pos[kFtl], uniq_pos[kBtl],

      // Right face
      uniq_pos[kFbr], uniq_pos[kBbr], uniq_pos[kBtr], uniq_pos[kFtr],

      // Top face
      uniq_pos[kFtl], uniq_pos[kFtr], uniq_pos[kBtr], uniq_pos[kBtl],

      // Bottom face
      uniq_pos[kBbl], uniq_pos[kBbr], uniq_pos[kFbr], uniq_pos[kFbl]};

  for (uint i = 0; i < kVertexCount; ++i) {
    pos[i] *= side_len;
  }

  std::memcpy(mesh->vl1p1uv.positions, pos, kVertexCount * sizeof(glm::vec3));

  glm::vec2 uniq_uv[kFaceCornerCount];
  uniq_uv[kBl] = {0, 0};
  uniq_uv[kTl] = {0, 1};
  uniq_uv[kBr] = {1, 0};
  uniq_uv[kTr] = {1, 1};

  glm::vec2 *uv = mesh->vl1p1uv.uv;
  for (uint i = 0; i < kVertexCount; i += kFaceCornerCount) {
    uv[i] = uniq_uv[kBl];
    uv[i + 1] = uniq_uv[kBr];
    uv[i + 2] = uniq_uv[kTr];
    uv[i + 3] = uniq_uv[kTl];
  }
  for (uint i = 0; i < kVertexCount; ++i) {
    uv[i] *= tex_repeat_count;
  }

  mesh->index_count = kIndexCount;
  mesh->indices = new uint[kIndexCount];

  uint *indices = mesh->indices;
  uint idx = 0;
  for (uint i = 0; i < kIndexCount; i += kIndicesPerFace) {
    indices[i] = idx;
    indices[i + 1] = idx + 2;
    indices[i + 2] = idx + 1;
    indices[i + 3] = idx;
    indices[i + 4] = idx + 3;
    indices[i + 5] = idx + 2;
    idx += kFaceCornerCount;
  }
}

void MakeRails(const VertexList1P1T1N1B *camspl_vertices,
               const glm::vec4 *color, float head_w, float head_h, float web_w,
               float web_h, float gauge, float pos_offset_in_camspl_norm_dir,
               Mesh *left_rail, Mesh *right_rail) {
  static constexpr uint kCrossSectionVertexCount = 8;

  enum RailType { kRailType_Left, kRailType_Right, kRailType__Count };

  assert(camspl_vertices);
  assert(camspl_vertices->positions);
  assert(camspl_vertices->normals);
  assert(camspl_vertices->binormals);
  assert(color);
  assert(left_rail);
  assert(right_rail);

  assert(head_w > 0);
  assert(web_w > 0);
  assert(head_w > web_w);
  assert(gauge > 0);

  glm::vec3 *cv_pos = camspl_vertices->positions;
  glm::vec3 *cv_norm = camspl_vertices->normals;
  glm::vec3 *cv_binorm = camspl_vertices->binormals;
  uint cv_count = camspl_vertices->count;

  Mesh *rails[kRailType__Count] = {left_rail, right_rail};
  uint rv_count = cv_count * kCrossSectionVertexCount;

  for (int i = 0; i < kRailType__Count; ++i) {
    rails[i]->vertex_list_type = kVertexListType_1P1C;
    rails[i]->vl1p1c.count = rv_count;
    rails[i]->vl1p1c.positions = new glm::vec3[rv_count];
    rails[i]->vl1p1c.colors = new glm::vec4[rv_count];
  }

  for (int i = 0; i < kRailType__Count; ++i) {
    for (uint j = 0; j < rv_count; ++j) {
      rails[i]->vl1p1c.colors[j] = *color;
    }
  }

  for (int i = 0; i < kRailType__Count; ++i) {
    glm::vec3 *pos = rails[i]->vl1p1c.positions;
    for (uint j = 0; j < cv_count; ++j) {
      uint k = j * kCrossSectionVertexCount;
      // See the comment block above the function declaration in the header
      // file for the visual index-to-position mapping.
      pos[k] = cv_pos[j] - web_h * cv_norm[j] + 0.5f * web_w * cv_binorm[j];
      pos[k + 1] = cv_pos[j] + 0.5f * web_w * cv_binorm[j];
      pos[k + 2] = cv_pos[j] + 0.5f * head_w * cv_binorm[j];
      pos[k + 3] =
          cv_pos[j] + head_h * cv_norm[j] + 0.5f * head_w * cv_binorm[j];
      pos[k + 4] =
          cv_pos[j] + head_h * cv_norm[j] - 0.5f * head_w * cv_binorm[j];
      pos[k + 5] = cv_pos[j] - 0.5f * head_w * cv_binorm[j];
      pos[k + 6] = cv_pos[j] - 0.5f * web_w * cv_binorm[j];
      pos[k + 7] = cv_pos[j] - web_h * cv_norm[j] - 0.5f * web_w * cv_binorm[j];
    }
  }

  // Set rail pair `gauge` distance apart.
  for (uint i = 0; i < cv_count; ++i) {
    uint j = kCrossSectionVertexCount * i;
    for (uint k = 0; k < kCrossSectionVertexCount; ++k) {
      right_rail->vl1p1c.positions[j + k] += 0.5f * gauge * cv_binorm[i];
    }
  }
  for (uint i = 0; i < cv_count; ++i) {
    uint j = kCrossSectionVertexCount * i;
    for (uint k = 0; k < kCrossSectionVertexCount; ++k) {
      left_rail->vl1p1c.positions[j + k] -= 0.5f * gauge * cv_binorm[i];
    }
  }

  for (int i = 0; i < kRailType__Count; ++i) {
    glm::vec3 *pos = rails[i]->vl1p1c.positions;
    for (uint j = 0; j < cv_count; ++j) {
      uint k = j * kCrossSectionVertexCount;
      for (uint l = 0; l < kCrossSectionVertexCount; ++l) {
        pos[k + l] += pos_offset_in_camspl_norm_dir * cv_norm[j];
      }
    }
  }

  static constexpr uint kFaceVertexCount = 6;
  static constexpr uint kFaceCount = 8;
  // kFaceVertexCount * kFaceCount = 48 indices for every
  // iteration of the inner loop, which processes 2 cross-sections
  // (kCrossSectionVertexCount * 2 = 16 vertices).
  //
  // rv_count / kCrossSectionVertexCount - 1 is the number of iterations for
  // the inner loop.

  uint index_count =
      (rv_count / kCrossSectionVertexCount - 1) * kFaceCount * kFaceVertexCount;

  for (int i = 0; i < kRailType__Count; ++i) {
    rails[i]->indices = new uint[index_count];
    rails[i]->index_count = index_count;

    uint *ri = rails[i]->indices;

    uint k = 0;
    for (uint j = 0; j + kCrossSectionVertexCount < rv_count;
         j += kCrossSectionVertexCount) {
      // Top face
      ri[k] = j + 4;
      ri[k + 1] = j + 12;
      ri[k + 2] = j + 3;
      ri[k + 3] = j + 12;
      ri[k + 4] = j + 11;
      ri[k + 5] = j + 3;

      k += 6;

      // Top right right face
      ri[k] = j + 3;
      ri[k + 1] = j + 11;
      ri[k + 2] = j + 2;
      ri[k + 3] = j + 11;
      ri[k + 4] = j + 10;
      ri[k + 5] = j + 2;

      k += 6;

      // Top right bottom face
      ri[k] = j + 2;
      ri[k + 1] = j + 10;
      ri[k + 2] = j + 1;
      ri[k + 3] = j + 10;
      ri[k + 4] = j + 9;
      ri[k + 5] = j + 1;

      k += 6;

      // Bottom right face
      ri[k] = j + 1;
      ri[k + 1] = j + 9;
      ri[k + 2] = j;
      ri[k + 3] = j + 9;
      ri[k + 4] = j + 8;
      ri[k + 5] = j;

      k += 6;

      // Bottom face
      ri[k] = j;
      ri[k + 1] = j + 8;
      ri[k + 2] = j + 7;
      ri[k + 3] = j + 8;
      ri[k + 4] = j + 15;
      ri[k + 5] = j + 7;

      k += 6;

      // Bottom left face
      ri[k] = j + 14;
      ri[k + 1] = j + 6;
      ri[k + 2] = j + 15;
      ri[k + 3] = j + 6;
      ri[k + 4] = j + 7;
      ri[k + 5] = j + 15;

      k += 6;

      // Top left bottom face
      ri[k] = j + 13;
      ri[k + 1] = j + 5;
      ri[k + 2] = j + 14;
      ri[k + 3] = j + 5;
      ri[k + 4] = j + 6;
      ri[k + 5] = j + 14;

      k += 6;

      // Top left left face
      ri[k] = j + 12;
      ri[k + 1] = j + 4;
      ri[k + 2] = j + 13;
      ri[k + 3] = j + 4;
      ri[k + 4] = j + 5;
      ri[k + 5] = j + 13;

      k += 6;
    }
  }
}

void MakeCrossties(const VertexList1P1T1N1B *camspl_vertices,
                   float separation_dist, float pos_offset_in_camspl_norm_dir,
                   VertexList1P1UV *vertices) {
  static constexpr int kUniqPosCountPerCrosstie = 8;
  static constexpr float kDepth = 0.3;
  static constexpr float kTolerance = 0.00001;

  static constexpr float kRailWebWidth = 0.1;
  static constexpr float kRailHeight = 0.1;
  static constexpr float kRailGauge = 2;
  static constexpr float kHeight = kRailHeight / 2;
  static constexpr float kHorizontalOffset = (kRailGauge - kRailWebWidth) / 2;

  assert(camspl_vertices);
  assert(camspl_vertices->positions);
  assert(camspl_vertices->tangents);
  assert(camspl_vertices->normals);
  assert(camspl_vertices->binormals);
  assert(separation_dist + kTolerance > 0);
  assert(vertices);

  glm::vec3 *cv_pos = camspl_vertices->positions;
  glm::vec3 *cv_tan = camspl_vertices->tangents;
  glm::vec3 *cv_binorm = camspl_vertices->binormals;
  glm::vec3 *cv_norm = camspl_vertices->normals;
  uint cv_count = camspl_vertices->count;

  uint max_vertex_count = 36 * (cv_count - 1);
  glm::vec3 *pos = new glm::vec3[max_vertex_count];
  glm::vec2 *uv = new glm::vec2[max_vertex_count];

  float dist_moved = 0;
  uint posi = 0;
  uint uvi = 0;
  for (uint i = 1; i < cv_count; ++i) {
    dist_moved += glm::length(cv_pos[i] - cv_pos[i - 1]);

    if (dist_moved < separation_dist + kTolerance) {
      continue;
    }

    glm::vec3 p[kUniqPosCountPerCrosstie];

    // front vertices
    p[0] = cv_pos[i] - kHeight * cv_norm[i] + kHorizontalOffset * cv_binorm[i];
    p[1] = cv_pos[i] + kHorizontalOffset * cv_binorm[i];
    p[2] = cv_pos[i] - kHorizontalOffset * cv_binorm[i];
    p[3] = cv_pos[i] - kHeight * cv_norm[i] - kHorizontalOffset * cv_binorm[i];

    // back vertices
    for (uint j = 0; j < 4; ++j) {
      p[j + 4] = p[j] + kDepth * cv_tan[i];
    }

    for (uint j = 0; j < kUniqPosCountPerCrosstie; ++j) {
      p[j] += pos_offset_in_camspl_norm_dir * cv_norm[i];
    }

    // Top face
    pos[posi] = p[2];
    pos[posi + 1] = p[6];
    pos[posi + 2] = p[1];
    pos[posi + 3] = p[6];
    pos[posi + 4] = p[5];
    pos[posi + 5] = p[1];

    posi += 6;

    // Right face
    pos[posi] = p[0];
    pos[posi + 1] = p[1];
    pos[posi + 2] = p[4];
    pos[posi + 3] = p[1];
    pos[posi + 4] = p[5];
    pos[posi + 5] = p[4];

    posi += 6;

    // Bottom face
    pos[posi] = p[7];
    pos[posi + 1] = p[3];
    pos[posi + 2] = p[4];
    pos[posi + 3] = p[3];
    pos[posi + 4] = p[0];
    pos[posi + 5] = p[4];

    posi += 6;

    // Left face
    pos[posi] = p[7];
    pos[posi + 1] = p[6];
    pos[posi + 2] = p[3];
    pos[posi + 3] = p[6];
    pos[posi + 4] = p[2];
    pos[posi + 5] = p[3];

    posi += 6;

    // Back face
    pos[posi] = p[7];
    pos[posi + 1] = p[6];
    pos[posi + 2] = p[4];
    pos[posi + 3] = p[6];
    pos[posi + 4] = p[5];
    pos[posi + 5] = p[4];

    posi += 6;

    // Front face
    pos[posi] = p[3];
    pos[posi + 1] = p[2];
    pos[posi + 2] = p[0];
    pos[posi + 3] = p[2];
    pos[posi + 4] = p[1];
    pos[posi + 5] = p[0];

    posi += 6;

    for (uint j = 0; j < 6; ++j) {
      uv[uvi] = glm::vec2(0, 0);
      uv[uvi + 1] = glm::vec2(0, 1);
      uv[uvi + 2] = glm::vec2(1, 0);
      uv[uvi + 3] = glm::vec2(0, 1);
      uv[uvi + 4] = glm::vec2(1, 1);
      uv[uvi + 5] = glm::vec2(1, 0);

      uvi += 6;
    }

    dist_moved = 0;
  }

  vertices->count = posi;

  vertices->positions = new glm::vec3[vertices->count];
  for (uint i = 0; i < vertices->count; ++i) {
    vertices->positions[i] = pos[i];
  }
  delete[] pos;

  vertices->uv = new glm::vec2[vertices->count];
  for (uint i = 0; i < vertices->count; ++i) {
    vertices->uv[i] = uv[i];
  }
  delete[] uv;
}