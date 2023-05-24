#include "models.hpp"

#include <cassert>
#include <cstdio>
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
                    float max_segment_len, VertexList *vertices) {
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
                                  VertexList *vertices) {
  static constexpr uint kVertexCount = 6;

  assert(side_len > 0);
  assert(tex_repeat_count == 1 || tex_repeat_count % 2 == 0);
  assert(vertices);

  vertices->positions = new glm::vec3[kVertexCount];
  vertices->tex_coords = new glm::vec2[kVertexCount];
  vertices->count = kVertexCount;

  glm::vec3 *pos = vertices->positions;
  glm::vec2 *texc = vertices->tex_coords;

  pos[0] = {-side_len, 0, side_len};
  pos[1] = {-side_len, 0, -side_len};
  pos[2] = {side_len, 0, -side_len};
  pos[3] = {-side_len, 0, side_len};
  pos[4] = {side_len, 0, -side_len};
  pos[5] = {side_len, 0, side_len};

  for (uint i = 0; i < kVertexCount; ++i) {
    pos[i] *= 0.5f;
  }

  texc[0] = {0, 0};
  texc[1] = {0, tex_repeat_count};
  texc[2] = {tex_repeat_count, tex_repeat_count};
  texc[3] = {0, 0};
  texc[4] = {tex_repeat_count, tex_repeat_count};
  texc[5] = {tex_repeat_count, 0};
}

void MakeAxisAlignedBox(float side_len, uint tex_repeat_count,
                        VertexList *vertices) {
  static constexpr uint kVertexCount = 36;

  assert(side_len > 0);
  assert(tex_repeat_count == 1 || tex_repeat_count % 2 == 0);
  assert(vertices);

  vertices->positions = new glm::vec3[kVertexCount];
  vertices->tex_coords = new glm::vec2[kVertexCount];
  vertices->count = kVertexCount;

  glm::vec3 *pos = vertices->positions;
  glm::vec2 *texc = vertices->tex_coords;

  uint i = 0;

  // x = -1 face
  pos[i] = {-side_len, -side_len, -side_len};
  pos[i + 1] = {-side_len, side_len, -side_len};
  pos[i + 2] = {-side_len, side_len, side_len};
  pos[i + 3] = {-side_len, -side_len, -side_len};
  pos[i + 4] = {-side_len, side_len, side_len};
  pos[i + 5] = {-side_len, -side_len, side_len};

  i += 6;

  // x = 1 face
  pos[i] = {side_len, -side_len, -side_len};
  pos[i + 1] = {side_len, side_len, -side_len};
  pos[i + 2] = {side_len, side_len, side_len};
  pos[i + 3] = {side_len, -side_len, -side_len};
  pos[i + 4] = {side_len, side_len, side_len};
  pos[i + 5] = {side_len, -side_len, side_len};

  i += 6;

  // y = -1 face
  pos[i] = {-side_len, -side_len, -side_len};
  pos[i + 1] = {-side_len, -side_len, side_len};
  pos[i + 2] = {side_len, -side_len, side_len};
  pos[i + 3] = {-side_len, -side_len, -side_len};
  pos[i + 4] = {side_len, -side_len, side_len};
  pos[i + 5] = {side_len, -side_len, -side_len};

  i += 6;

  // y = 1 face
  pos[i] = {-side_len, side_len, -side_len};
  pos[i + 1] = {-side_len, side_len, side_len};
  pos[i + 2] = {side_len, side_len, side_len};
  pos[i + 3] = {-side_len, side_len, -side_len};
  pos[i + 4] = {side_len, side_len, side_len};
  pos[i + 5] = {side_len, side_len, -side_len};

  i += 6;

  // z = -1 face
  pos[i] = {-side_len, -side_len, -side_len};
  pos[i + 1] = {-side_len, side_len, -side_len};
  pos[i + 2] = {side_len, side_len, -side_len};
  pos[i + 3] = {-side_len, -side_len, -side_len};
  pos[i + 4] = {side_len, side_len, -side_len};
  pos[i + 5] = {side_len, -side_len, -side_len};

  i += 6;

  // z = 1 face
  pos[i] = {-side_len, -side_len, side_len};
  pos[i + 1] = {-side_len, side_len, side_len};
  pos[i + 2] = {side_len, side_len, side_len};
  pos[i + 3] = {-side_len, -side_len, side_len};
  pos[i + 4] = {side_len, side_len, side_len};
  pos[i + 5] = {side_len, -side_len, side_len};

  for (uint j = 0; j < kVertexCount; ++j) {
    pos[j] *= 0.5f;
  }

  for (uint j = 0; j < kVertexCount; j += 6) {
    texc[j] = {0, 0};
    texc[j + 1] = {0, tex_repeat_count};
    texc[j + 2] = {tex_repeat_count, tex_repeat_count};
    texc[j + 3] = {0, 0};
    texc[j + 4] = {tex_repeat_count, tex_repeat_count};
    texc[j + 5] = {tex_repeat_count, 0};
  }
}

