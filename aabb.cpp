#include "aabb.hpp"

#include <limits>

void AabbCenter(const glm::vec3* min_pos, const glm::vec3* max_pos,
                glm::vec3* center) {
  *center = {(max_pos->x + min_pos->x), (max_pos->y + min_pos->y),
             (max_pos->z + min_pos->z)};
  *center *= 0.5f;
}

void AabbSize(const glm::vec3* min_pos, const glm::vec3* max_pos,
              glm::vec3* size) {
  *size = *max_pos - *min_pos;
}

void AabbMinMaxPositions(const glm::vec3* positions, uint position_count,
                         glm::vec3* min_pos, glm::vec3* max_pos) {
  *min_pos = glm::vec3(std::numeric_limits<glm::vec3::value_type>::max());
  *max_pos = glm::vec3(std::numeric_limits<glm::vec3::value_type>::min());

  for (uint i = 0; i < position_count; ++i) {
    auto& p = positions[i];
    if (p.x < min_pos->x) {
      min_pos->x = p.x;
    }
    if (p.x > max_pos->x) {
      max_pos->x = p.x;
    }

    if (p.y < min_pos->y) {
      min_pos->y = p.y;
    }
    if (p.y > max_pos->y) {
      max_pos->y = p.y;
    }

    if (p.z < min_pos->z) {
      min_pos->z = p.z;
    }
    if (p.z > max_pos->z) {
      max_pos->z = p.z;
    }
  }
}

void AabbCenterAndSize(const glm::vec3* positions, uint position_count,
                       glm::vec3* center, glm::vec3* size) {
  glm::vec3 min_pos;
  glm::vec3 max_pos;
  AabbMinMaxPositions(positions, position_count, &min_pos, &max_pos);
  AabbCenter(&min_pos, &max_pos, center);
  AabbSize(&min_pos, &max_pos, size);
}