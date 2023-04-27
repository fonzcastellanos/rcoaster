#ifndef ROLLER_COASTER_SPLINE_HPP
#define ROLLER_COASTER_SPLINE_HPP

#include <glm/glm.hpp>
#include <vector>

#include "types.hpp"

struct Point {
  float x;
  float y;
  float z;
};

struct SplineVertices {
  std::vector<glm::vec3> positions;
  std::vector<glm::vec3> tangents;
};

uint Count(const SplineVertices *s);

void EvalCatmullRomSpline(const std::vector<Point> *spline,
                          SplineVertices *vertices);

#endif  // ROLLER_COASTER_SPLINE_HPP