#ifndef RCOASTER_OPENGL_RENDERER_HPP
#define RCOASTER_OPENGL_RENDERER_HPP

#include "meshes.hpp"
#include "opengl.hpp"
#include "status.hpp"
#include "types.hpp"

struct OpenGlRenderer {
  GLuint *texture_names;
  uint texture_name_count;
  GLfloat max_anisotropy_degree;
};

void Setup(OpenGlRenderer *r);

Status SetupTextures(OpenGlRenderer *renderer, const char *const *filepaths,
                     uint filepath_count, float anisotropy_degree);

void SubmitMeshes(const Mesh1P1UV *meshes, uint mesh_count,
                  GLuint vertices_vbo_name, GLuint indices_vbo_name,
                  GLuint vao_name, GLuint position_attrib_loc,
                  GLuint tex_coord_attrib_loc);

void SubmitMeshes(const Mesh1P1C *meshes, uint mesh_count,
                  GLuint vertices_vbo_name, GLuint indices_vbo_name,
                  GLuint vao_name, GLuint position_attrib_loc,
                  GLuint color_attrib_loc);

#endif  // RCOASTER_OPENGL_RENDERER_HPP