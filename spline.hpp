#ifndef ROLLER_COASTER_SPLINE_HPP
#define ROLLER_COASTER_SPLINE_HPP

#include <glm/glm.hpp>

glm::vec3 CatmullRomPosition(float u, const glm::mat4x3 *control);

glm::vec3 CatmullRomTangent(float u, const glm::mat4x3 *control);

#endif  // ROLLER_COASTER_SPLINE_HPP