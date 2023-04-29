#include "aabb.hpp"

#include <limits>

void MakeAabb(Aabb* a, const glm::vec3* positions, uint position_count) {
  a->min_position = {std::numeric_limits<float>::max(),
                     std::numeric_limits<float>::max(),
                     std::numeric_limits<float>::max()};

  a->max_position = {std::numeric_limits<float>::min(),
                     std::numeric_limits<float>::min(),
                     std::numeric_limits<float>::min()};

  for (uint i = 0; i < position_count; ++i) {
    auto& p = positions[i];
    if (p.x < a->min_position.x) {
      a->min_position.x = p.x;
    }
    if (p.x > a->max_position.x) {
      a->max_position.x = p.x;
    }

    if (p.y < a->min_position.y) {
      a->min_position.y = p.y;
    }
    if (p.y > a->max_position.y) {
      a->max_position.y = p.y;
    }

    if (p.z < a->min_position.z) {
      a->min_position.z = p.z;
    }
    if (p.z > a->max_position.z) {
      a->max_position.z = p.z;
    }
  }
}

glm::vec3 Center(const Aabb* a) {
  glm::vec3 res = {(a->max_position.x + a->min_position.x) * 0.5f,
                   (a->max_position.y + a->min_position.y) * 0.5f,
                   (a->max_position.z + a->min_position.z) * 0.5f};
  return res;
}

glm::vec3 Size(const Aabb* a) {
  glm::vec3 res = a->max_position - a->min_position;
  return res;
}