void MakeRails(const VertexList *campath_vertices, const glm::vec4 *color,
               float head_w, float head_h, float web_w, float web_h,
               float gauge, float pos_offset_in_campath_norm_dir,
               Mesh *left_rail, Mesh *right_rail) {
  static constexpr uint kCrossSectionVertexCount = 8;

  enum RailType { kRailType_Left, kRailType_Right, kRailType__Count };

  assert(campath_vertices);
  assert(campath_vertices->positions);
  assert(campath_vertices->normals);
  assert(campath_vertices->binormals);
  assert(color);
  assert(left_rail);
  assert(right_rail);

  assert(head_w > 0);
  assert(web_w > 0);
  assert(head_w > web_w);
  assert(gauge > 0);

  glm::vec3 *cv_pos = campath_vertices->positions;
  glm::vec3 *cv_norm = campath_vertices->normals;
  glm::vec3 *cv_binorm = campath_vertices->binormals;
  uint cv_count = campath_vertices->count;

  Mesh *rails[kRailType__Count] = {left_rail, right_rail};
  uint rv_count = cv_count * kCrossSectionVertexCount;

  for (int i = 0; i < kRailType__Count; ++i) {
    rails[i]->vertices.positions = new glm::vec3[rv_count];
    rails[i]->vertices.colors = new glm::vec4[rv_count];
    rails[i]->vertices.count = rv_count;
  }

  for (int i = 0; i < kRailType__Count; ++i) {
    for (uint j = 0; j < rv_count; ++j) {
      rails[i]->vertices.colors[j] = *color;
    }
  }

  for (int i = 0; i < kRailType__Count; ++i) {
    glm::vec3 *pos = rails[i]->vertices.positions;
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
      right_rail->vertices.positions[j + k] += 0.5f * gauge * cv_binorm[i];
    }
  }
  for (uint i = 0; i < cv_count; ++i) {
    uint j = kCrossSectionVertexCount * i;
    for (uint k = 0; k < kCrossSectionVertexCount; ++k) {
      left_rail->vertices.positions[j + k] -= 0.5f * gauge * cv_binorm[i];
    }
  }

  for (int i = 0; i < kRailType__Count; ++i) {
    glm::vec3 *pos = rails[i]->vertices.positions;
    for (uint j = 0; j < cv_count; ++j) {
      uint k = j * kCrossSectionVertexCount;
      for (uint l = 0; l < kCrossSectionVertexCount; ++l) {
        pos[k + l] += pos_offset_in_campath_norm_dir * cv_norm[j];
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

void MakeCrossties(const VertexList *campath_vertices, float separation_dist,
                   float pos_offset_in_campath_norm_dir, VertexList *vertices) {
  static constexpr float kAlpha = 0.1;
  static constexpr float kBeta = 1.5;
  static constexpr int kUniqPosCountPerCrosstie = 8;
  static constexpr float kBarDepth = 0.3;
  static constexpr float kTolerance = 0.00001;

  assert(campath_vertices);
  assert(campath_vertices->positions);
  assert(campath_vertices->tangents);
  assert(campath_vertices->normals);
  assert(campath_vertices->binormals);
  assert(separation_dist + kTolerance > 0);
  assert(vertices);

  glm::vec3 *cv_pos = campath_vertices->positions;
  glm::vec3 *cv_tan = campath_vertices->tangents;
  glm::vec3 *cv_binorm = campath_vertices->binormals;
  glm::vec3 *cv_norm = campath_vertices->normals;
  uint path_count = campath_vertices->count;

  uint max_vertex_count = 36 * (path_count - 1);
  glm::vec3 *pos = new glm::vec3[max_vertex_count];
  glm::vec2 *texc = new glm::vec2[max_vertex_count];

  glm::vec3 p[kUniqPosCountPerCrosstie];
  float dist_moved = 0;
  uint posi = 0;
  uint texci = 0;
  for (uint i = 1; i < path_count; ++i) {
    dist_moved += glm::length(cv_pos[i] - cv_pos[i - 1]);

    if (dist_moved < separation_dist + kTolerance) {
      continue;
    }

    p[0] = cv_pos[i] + kAlpha * (-kBeta * cv_norm[i] + cv_binorm[i] * 0.5f) +
           cv_binorm[i];
    p[1] =
        cv_pos[i] + kAlpha * (-cv_norm[i] + cv_binorm[i] * 0.5f) + cv_binorm[i];
    p[2] = cv_pos[i] + kAlpha * (-kBeta * cv_norm[i] + cv_binorm[i] * 0.5f) -
           cv_binorm[i] - kAlpha * cv_binorm[i];
    p[3] = cv_pos[i] + kAlpha * (-cv_norm[i] + cv_binorm[i] * 0.5f) -
           cv_binorm[i] - kAlpha * cv_binorm[i];

    p[4] = cv_pos[i] + kAlpha * (-kBeta * cv_norm[i] + cv_binorm[i] * 0.5f) +
           cv_binorm[i] + kBarDepth * cv_tan[i];
    p[5] = cv_pos[i] + kAlpha * (-cv_norm[i] + cv_binorm[i] * 0.5f) +
           cv_binorm[i] + kBarDepth * cv_tan[i];
    p[6] = cv_pos[i] + kAlpha * (-kBeta * cv_norm[i] + cv_binorm[i] * 0.5f) -
           cv_binorm[i] + kBarDepth * cv_tan[i] - kAlpha * cv_binorm[i];
    p[7] = cv_pos[i] + kAlpha * (-cv_norm[i] + cv_binorm[i] * 0.5f) -
           cv_binorm[i] + kBarDepth * cv_tan[i] - kAlpha * cv_binorm[i];

    for (uint j = 0; j < kUniqPosCountPerCrosstie; ++j) {
      p[j] += pos_offset_in_campath_norm_dir * cv_norm[i];
    }

    // Top face
    pos[posi] = p[6];
    pos[posi + 1] = p[5];
    pos[posi + 2] = p[2];
    pos[posi + 3] = p[5];
    pos[posi + 4] = p[1];
    pos[posi + 5] = p[2];

    posi += 6;

    // Right face
    pos[posi] = p[5];
    pos[posi + 1] = p[4];
    pos[posi + 2] = p[1];
    pos[posi + 3] = p[4];
    pos[posi + 4] = p[0];
    pos[posi + 5] = p[1];

    posi += 6;

    // Bottom face
    pos[posi] = p[4];
    pos[posi + 1] = p[7];
    pos[posi + 2] = p[0];
    pos[posi + 3] = p[7];
    pos[posi + 4] = p[3];
    pos[posi + 5] = p[0];

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
    pos[posi] = p[5];
    pos[posi + 1] = p[6];
    pos[posi + 2] = p[4];
    pos[posi + 3] = p[6];
    pos[posi + 4] = p[7];
    pos[posi + 5] = p[4];

    posi += 6;

    // Front face
    pos[posi] = p[2];
    pos[posi + 1] = p[1];
    pos[posi + 2] = p[3];
    pos[posi + 3] = p[1];
    pos[posi + 4] = p[0];
    pos[posi + 5] = p[3];

    posi += 6;

    for (uint j = 0; j < 6; ++j) {
      texc[texci] = glm::vec2(0, 1);
      texc[texci + 1] = glm::vec2(1, 1);
      texc[texci + 2] = glm::vec2(0, 0);
      texc[texci + 3] = glm::vec2(1, 1);
      texc[texci + 4] = glm::vec2(1, 0);
      texc[texci + 5] = glm::vec2(0, 0);

      texci += 6;
    }

    dist_moved = 0;
  }

  vertices->count = posi;

  vertices->positions = new glm::vec3[vertices->count];
  for (uint i = 0; i < vertices->count; ++i) {
    vertices->positions[i] = pos[i];
  }
  delete[] pos;

  vertices->tex_coords = new glm::vec2[vertices->count];
  for (uint i = 0; i < vertices->count; ++i) {
    vertices->tex_coords[i] = texc[i];
  }
  delete[] texc;
}