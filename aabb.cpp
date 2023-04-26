#include "aabb.hpp"

#include <limits>

void Init(AABB* a) {
  a->min_point = {std::numeric_limits<float>::max(),
                  std::numeric_limits<float>::max(),
                  std::numeric_limits<float>::max()};
  a->max_point = {std::numeric_limits<float>::min(),
                  std::numeric_limits<float>::min(),
                  std::numeric_limits<float>::min()};
}

void Update(AABB* a, const glm::vec3* point) {
  if (point->x < a->min_point.x) {
    a->min_point.x = point->x;
  }
  if (point->x > a->max_point.x) {
    a->max_point.x = point->x;
  }

  if (point->y < a->min_point.y) {
    a->min_point.y = point->y;
  }
  if (point->y > a->max_point.y) {
    a->max_point.y = point->y;
  }

  if (point->z < a->min_point.z) {
    a->min_point.z = point->z;
  }
  if (point->z > a->max_point.z) {
    a->max_point.z = point->z;
  }
}

glm::vec3 Center(AABB* a) {
  glm::vec3 res = {(a->max_point.x + a->min_point.x) / 2,
                   (a->max_point.y + a->min_point.y) / 2,
                   (a->max_point.z + a->min_point.z) / 2};
  return res;
}

glm::vec3 Size(AABB* a) {
  glm::vec3 res = a->max_point - a->min_point;
  return res;
}