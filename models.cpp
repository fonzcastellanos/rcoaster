#include "models.hpp"

#include <cassert>
#include <glm/glm.hpp>

uint Count(const CameraPathVertices *v) {
  assert(v);
  assert(v->positions.size() == v->tangents.size());
  assert(v->tangents.size() == v->normals.size());
  assert(v->normals.size() == v->binormals.size());

  return v->positions.size();
}

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

static void Subdivide(float u0, float u1, float max_line_len,
                      const glm::mat4x3 *control,
                      std::vector<glm::vec3> *positions,
                      std::vector<glm::vec3> *tangents) {
  assert(control);
  assert(positions);
  assert(tangents);

#ifndef NDEBUG
  static constexpr float kTolerance = 0.00001;
#endif
  assert(max_line_len + kTolerance > 0);

  glm::vec3 p0 = CatmullRomSplinePosition(u0, control);
  glm::vec3 p1 = CatmullRomSplinePosition(u1, control);

  if (glm::length(p1 - p0) > max_line_len) {
    float umid = (u0 + u1) * 0.5f;

    Subdivide(u0, umid, max_line_len, control, positions, tangents);
    Subdivide(umid, u1, max_line_len, control, positions, tangents);
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

void EvalCatmullRomSpline(const std::vector<glm::vec3> *spline,
                          float max_line_len, std::vector<glm::vec3> *positions,
                          std::vector<glm::vec3> *tangents) {
  assert(spline);
  assert(positions);
  assert(tangents);

#ifndef NDEBUG
  static constexpr float kTolerance = 0.00001;
#endif
  assert(max_line_len + kTolerance > 0);

  auto &spline_ = *spline;

  for (uint i = 1; i + 2 < spline->size(); ++i) {
    // clang-format off
    glm::mat4x3 control(
      spline_[i - 1].x, spline_[i - 1].y, spline_[i - 1].z,
      spline_[i].x, spline_[i].y, spline_[i].z,
      spline_[i + 1].x, spline_[i + 1].y, spline_[i + 1].z,
      spline_[i + 2].x, spline_[i + 2].y, spline_[i + 2].z
    );
    // clang-format on
    Subdivide(0, 1, max_line_len, &control, positions, tangents);
  }
}

void MakeAxisAlignedXzSquarePlane(float side_len, uint tex_repeat_count,
                                  glm::vec3 *positions, glm::vec2 *tex_coords) {
  assert(side_len > 0);
  assert(tex_repeat_count == 1 || tex_repeat_count % 2 == 0);
  assert(positions);
  assert(tex_coords);

  positions[0] = {-side_len, 0, side_len};
  positions[1] = {-side_len, 0, -side_len};
  positions[2] = {side_len, 0, -side_len};
  positions[3] = {-side_len, 0, side_len};
  positions[4] = {side_len, 0, -side_len};
  positions[5] = {side_len, 0, side_len};

  for (uint i = 0; i < 6; ++i) {
    positions[i] *= 0.5f;
  }

  tex_coords[0] = {0, 0};
  tex_coords[1] = {0, tex_repeat_count};
  tex_coords[2] = {tex_repeat_count, tex_repeat_count};
  tex_coords[3] = {0, 0};
  tex_coords[4] = {tex_repeat_count, tex_repeat_count};
  tex_coords[5] = {tex_repeat_count, 0};
}

void MakeAxisAlignedBox(float side_len, uint tex_repeat_count,
                        glm::vec3 *positions, glm::vec2 *tex_coords) {
  assert(side_len > 0);
  assert(positions);
  assert(tex_coords);
  assert(tex_repeat_count == 1 || tex_repeat_count % 2 == 0);

  uint i = 0;

  // x = -1 face
  positions[i] = {-side_len, -side_len, -side_len};
  positions[i + 1] = {-side_len, side_len, -side_len};
  positions[i + 2] = {-side_len, side_len, side_len};
  positions[i + 3] = {-side_len, -side_len, -side_len};
  positions[i + 4] = {-side_len, side_len, side_len};
  positions[i + 5] = {-side_len, -side_len, side_len};

  i += 6;

  // x = 1 face
  positions[i] = {side_len, -side_len, -side_len};
  positions[i + 1] = {side_len, side_len, -side_len};
  positions[i + 2] = {side_len, side_len, side_len};
  positions[i + 3] = {side_len, -side_len, -side_len};
  positions[i + 4] = {side_len, side_len, side_len};
  positions[i + 5] = {side_len, -side_len, side_len};

  i += 6;

  // y = -1 face
  positions[i] = {-side_len, -side_len, -side_len};
  positions[i + 1] = {-side_len, -side_len, side_len};
  positions[i + 2] = {side_len, -side_len, side_len};
  positions[i + 3] = {-side_len, -side_len, -side_len};
  positions[i + 4] = {side_len, -side_len, side_len};
  positions[i + 5] = {side_len, -side_len, -side_len};

  i += 6;

  // y = 1 face
  positions[i] = {-side_len, side_len, -side_len};
  positions[i + 1] = {-side_len, side_len, side_len};
  positions[i + 2] = {side_len, side_len, side_len};
  positions[i + 3] = {-side_len, side_len, -side_len};
  positions[i + 4] = {side_len, side_len, side_len};
  positions[i + 5] = {side_len, side_len, -side_len};

  i += 6;

  // z = -1 face
  positions[i] = {-side_len, -side_len, -side_len};
  positions[i + 1] = {-side_len, side_len, -side_len};
  positions[i + 2] = {side_len, side_len, -side_len};
  positions[i + 3] = {-side_len, -side_len, -side_len};
  positions[i + 4] = {side_len, side_len, -side_len};
  positions[i + 5] = {side_len, -side_len, -side_len};

  i += 6;

  // z = 1 face
  positions[i] = {-side_len, -side_len, side_len};
  positions[i + 1] = {-side_len, side_len, side_len};
  positions[i + 2] = {side_len, side_len, side_len};
  positions[i + 3] = {-side_len, -side_len, side_len};
  positions[i + 4] = {side_len, side_len, side_len};
  positions[i + 5] = {side_len, -side_len, side_len};

  for (uint j = 0; j < 36; ++j) {
    positions[j] *= 0.5f;
  }

  for (uint j = 0; j < 6; ++j) {
    uint k = j * 6;
    tex_coords[k] = {0, 0};
    tex_coords[k + 1] = {0, tex_repeat_count};
    tex_coords[k + 2] = {tex_repeat_count, tex_repeat_count};
    tex_coords[k + 3] = {0, 0};
    tex_coords[k + 4] = {tex_repeat_count, tex_repeat_count};
    tex_coords[k + 5] = {tex_repeat_count, 0};
  }
}

void CameraOrientation(const std::vector<glm::vec3> *tangents,
                       std::vector<glm::vec3> *normals,
                       std::vector<glm::vec3> *binormals) {
  assert(tangents);
  assert(normals);
  assert(binormals);
  assert(!tangents->empty());

  // Initial binormal chosen arbitrarily.
  static const glm::vec3 kInitialBinormal = {0, 1, -0.5};

  normals->resize(tangents->size());
  binormals->resize(tangents->size());

  const auto &tangents_ = *tangents;
  auto &normals_ = *normals;
  auto &binormals_ = *binormals;

  normals_[0] = glm::normalize(glm::cross(tangents_[0], kInitialBinormal));
  binormals_[0] = glm::normalize(glm::cross(tangents_[0], normals_[0]));

  for (uint i = 1; i < tangents->size(); ++i) {
    normals_[i] = glm::normalize(glm::cross(binormals_[i - 1], tangents_[i]));
    binormals_[i] = glm::normalize(glm::cross(tangents_[i], normals_[i]));
  }
}

void MakeRails(const CameraPathVertices *campath, const glm::vec4 *color,
               float head_w, float head_h, float web_w, float web_h,
               float gauge, float pos_offset_in_campath_norm_dir,
               std::vector<glm::vec3> *positions,
               std::vector<glm::vec4> *colors, std::vector<uint> *indices) {
  assert(campath);
  assert(color);
  assert(positions);
  assert(colors);
  assert(indices);

  assert(head_w > 0);
  assert(web_w > 0);
  assert(head_w > web_w);
  assert(gauge > 0);

  static constexpr uint kCrossSectionVertexCount = 8;

  auto &cv_pos = campath->positions;
  auto &cv_binorm = campath->binormals;
  auto &cv_norm = campath->normals;

  uint cv_count = Count(campath);
  uint rv_count = 2 * cv_count * kCrossSectionVertexCount;

  positions->resize(rv_count);
  colors->resize(rv_count);

  for (auto &c : *colors) {
    c = *color;
  }

  auto &pos = *positions;

  for (uint i = 0; i < 2; ++i) {
    for (uint j = 0; j < cv_count; ++j) {
      uint k = kCrossSectionVertexCount * (j + i * cv_count);
      // See the comment block above the function declaration in the header file
      // for the visual index-to-position mapping.
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
      pos[j + k] += 0.5f * gauge * cv_binorm[i];
    }
  }
  for (uint i = 0; i < cv_count; ++i) {
    uint j = kCrossSectionVertexCount * (i + cv_count);
    for (uint k = 0; k < kCrossSectionVertexCount; ++k) {
      pos[j + k] -= 0.5f * gauge * cv_binorm[i];
    }
  }

  for (uint i = 0; i < 2; ++i) {
    for (uint j = 0; j < cv_count; ++j) {
      uint k = kCrossSectionVertexCount * (j + i * cv_count);
      for (uint l = 0; l < kCrossSectionVertexCount; ++l) {
        pos[k + l] += pos_offset_in_campath_norm_dir * cv_norm[j];
      }
    }
  }

  auto &ri = *indices;

  for (uint i = 0; i < 2; ++i) {
    for (uint j = i * rv_count / 2;
         j + kCrossSectionVertexCount < rv_count / (2 - i);
         j += kCrossSectionVertexCount) {
      // Top face
      ri.push_back(j + 4);
      ri.push_back(j + 12);
      ri.push_back(j + 3);
      ri.push_back(j + 12);
      ri.push_back(j + 11);
      ri.push_back(j + 3);

      // Top right right face
      ri.push_back(j + 3);
      ri.push_back(j + 11);
      ri.push_back(j + 2);
      ri.push_back(j + 11);
      ri.push_back(j + 10);
      ri.push_back(j + 2);

      // Top right bottom face
      ri.push_back(j + 2);
      ri.push_back(j + 10);
      ri.push_back(j + 1);
      ri.push_back(j + 10);
      ri.push_back(j + 9);
      ri.push_back(j + 1);

      // Bottom right face
      ri.push_back(j + 1);
      ri.push_back(j + 9);
      ri.push_back(j);
      ri.push_back(j + 9);
      ri.push_back(j + 8);
      ri.push_back(j);

      // Bottom face
      ri.push_back(j);
      ri.push_back(j + 8);
      ri.push_back(j + 7);
      ri.push_back(j + 8);
      ri.push_back(j + 15);
      ri.push_back(j + 7);

      // Bottom left face
      ri.push_back(j + 14);
      ri.push_back(j + 6);
      ri.push_back(j + 15);
      ri.push_back(j + 6);
      ri.push_back(j + 7);
      ri.push_back(j + 15);

      // Top left bottom face
      ri.push_back(j + 13);
      ri.push_back(j + 5);
      ri.push_back(j + 14);
      ri.push_back(j + 5);
      ri.push_back(j + 6);
      ri.push_back(j + 14);

      // Top left left face
      ri.push_back(j + 12);
      ri.push_back(j + 4);
      ri.push_back(j + 13);
      ri.push_back(j + 4);
      ri.push_back(j + 5);
      ri.push_back(j + 13);
    }
  }
}

void MakeCrossties(const CameraPathVertices *campath, float separation_dist,
                   float pos_offset_in_campath_norm_dir, glm::vec3 **positions,
                   glm::vec2 **tex_coords, uint *count) {
  static constexpr float kAlpha = 0.1;
  static constexpr float kBeta = 1.5;
  static constexpr int kUniqPosCountPerCrosstie = 8;
  static constexpr float kBarDepth = 0.3;
  static constexpr float kTolerance = 0.00001;

  assert(campath);
  assert(positions);
  assert(tex_coords);
  assert(count);
  assert(separation_dist + kTolerance > 0);

  auto &path_pos = campath->positions;
  auto &path_tan = campath->tangents;
  auto &path_binorm = campath->binormals;
  auto &path_norm = campath->normals;
  uint path_count = Count(campath);

  uint max_vertex_count = 36 * (path_count - 1);
  glm::vec3 *pos = new glm::vec3[max_vertex_count];
  glm::vec2 *texc = new glm::vec2[max_vertex_count];

  glm::vec3 p[kUniqPosCountPerCrosstie];
  float dist_moved = 0;
  uint posi = 0;
  uint texci = 0;
  for (uint i = 1; i < path_count; ++i) {
    dist_moved += glm::length(path_pos[i] - path_pos[i - 1]);

    if (dist_moved < separation_dist + kTolerance) {
      continue;
    }

    p[0] = path_pos[i] +
           kAlpha * (-kBeta * path_norm[i] + path_binorm[i] * 0.5f) +
           path_binorm[i];
    p[1] = path_pos[i] + kAlpha * (-path_norm[i] + path_binorm[i] * 0.5f) +
           path_binorm[i];
    p[2] = path_pos[i] +
           kAlpha * (-kBeta * path_norm[i] + path_binorm[i] * 0.5f) -
           path_binorm[i] - kAlpha * path_binorm[i];
    p[3] = path_pos[i] + kAlpha * (-path_norm[i] + path_binorm[i] * 0.5f) -
           path_binorm[i] - kAlpha * path_binorm[i];

    p[4] = path_pos[i] +
           kAlpha * (-kBeta * path_norm[i] + path_binorm[i] * 0.5f) +
           path_binorm[i] + kBarDepth * path_tan[i];
    p[5] = path_pos[i] + kAlpha * (-path_norm[i] + path_binorm[i] * 0.5f) +
           path_binorm[i] + kBarDepth * path_tan[i];
    p[6] = path_pos[i] +
           kAlpha * (-kBeta * path_norm[i] + path_binorm[i] * 0.5f) -
           path_binorm[i] + kBarDepth * path_tan[i] - kAlpha * path_binorm[i];
    p[7] = path_pos[i] + kAlpha * (-path_norm[i] + path_binorm[i] * 0.5f) -
           path_binorm[i] + kBarDepth * path_tan[i] - kAlpha * path_binorm[i];

    for (uint j = 0; j < kUniqPosCountPerCrosstie; ++j) {
      p[j] += pos_offset_in_campath_norm_dir * path_norm[i];
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

  *count = posi;

  *positions = new glm::vec3[*count];
  for (uint i = 0; i < *count; ++i) {
    (*positions)[i] = pos[i];
  }
  delete[] pos;

  *tex_coords = new glm::vec2[*count];
  for (uint i = 0; i < *count; ++i) {
    (*tex_coords)[i] = texc[i];
  }
  delete[] texc;
}