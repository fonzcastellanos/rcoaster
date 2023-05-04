#ifndef RCOASTER_MODELS_HPP
#define RCOASTER_MODELS_HPP

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <vector>

#include "opengl.hpp"
#include "types.hpp"

struct Vertex {
  glm::vec3 position;
  glm::vec4 color;
};

struct SplineVertices {
  std::vector<glm::vec3> positions;
  std::vector<glm::vec3> tangents;
};

struct TexturedVertices {
  std::vector<glm::vec3> positions;
  std::vector<glm::vec2> tex_coords;
};

struct CameraPathVertices {
  std::vector<glm::vec3> positions;
  std::vector<glm::vec3> tangents;
  std::vector<glm::vec3> normals;
  std::vector<glm::vec3> binormals;
};

uint Count(const SplineVertices *s);
uint Count(const TexturedVertices *t);
uint Count(const CameraPathVertices *c);

void EvalCatmullRomSpline(const std::vector<glm::vec3> *control_points,
                          SplineVertices *vertices);

void AxisAlignedXzSquarePlane(float side_len, uint tex_repeat_count,
                              TexturedVertices *tvs);

void AxisAlignedBox(float side_len, uint tex_repeat_count,
                    TexturedVertices *tvs);

void MakeCameraPath(const SplineVertices *spline, CameraPathVertices *path);

void MakeRails(const CameraPathVertices *campath, const glm::vec4 *color,
               float head_w, float head_h, float web_w, float web_h,
               float gauge, float pos_offset_in_campath_norm_dir,
               std::vector<Vertex> *vertices, std::vector<GLuint> *indices);

void MakeCrossties(const CameraPathVertices *campath, float separation_dist,
                   float pos_offset_in_campath_norm_dir,
                   TexturedVertices *crosstie);

#endif  // RCOASTER_MODELS_HPP