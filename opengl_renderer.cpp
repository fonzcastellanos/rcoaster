#include "opengl_renderer.hpp"

#include <cassert>
#include <cstddef>
#include <cstdio>

#include "stb_image.h"

#define BUFFER_OFFSET(offset) ((GLvoid*)(offset))

enum RgbaChannel {
  kRgbaChannel_Red,
  kRgbaChannel_Green,
  kRgbaChannel_Blue,
  kRgbaChannel_Alpha,
  kRgbaChannel__Count
};

void Setup(OpenGlRenderer* r) {
  assert(r);

  r->texture_names = 0;
  r->texture_name_count = 0;

  glClearColor(0, 0, 0, 0);
  glEnable(GL_DEPTH_TEST);

  glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &r->max_anisotropy_degree);
}

Status SetupTextures(OpenGlRenderer* renderer, const char* const* filepaths,
                     uint filepath_count, float anisotropy_degree) {
  assert(renderer);
  assert(filepaths);

  assert(renderer->texture_name_count == 0);
  assert(!renderer->texture_names);
  assert(anisotropy_degree >= 1);
  assert(anisotropy_degree <= renderer->max_anisotropy_degree);

  renderer->texture_name_count = filepath_count;
  renderer->texture_names = new GLuint[filepath_count];

  glGenTextures(filepath_count, renderer->texture_names);

  for (uint i = 0; i < filepath_count; ++i) {
    int w;
    int h;
    int channel_count;
    uchar* data =
        stbi_load(filepaths[i], &w, &h, &channel_count, kRgbaChannel__Count);
    if (!data) {
      std::fprintf(stderr, "Failed to load image file %s.\n", filepaths[i]);

      renderer->texture_name_count = 0;
      delete[] renderer->texture_names;

      return kStatus_IoError;
    }

    glBindTexture(GL_TEXTURE_2D, renderer->texture_names[i]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 data);
    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                    GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT,
                    anisotropy_degree);

    stbi_image_free(data);
  }

  return kStatus_Ok;
}

static void SubmitVertices(const Mesh1P1UV* meshes, uint mesh_count,
                           uint vertex_count, GLuint vertices_vbo_name) {
  assert(meshes);

  glBindBuffer(GL_ARRAY_BUFFER, vertices_vbo_name);

  GLsizeiptr buf_size = vertex_count * sizeof(Vertex1P1UV);
  glBufferData(GL_ARRAY_BUFFER, buf_size, 0, GL_STATIC_DRAW);

  GLintptr offset = 0;
  for (uint i = 0; i < mesh_count; ++i) {
    GLsizeiptr size = meshes[i].vertex_count * sizeof(Vertex1P1UV);
    glBufferSubData(GL_ARRAY_BUFFER, offset, size, meshes[i].vertices);
    offset += size;
  }

  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

static void SubmitIndices(const Mesh1P1UV* meshes, uint mesh_count,
                          GLuint indices_vbo_name) {
  uint index_count = 0;
  for (uint i = 0; i < mesh_count; ++i) {
    index_count += meshes[i].index_count;
  }

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indices_vbo_name);

  uint buf_size = index_count * sizeof(uint);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, buf_size, 0, GL_STATIC_DRAW);

  uint offset = 0;
  for (uint i = 0; i < mesh_count; ++i) {
    uint size = meshes[i].index_count * sizeof(uint);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, offset, size, meshes[i].indices);
    offset += size;
  }

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

