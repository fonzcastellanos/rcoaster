#include "models.hpp"

#include <cassert>
#include <glm/glm.hpp>

uint Count(const SplineVertices *v) {
  assert(v);
  assert(v->positions.size() == v->tangents.size());

  return v->positions.size();
}

uint Count(const TexturedVertices *v) {
  assert(v);
  assert(v->positions.size() == v->tex_coords.size());

  return v->positions.size();
}

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
  glm::vec4 parameters(u * u * u, u * u, u, 1);
  return (*control) * kCatmullRomBasis * parameters;
}

static glm::vec3 CatmullRomSplineTangent(float u, const glm::mat4x3 *control) {
  glm::vec4 parameters(3 * u * u, 2 * u, 1, 0);
  return (*control) * kCatmullRomBasis * parameters;
}

static void Subdivide(float u0, float u1, float max_line_len,
                      const glm::mat4x3 *control, SplineVertices *vertices) {
  glm::vec3 p0 = CatmullRomSplinePosition(u0, control);
  glm::vec3 p1 = CatmullRomSplinePosition(u1, control);

  if (glm::length(p1 - p0) > max_line_len) {
    float umid = (u0 + u1) * 0.5f;

    Subdivide(u0, umid, max_line_len, control, vertices);
    Subdivide(umid, u1, max_line_len, control, vertices);
  } else {
    vertices->positions.push_back(p0);
    vertices->positions.push_back(p1);

    glm::vec3 t0 = CatmullRomSplineTangent(u0, control);
    glm::vec3 t1 = CatmullRomSplineTangent(u1, control);

    glm::vec3 t0_norm = glm::normalize(t0);
    glm::vec3 t1_norm = glm::normalize(t1);

    vertices->tangents.push_back(t0_norm);
    vertices->tangents.push_back(t1_norm);
  }
}

void EvalCatmullRomSpline(const std::vector<glm::vec3> *spline,
                          SplineVertices *vertices) {
  auto &spline_ = *spline;
  static constexpr float kMaxLineLen = 0.5;

  for (uint i = 1; i + 2 < spline->size(); ++i) {
    // clang-format off
    glm::mat4x3 control(
      spline_[i - 1].x, spline_[i - 1].y, spline_[i - 1].z,
      spline_[i].x, spline_[i].y, spline_[i].z,
      spline_[i + 1].x, spline_[i + 1].y, spline_[i + 1].z,
      spline_[i + 2].x, spline_[i + 2].y, spline_[i + 2].z
    );
    // clang-format on
    Subdivide(0, 1, kMaxLineLen, &control, vertices);
  }
}

void AxisAlignedXzSquarePlane(float side_len, uint tex_repeat_count,
                              TexturedVertices *tvs) {
  assert(side_len > 0);
  assert(tex_repeat_count == 1 || tex_repeat_count % 2 == 0);
  assert(tvs);

  tvs->positions = {{-side_len, 0, side_len}, {-side_len, 0, -side_len},
                    {side_len, 0, -side_len}, {-side_len, 0, side_len},
                    {side_len, 0, -side_len}, {side_len, 0, side_len}};
  for (uint i = 0; i < tvs->positions.size(); ++i) {
    tvs->positions[i] *= 0.5f;
  }

  tvs->tex_coords = {{0, 0},
                     {0, tex_repeat_count},
                     {tex_repeat_count, tex_repeat_count},
                     {0, 0},
                     {tex_repeat_count, tex_repeat_count},
                     {tex_repeat_count, 0}};
}

