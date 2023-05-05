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
                                  std::vector<glm::vec3> *positions,
                                  std::vector<glm::vec2> *tex_coords) {
  assert(side_len > 0);
  assert(tex_repeat_count == 1 || tex_repeat_count % 2 == 0);
  assert(positions);
  assert(tex_coords);

  *positions = {{-side_len, 0, side_len}, {-side_len, 0, -side_len},
                {side_len, 0, -side_len}, {-side_len, 0, side_len},
                {side_len, 0, -side_len}, {side_len, 0, side_len}};

  for (auto &p : *positions) {
    p *= 0.5f;
  }

  *tex_coords = {{0, 0},
                 {0, tex_repeat_count},
                 {tex_repeat_count, tex_repeat_count},
                 {0, 0},
                 {tex_repeat_count, tex_repeat_count},
                 {tex_repeat_count, 0}};
}

void MakeAxisAlignedBox(float side_len, uint tex_repeat_count,
                        std::vector<glm::vec3> *positions,
                        std::vector<glm::vec2> *tex_coords) {
  assert(side_len > 0);
  assert(positions);
  assert(tex_coords);
  assert(tex_repeat_count == 1 || tex_repeat_count % 2 == 0);

  *positions = {// x = -1 face
                {-side_len, -side_len, -side_len},
                {-side_len, side_len, -side_len},
                {-side_len, side_len, side_len},
                {-side_len, -side_len, -side_len},
                {-side_len, side_len, side_len},
                {-side_len, -side_len, side_len},

                // x = 1 face
                {side_len, -side_len, -side_len},
                {side_len, side_len, -side_len},
                {side_len, side_len, side_len},
                {side_len, -side_len, -side_len},
                {side_len, side_len, side_len},
                {side_len, -side_len, side_len},

                // y = -1 face
                {-side_len, -side_len, -side_len},
                {-side_len, -side_len, side_len},
                {side_len, -side_len, side_len},
                {-side_len, -side_len, -side_len},
                {side_len, -side_len, side_len},
                {side_len, -side_len, -side_len},

                // y = 1 face
                {-side_len, side_len, -side_len},
                {-side_len, side_len, side_len},
                {side_len, side_len, side_len},
                {-side_len, side_len, -side_len},
                {side_len, side_len, side_len},
                {side_len, side_len, -side_len},

                // z = -1 face
                {-side_len, -side_len, -side_len},
                {-side_len, side_len, -side_len},
                {side_len, side_len, -side_len},
                {-side_len, -side_len, -side_len},
                {side_len, side_len, -side_len},
                {side_len, -side_len, -side_len},

                // z = 1 face
                {-side_len, -side_len, side_len},
                {-side_len, side_len, side_len},
                {side_len, side_len, side_len},
                {-side_len, -side_len, side_len},
                {side_len, side_len, side_len},
                {side_len, -side_len, side_len}};

  for (auto &p : *positions) {
    p *= 0.5f;
  }

  *tex_coords = {{0, 0},
                 {0, tex_repeat_count},
                 {tex_repeat_count, tex_repeat_count},
                 {0, 0},
                 {tex_repeat_count, tex_repeat_count},
                 {tex_repeat_count, 0},
                 {0, 0},
                 {0, tex_repeat_count},
                 {tex_repeat_count, tex_repeat_count},
                 {0, 0},
                 {tex_repeat_count, tex_repeat_count},
                 {tex_repeat_count, 0},
                 {0, 0},
                 {0, tex_repeat_count},
                 {tex_repeat_count, tex_repeat_count},
                 {0, 0},
                 {tex_repeat_count, tex_repeat_count},
                 {tex_repeat_count, 0},
                 {0, 0},
                 {0, tex_repeat_count},
                 {tex_repeat_count, tex_repeat_count},
                 {0, 0},
                 {tex_repeat_count, tex_repeat_count},
                 {tex_repeat_count, 0},
                 {0, 0},
                 {0, tex_repeat_count},
                 {tex_repeat_count, tex_repeat_count},
                 {0, 0},
                 {tex_repeat_count, tex_repeat_count},
                 {tex_repeat_count, 0},
                 {0, 0},
                 {0, tex_repeat_count},
                 {tex_repeat_count, tex_repeat_count},
                 {0, 0},
                 {tex_repeat_count, tex_repeat_count},
                 {tex_repeat_count, 0}};
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
                   float pos_offset_in_campath_norm_dir,
                   std::vector<glm::vec3> *positions,
                   std::vector<glm::vec2> *tex_coords) {
  static constexpr float kAlpha = 0.1;
  static constexpr float kBeta = 1.5;
  static constexpr int kVertexCount = 8;
  static constexpr float kBarDepth = 0.3;
  static constexpr float kTolerance = 0.00001;

  assert(campath);
  assert(positions);
  assert(tex_coords);
  assert(separation_dist + kTolerance > 0);

  auto &p_pos = campath->positions;
  auto &p_tan = campath->tangents;
  auto &p_binorm = campath->binormals;
  auto &p_norm = campath->normals;
  uint p_count = Count(campath);

  auto &pos = *positions;
  auto &texc = *tex_coords;

  glm::vec3 v[kVertexCount];
  float dist_moved = 0;
  for (uint i = 1; i < p_count; ++i) {
    dist_moved += glm::length(p_pos[i] - p_pos[i - 1]);

    if (dist_moved < separation_dist + kTolerance) {
      continue;
    }

    v[0] = p_pos[i] + kAlpha * (-kBeta * p_norm[i] + p_binorm[i] * 0.5f) +
           p_binorm[i];
    v[1] = p_pos[i] + kAlpha * (-p_norm[i] + p_binorm[i] * 0.5f) + p_binorm[i];
    v[2] = p_pos[i] + kAlpha * (-kBeta * p_norm[i] + p_binorm[i] * 0.5f) -
           p_binorm[i] - kAlpha * p_binorm[i];
    v[3] = p_pos[i] + kAlpha * (-p_norm[i] + p_binorm[i] * 0.5f) - p_binorm[i] -
           kAlpha * p_binorm[i];

    v[4] = p_pos[i] + kAlpha * (-kBeta * p_norm[i] + p_binorm[i] * 0.5f) +
           p_binorm[i] + kBarDepth * p_tan[i];
    v[5] = p_pos[i] + kAlpha * (-p_norm[i] + p_binorm[i] * 0.5f) + p_binorm[i] +
           kBarDepth * p_tan[i];
    v[6] = p_pos[i] + kAlpha * (-kBeta * p_norm[i] + p_binorm[i] * 0.5f) -
           p_binorm[i] + kBarDepth * p_tan[i] - kAlpha * p_binorm[i];
    v[7] = p_pos[i] + kAlpha * (-p_norm[i] + p_binorm[i] * 0.5f) - p_binorm[i] +
           kBarDepth * p_tan[i] - kAlpha * p_binorm[i];

    for (uint j = 0; j < kVertexCount; ++j) {
      v[j] += pos_offset_in_campath_norm_dir * p_norm[i];
    }

    // Top face
    pos.push_back(v[6]);
    pos.push_back(v[5]);
    pos.push_back(v[2]);
    pos.push_back(v[5]);
    pos.push_back(v[1]);
    pos.push_back(v[2]);

    // Right face
    pos.push_back(v[5]);
    pos.push_back(v[4]);
    pos.push_back(v[1]);
    pos.push_back(v[4]);
    pos.push_back(v[0]);
    pos.push_back(v[1]);

    // Bottom face
    pos.push_back(v[4]);
    pos.push_back(v[7]);
    pos.push_back(v[0]);
    pos.push_back(v[7]);
    pos.push_back(v[3]);
    pos.push_back(v[0]);

    // Left face
    pos.push_back(v[7]);
    pos.push_back(v[6]);
    pos.push_back(v[3]);
    pos.push_back(v[6]);
    pos.push_back(v[2]);
    pos.push_back(v[3]);

    // Back face
    pos.push_back(v[5]);
    pos.push_back(v[6]);
    pos.push_back(v[4]);
    pos.push_back(v[6]);
    pos.push_back(v[7]);
    pos.push_back(v[4]);

    // Front face
    pos.push_back(v[2]);
    pos.push_back(v[1]);
    pos.push_back(v[3]);
    pos.push_back(v[1]);
    pos.push_back(v[0]);
    pos.push_back(v[3]);

    for (uint j = 0; j < 6; ++j) {
      texc.emplace_back(0, 1);
      texc.emplace_back(1, 1);
      texc.emplace_back(0, 0);
      texc.emplace_back(1, 1);
      texc.emplace_back(1, 0);
      texc.emplace_back(0, 0);
    }

    dist_moved = 0;
  }
}