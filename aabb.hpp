#ifndef RCOASTER_AABB_HPP
#define RCOASTER_AABB_HPP

#include <glm/glm.hpp>

#include "types.hpp"

void AabbCenter(const glm::vec3* min_pos, const glm::vec3* max_pos,
                glm::vec3* center);

void AabbSize(const glm::vec3* min_pos, const glm::vec3* max_pos,
              glm::vec3* size);

void AabbMinMaxPositions(const glm::vec3* positions, uint position_count,
                         glm::vec3* min_pos, glm::vec3* max_pos);

void AabbCenterAndSize(const glm::vec3* positions, uint position_count,
                       glm::vec3* center, glm::vec3* size);

#endif  // RCOASTER_AABB_HPP