void AxisAlignedBox(float side_len, uint tex_repeat_count,
                    TexturedVertices *tvs) {
  assert(side_len > 0);
  assert(tex_repeat_count == 1 || tex_repeat_count % 2 == 0);
  assert(tvs);

  tvs->positions = {// x = -1 face
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

  for (uint i = 0; i < tvs->positions.size(); ++i) {
    tvs->positions[i] *= 0.5f;
  }

  tvs->tex_coords = {{0, 0},
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

void MakeCameraPath(const SplineVertices *spline, CameraPathVertices *path) {
  // Initial binormal chosen arbitrarily.
  static const glm::vec3 kInitialBinormal = {0, 1, -0.5};

  assert(spline);
  assert(path);

  uint count = Count(spline);

  assert(count > 0);

  path->positions = spline->positions;
  path->tangents = spline->tangents;

  path->normals.resize(count);
  path->binormals.resize(count);

  path->normals[0] =
      glm::normalize(glm::cross(spline->tangents[0], kInitialBinormal));
  path->binormals[0] =
      glm::normalize(glm::cross(spline->tangents[0], path->normals[0]));

  for (uint i = 1; i < count; ++i) {
    path->normals[i] =
        glm::normalize(glm::cross(path->binormals[i - 1], spline->tangents[i]));
    path->binormals[i] =
        glm::normalize(glm::cross(spline->tangents[i], path->normals[i]));
  }
}

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
               std::vector<Vertex> *vertices, std::vector<GLuint> *indices) {
  assert(campath);
  assert(color);
  assert(vertices);
  assert(indices);

  assert(head_w > 0);
  assert(web_w > 0);
  assert(head_w > web_w);
  assert(gauge > 0);

  static constexpr uint kCrossSectionVertexCount = 8;

  auto &cv_pos = campath->positions;
  auto &cv_binorm = campath->binormals;
  auto &cv_norm = campath->normals;

  auto &rv = *vertices;

  uint cv_count = Count(campath);

  uint rv_count = 2 * cv_count * kCrossSectionVertexCount;
  rv.resize(rv_count);

  for (uint i = 0; i < rv_count; ++i) {
    rv[i].color = *color;
  }

  for (uint i = 0; i < 2; ++i) {
    for (uint j = 0; j < cv_count; ++j) {
      uint k = kCrossSectionVertexCount * (j + i * cv_count);
      // See the comment block immediately above this function definition for
      // the visual index-to-position mapping
      rv[k].position =
          cv_pos[j] - web_h * cv_norm[j] + 0.5f * web_w * cv_binorm[j];
      rv[k + 1].position = cv_pos[j] + 0.5f * web_w * cv_binorm[j];
      rv[k + 2].position = cv_pos[j] + 0.5f * head_w * cv_binorm[j];
      rv[k + 3].position =
          cv_pos[j] + head_h * cv_norm[j] + 0.5f * head_w * cv_binorm[j];
      rv[k + 4].position = rv[k + 4].position =
          cv_pos[j] + head_h * cv_norm[j] - 0.5f * head_w * cv_binorm[j];
      rv[k + 5].position = cv_pos[j] - 0.5f * head_w * cv_binorm[j];
      rv[k + 6].position = cv_pos[j] - 0.5f * web_w * cv_binorm[j];
      rv[k + 7].position =
          cv_pos[j] - web_h * cv_norm[j] - 0.5f * web_w * cv_binorm[j];
    }
  }

  // Set rail pair `gauge` distance apart
  for (uint i = 0; i < cv_count; ++i) {
    uint j = kCrossSectionVertexCount * i;
    for (uint k = 0; k < kCrossSectionVertexCount; ++k) {
      rv[j + k].position += 0.5f * gauge * cv_binorm[i];
    }
  }
  for (uint i = 0; i < cv_count; ++i) {
    uint j = kCrossSectionVertexCount * (i + cv_count);
    for (uint k = 0; k < kCrossSectionVertexCount; ++k) {
      rv[j + k].position -= 0.5f * gauge * cv_binorm[i];
    }
  }

  for (uint i = 0; i < 2; ++i) {
    for (uint j = 0; j < cv_count; ++j) {
      uint k = kCrossSectionVertexCount * (j + i * cv_count);
      for (uint l = 0; l < kCrossSectionVertexCount; ++l) {
        rv[k + l].position += pos_offset_in_campath_norm_dir * cv_norm[j];
      }
    }
  }

  auto &ri = *indices;

  for (uint i = 0; i < 2; ++i) {
    for (uint j = i * rv_count / 2;
         j + kCrossSectionVertexCount < rv_count / (2 - i);
         j += kCrossSectionVertexCount) {
      // top face
      ri.push_back(j + 4);
      ri.push_back(j + 12);
      ri.push_back(j + 3);
      ri.push_back(j + 12);
      ri.push_back(j + 11);
      ri.push_back(j + 3);

      // top right right face
      ri.push_back(j + 3);
      ri.push_back(j + 11);
      ri.push_back(j + 2);
      ri.push_back(j + 11);
      ri.push_back(j + 10);
      ri.push_back(j + 2);

      // top right bottom face
      ri.push_back(j + 2);
      ri.push_back(j + 10);
      ri.push_back(j + 1);
      ri.push_back(j + 10);
      ri.push_back(j + 9);
      ri.push_back(j + 1);

      // bottom right face
      ri.push_back(j + 1);
      ri.push_back(j + 9);
      ri.push_back(j);
      ri.push_back(j + 9);
      ri.push_back(j + 8);
      ri.push_back(j);

      // bottom face
      ri.push_back(j);
      ri.push_back(j + 8);
      ri.push_back(j + 7);
      ri.push_back(j + 8);
      ri.push_back(j + 15);
      ri.push_back(j + 7);

      // bottom left face
      ri.push_back(j + 14);
      ri.push_back(j + 6);
      ri.push_back(j + 15);
      ri.push_back(j + 6);
      ri.push_back(j + 7);
      ri.push_back(j + 15);

      // top left bottom face
      ri.push_back(j + 13);
      ri.push_back(j + 5);
      ri.push_back(j + 14);
      ri.push_back(j + 5);
      ri.push_back(j + 6);
      ri.push_back(j + 14);

      // top left left face
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
                   TexturedVertices *crosstie) {
  static constexpr float kAlpha = 0.1;
  static constexpr float kBeta = 1.5;
  static constexpr int kVertexCount = 8;
  static constexpr float kBarDepth = 0.3;
  static constexpr float kTolerance = 0.00001;

  auto &p_pos = campath->positions;
  auto &p_tan = campath->tangents;
  auto &p_binorm = campath->binormals;
  auto &p_norm = campath->normals;
  uint p_count = Count(campath);

  auto &b_pos = crosstie->positions;
  auto &b_tex_coords = crosstie->tex_coords;

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

    // top face
    b_pos.push_back(v[6]);
    b_pos.push_back(v[5]);
    b_pos.push_back(v[2]);
    b_pos.push_back(v[5]);
    b_pos.push_back(v[1]);
    b_pos.push_back(v[2]);

    // right face
    b_pos.push_back(v[5]);
    b_pos.push_back(v[4]);
    b_pos.push_back(v[1]);
    b_pos.push_back(v[4]);
    b_pos.push_back(v[0]);
    b_pos.push_back(v[1]);

    // bottom face
    b_pos.push_back(v[4]);
    b_pos.push_back(v[7]);
    b_pos.push_back(v[0]);
    b_pos.push_back(v[7]);
    b_pos.push_back(v[3]);
    b_pos.push_back(v[0]);

    // left face
    b_pos.push_back(v[7]);
    b_pos.push_back(v[6]);
    b_pos.push_back(v[3]);
    b_pos.push_back(v[6]);
    b_pos.push_back(v[2]);
    b_pos.push_back(v[3]);

    // back face
    b_pos.push_back(v[5]);
    b_pos.push_back(v[6]);
    b_pos.push_back(v[4]);
    b_pos.push_back(v[6]);
    b_pos.push_back(v[7]);
    b_pos.push_back(v[4]);

    // front face
    b_pos.push_back(v[2]);
    b_pos.push_back(v[1]);
    b_pos.push_back(v[3]);
    b_pos.push_back(v[1]);
    b_pos.push_back(v[0]);
    b_pos.push_back(v[3]);

    for (uint j = 0; j < 6; ++j) {
      b_tex_coords.emplace_back(0, 1);
      b_tex_coords.emplace_back(1, 1);
      b_tex_coords.emplace_back(0, 0);
      b_tex_coords.emplace_back(1, 1);
      b_tex_coords.emplace_back(1, 0);
      b_tex_coords.emplace_back(0, 0);
    }

    dist_moved = 0;
  }
}