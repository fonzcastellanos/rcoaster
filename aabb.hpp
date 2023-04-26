#ifndef ROLLER_COASTER_AABB_HPP
#define ROLLER_COASTER_AABB_HPP

#include <glm/glm.hpp>

struct AABB {
  glm::vec3 min_point;
  glm::vec3 max_point;
};

void Init(AABB* a);

void Update(AABB* a, const glm::vec3* point);

glm::vec3 Center(AABB* a);

glm::vec3 Size(AABB* a);

#endif  // ROLLER_COASTER_AABB_HPP