static void SetupVao1P1UV(GLuint vao_name, GLuint vertices_vbo_name,
                          GLuint indices_vbo_name, GLuint position_attrib_loc,
                          GLuint tex_coord_attrib_loc) {
  glBindVertexArray(vao_name);

  glBindBuffer(GL_ARRAY_BUFFER, vertices_vbo_name);

  glVertexAttribPointer(position_attrib_loc, 3, GL_FLOAT, GL_FALSE,
                        sizeof(Vertex1P1UV), BUFFER_OFFSET(0));
  glVertexAttribPointer(tex_coord_attrib_loc, 2, GL_FLOAT, GL_FALSE,
                        sizeof(Vertex1P1UV),
                        BUFFER_OFFSET(offsetof(Vertex1P1UV, uv)));

  glEnableVertexAttribArray(position_attrib_loc);
  glEnableVertexAttribArray(tex_coord_attrib_loc);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indices_vbo_name);

  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void SubmitMeshes(const Mesh1P1UV* meshes, uint mesh_count,
                  GLuint vertices_vbo_name, GLuint indices_vbo_name,
                  GLuint vao_name, GLuint position_attrib_loc,
                  GLuint tex_coord_attrib_loc) {
  assert(meshes);

  uint vert_count = 0;
  for (uint i = 0; i < mesh_count; ++i) {
    vert_count += meshes[i].vertex_count;
  }

  SubmitVertices(meshes, mesh_count, vert_count, vertices_vbo_name);
  SubmitIndices(meshes, mesh_count, indices_vbo_name);
  SetupVao1P1UV(vao_name, vertices_vbo_name, indices_vbo_name,
                position_attrib_loc, tex_coord_attrib_loc);
}

static void SubmitVertices(const Mesh1P1C* meshes, uint mesh_count,
                           uint vertex_count, GLuint vertices_vbo_name) {
  assert(meshes);

  glBindBuffer(GL_ARRAY_BUFFER, vertices_vbo_name);

  GLsizeiptr buf_size = vertex_count * sizeof(Vertex1P1C);
  glBufferData(GL_ARRAY_BUFFER, buf_size, 0, GL_STATIC_DRAW);

  GLintptr offset = 0;
  for (uint i = 0; i < mesh_count; ++i) {
    GLsizeiptr size = meshes[i].vertex_count * sizeof(Vertex1P1C);
    glBufferSubData(GL_ARRAY_BUFFER, offset, size, meshes[i].vertices);
    offset += size;
  }

  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

static void SubmitIndices(const Mesh1P1C* meshes, uint mesh_count,
                          GLuint indices_vbo_name) {
  uint index_count = 0;
  for (uint i = 0; i < mesh_count; ++i) {
    index_count += meshes[i].index_count;
  }

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indices_vbo_name);

  uint buf_size = index_count * sizeof(uint);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, buf_size, 0, GL_STATIC_DRAW);

  uint offset = 0;
  for (uint i = 0; i < mesh_count; ++i) {
    uint size = meshes[i].index_count * sizeof(uint);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, offset, size, meshes[i].indices);
    offset += size;
  }

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

static void SetupVao1P1C(GLuint vao_name, GLuint vertices_vbo_name,
                         GLuint indices_vbo_name, GLuint position_attrib_loc,
                         GLuint color_attrib_loc) {
  glBindVertexArray(vao_name);

  glBindBuffer(GL_ARRAY_BUFFER, vertices_vbo_name);

  glVertexAttribPointer(position_attrib_loc, 3, GL_FLOAT, GL_FALSE,
                        sizeof(Vertex1P1C), BUFFER_OFFSET(0));
  glVertexAttribPointer(color_attrib_loc, 4, GL_FLOAT, GL_FALSE,
                        sizeof(Vertex1P1C),
                        BUFFER_OFFSET(offsetof(Vertex1P1C, color)));

  glEnableVertexAttribArray(position_attrib_loc);
  glEnableVertexAttribArray(color_attrib_loc);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indices_vbo_name);

  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void SubmitMeshes(const Mesh1P1C* meshes, uint mesh_count,
                  GLuint vertices_vbo_name, GLuint indices_vbo_name,
                  GLuint vao_name, GLuint position_attrib_loc,
                  GLuint color_attrib_loc) {
  assert(meshes);

  uint vert_count = 0;
  for (uint i = 0; i < mesh_count; ++i) {
    vert_count += meshes[i].vertex_count;
  }

  SubmitVertices(meshes, mesh_count, vert_count, vertices_vbo_name);
  SubmitIndices(meshes, mesh_count, indices_vbo_name);
  SetupVao1P1C(vao_name, vertices_vbo_name, indices_vbo_name,
               position_attrib_loc, color_attrib_loc);
}