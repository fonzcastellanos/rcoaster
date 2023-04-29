#ifndef ROLLER_COASTER_AABB_HPP
#define ROLLER_COASTER_AABB_HPP

#include <glm/glm.hpp>

#include "types.hpp"

union Aabb {
  struct {
    glm::vec3 min_position;
    glm::vec3 max_position;
  };
  struct {
    glm::vec3 center;
    glm::vec3 size;
  };
};

void MakeAabb(Aabb* a, const glm::vec3* positions, uint position_count);

glm::vec3 Center(const Aabb* a);
glm::vec3 Size(const Aabb* a);

#endif  // ROLLER_COASTER_AABB_HPP