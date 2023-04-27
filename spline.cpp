#include "spline.hpp"

constexpr float kTension = 0.5;

const glm::mat4x4 kCatmullRomBasis(-kTension, 2 - kTension, kTension - 2,
                                   kTension, 2 * kTension, kTension - 3,
                                   3 - 2 * kTension, -kTension, -kTension, 0,
                                   kTension, 0, 0, 1, 0, 0);

glm::vec3 CatmullRomPosition(float u, const glm::mat4x3 *control) {
  glm::vec4 parameters(u * u * u, u * u, u, 1);
  return (*control) * kCatmullRomBasis * parameters;
}

glm::vec3 CatmullRomTangent(float u, const glm::mat4x3 *control) {
  glm::vec4 parameters(3 * u * u, 2 * u, 1, 0);
  return (*control) * kCatmullRomBasis * parameters;
}