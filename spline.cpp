#include "spline.hpp"

#include <cassert>

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

uint Count(const SplineVertices *s) {
  assert(s);
  assert(s->positions.size() == s->tangents.size());

  return s->positions.size();
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

void EvalCatmullRomSpline(const std::vector<Point> *spline,
                          SplineVertices *vertices) {
  auto &spline_ = *spline;
  static constexpr float kMaxLineLen = 0.5;

  for (int i = 1; i < spline->size() - 2; ++i) {
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