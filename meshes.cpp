#include "meshes.hpp"

#include <cassert>
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
                                  Mesh1P1UV *mesh) {
  enum Corner { kBl, kTl, kTr, kBr, kCornerCount };

  static constexpr uint kVertexCountPerTriangle = 3;
  static constexpr uint kTriangleCount = 2;
  static constexpr uint kIndexCount = kVertexCountPerTriangle * kTriangleCount;

  assert(side_len > 0);
  assert(tex_repeat_count == 1 || tex_repeat_count % 2 == 0);
  assert(mesh);

  mesh->vertex_count = kCornerCount;
  mesh->vertices = new Vertex1P1UV[kCornerCount];

  mesh->vertices[kBl].position = {-side_len, 0, -side_len};
  mesh->vertices[kTl].position = {-side_len, 0, side_len};
  mesh->vertices[kTr].position = {side_len, 0, side_len};
  mesh->vertices[kBr].position = {side_len, 0, -side_len};
  for (int i = 0; i < kCornerCount; ++i) {
    mesh->vertices[i].position *= 0.5f;
  }

  mesh->vertices[kBl].uv = {0, 0};
  mesh->vertices[kTl].uv = {0, tex_repeat_count};
  mesh->vertices[kTr].uv = {tex_repeat_count, tex_repeat_count};
  mesh->vertices[kBr].uv = {tex_repeat_count, 0};

  mesh->index_count = kIndexCount;
  mesh->indices = new uint[kIndexCount];

  mesh->indices[0] = kBl;
  mesh->indices[1] = kBr;
  mesh->indices[2] = kTl;
  mesh->indices[3] = kBr;
  mesh->indices[4] = kTr;
  mesh->indices[5] = kTl;
}

void MakeAxisAlignedCube(float side_len, uint tex_repeat_count,
                         Mesh1P1UV *mesh) {
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

  mesh->vertex_count = kVertexCount;
  mesh->vertices = new Vertex1P1UV[kVertexCount];

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
    mesh->vertices[i].position = pos[i] * side_len;
  }

  glm::vec2 uniq_uv[kFaceCornerCount];
  uniq_uv[kBl] = {0, 0};
  uniq_uv[kTl] = {0, 1};
  uniq_uv[kBr] = {1, 0};
  uniq_uv[kTr] = {1, 1};

  for (uint i = 0; i < kVertexCount; i += kFaceCornerCount) {
    mesh->vertices[i].uv = uniq_uv[kBl];
    mesh->vertices[i + 1].uv = uniq_uv[kBr];
    mesh->vertices[i + 2].uv = uniq_uv[kTr];
    mesh->vertices[i + 3].uv = uniq_uv[kTl];
  }
  for (uint i = 0; i < kVertexCount; ++i) {
    mesh->vertices[i].uv *= tex_repeat_count;
  }

  mesh->index_count = kIndexCount;
  mesh->indices = new uint[kIndexCount];

  uint idx = 0;
  for (uint i = 0; i < kIndexCount; i += kIndicesPerFace) {
    mesh->indices[i] = idx;
    mesh->indices[i + 1] = idx + 2;
    mesh->indices[i + 2] = idx + 1;
    mesh->indices[i + 3] = idx;
    mesh->indices[i + 4] = idx + 3;
    mesh->indices[i + 5] = idx + 2;
    idx += kFaceCornerCount;
  }
}

void MakeRails(const VertexList1P1T1N1B *camspl_vertices,
               const glm::vec4 *color, float head_w, float head_h, float web_w,
               float web_h, float gauge, float pos_offset_in_camspl_norm_dir,
               Mesh1P1C *left_rail, Mesh1P1C *right_rail) {
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

  Mesh1P1C *rails[kRailType__Count] = {left_rail, right_rail};
  uint rv_count = cv_count * kCrossSectionVertexCount;

  for (int i = 0; i < kRailType__Count; ++i) {
    rails[i]->vertex_count = rv_count;
    rails[i]->vertices = new Vertex1P1C[rv_count];
  }

  for (int i = 0; i < kRailType__Count; ++i) {
    for (uint j = 0; j < rv_count; ++j) {
      rails[i]->vertices[j].color = *color;
    }
  }

  for (int i = 0; i < kRailType__Count; ++i) {
    Vertex1P1C *v = rails[i]->vertices;
    for (uint j = 0; j < cv_count; ++j) {
      uint k = j * kCrossSectionVertexCount;
      // See the comment block above the function declaration in the header
      // file for the visual index-to-position mapping.
      v[k].position =
          cv_pos[j] - web_h * cv_norm[j] + 0.5f * web_w * cv_binorm[j];
      v[k + 1].position = cv_pos[j] + 0.5f * web_w * cv_binorm[j];
      v[k + 2].position = cv_pos[j] + 0.5f * head_w * cv_binorm[j];
      v[k + 3].position =
          cv_pos[j] + head_h * cv_norm[j] + 0.5f * head_w * cv_binorm[j];
      v[k + 4].position =
          cv_pos[j] + head_h * cv_norm[j] - 0.5f * head_w * cv_binorm[j];
      v[k + 5].position = cv_pos[j] - 0.5f * head_w * cv_binorm[j];
      v[k + 6].position = cv_pos[j] - 0.5f * web_w * cv_binorm[j];
      v[k + 7].position =
          cv_pos[j] - web_h * cv_norm[j] - 0.5f * web_w * cv_binorm[j];
    }
  }

  // Set rail pair `gauge` distance apart.
  for (uint i = 0; i < cv_count; ++i) {
    uint j = kCrossSectionVertexCount * i;
    for (uint k = 0; k < kCrossSectionVertexCount; ++k) {
      right_rail->vertices[j + k].position += 0.5f * gauge * cv_binorm[i];
    }
  }
  for (uint i = 0; i < cv_count; ++i) {
    uint j = kCrossSectionVertexCount * i;
    for (uint k = 0; k < kCrossSectionVertexCount; ++k) {
      left_rail->vertices[j + k].position -= 0.5f * gauge * cv_binorm[i];
    }
  }

  for (int i = 0; i < kRailType__Count; ++i) {
    // glm::vec3 *pos = rails[i]->vertices.positions;
    Vertex1P1C *verts = rails[i]->vertices;
    for (uint j = 0; j < cv_count; ++j) {
      uint k = j * kCrossSectionVertexCount;
      for (uint l = 0; l < kCrossSectionVertexCount; ++l) {
        verts[k + l].position += pos_offset_in_camspl_norm_dir * cv_norm[j];
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
                   Mesh1P1UV *mesh) {
  enum CuboidCorner {
    kFbl,
    kFtl,
    kFtr,
    kFbr,
    kBbl,
    kBtl,
    kBtr,
    kBbr,
    kCuboidCornerCount
  };

  enum FaceCorner { kBl, kTl, kBr, kTr, kFaceCornerCount };

  static constexpr uint kFaceCount = 6;
  static constexpr uint kTrianglesPerFace = 2;

  static constexpr uint kIndicesPerTriangle = 3;
  static constexpr uint kIndicesPerFace =
      kIndicesPerTriangle * kTrianglesPerFace;

  static constexpr uint kUniqVerticesPerCrosstie =
      kFaceCount * kFaceCornerCount;
  static constexpr uint kIndicesPerCrosstie = kIndicesPerFace * kFaceCount;

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
  assert(mesh);

  glm::vec3 *cv_pos = camspl_vertices->positions;
  glm::vec3 *cv_tan = camspl_vertices->tangents;
  glm::vec3 *cv_binorm = camspl_vertices->binormals;
  glm::vec3 *cv_norm = camspl_vertices->normals;
  uint cv_count = camspl_vertices->count;

  uint max_vertex_count = kUniqVerticesPerCrosstie * (cv_count - 1);
  Vertex1P1UV *vertices = new Vertex1P1UV[max_vertex_count];

  uint max_index_count = kIndicesPerCrosstie * (cv_count - 1);
  uint *indices = new uint[max_index_count];

  float dist_moved = 0;
  uint vi = 0;
  uint indi = 0;
  for (uint i = 1; i < cv_count; ++i) {
    dist_moved += glm::length(cv_pos[i] - cv_pos[i - 1]);

    if (dist_moved < separation_dist + kTolerance) {
      continue;
    }

    glm::vec3 uniq_pos[kCuboidCornerCount];

    // Front vertices
    uniq_pos[kFbl] =
        cv_pos[i] - kHeight * cv_norm[i] - kHorizontalOffset * cv_binorm[i];
    uniq_pos[kFtl] = cv_pos[i] - kHorizontalOffset * cv_binorm[i];
    uniq_pos[kFtr] = cv_pos[i] + kHorizontalOffset * cv_binorm[i];
    uniq_pos[kFbr] =
        cv_pos[i] - kHeight * cv_norm[i] + kHorizontalOffset * cv_binorm[i];

    // Back vertices
    for (int j = kBbl; j < kCuboidCornerCount; ++j) {
      uniq_pos[j] = uniq_pos[j - kBbl] + kDepth * cv_tan[i];
    }
    for (int j = 0; j < kCuboidCornerCount; ++j) {
      uniq_pos[j] += pos_offset_in_camspl_norm_dir * cv_norm[i];
    }

    glm::vec3 pos[kUniqVerticesPerCrosstie] = {
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

    for (uint j = 0; j < kUniqVerticesPerCrosstie; ++j) {
      vertices[vi + j].position = pos[j];
    }

    glm::vec2 uniq_uv[kFaceCornerCount];
    uniq_uv[kBl] = {0, 0};
    uniq_uv[kTl] = {0, 1};
    uniq_uv[kBr] = {1, 0};
    uniq_uv[kTr] = {1, 1};

    for (uint j = 0; j < kUniqVerticesPerCrosstie; j += kFaceCornerCount) {
      uint k = vi + j;
      vertices[k].uv = uniq_uv[kBl];
      vertices[k + 1].uv = uniq_uv[kBr];
      vertices[k + 2].uv = uniq_uv[kTr];
      vertices[k + 3].uv = uniq_uv[kTl];
    }

    int ind = 0;
    for (uint j = 0; j < kIndicesPerCrosstie; j += kIndicesPerFace) {
      uint k = indi + j;
      indices[k] = ind;
      indices[k + 1] = ind + 1;
      indices[k + 2] = ind + 2;
      indices[k + 3] = ind;
      indices[k + 4] = ind + 2;
      indices[k + 5] = ind + 3;
      ind += kFaceCornerCount;
    }

    vi += kUniqVerticesPerCrosstie;
    indi += kIndicesPerCrosstie;

    dist_moved = 0;
  }

  mesh->vertex_count = vi;
  mesh->vertices = new Vertex1P1UV[vi];
  for (uint i = 0; i < mesh->vertex_count; ++i) {
    mesh->vertices[i].position = vertices[i].position;
    mesh->vertices[i].uv = vertices[i].uv;
  }
  delete[] vertices;

  mesh->index_count = indi;
  mesh->indices = new uint[indi];
  for (uint i = 0; i < mesh->index_count; ++i) {
    mesh->indices[i] = indices[i];
  }
  delete[] indices;
}