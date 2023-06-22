#include "opengl_renderer.hpp"

#include <cassert>
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

void SubmitVertexLists(const VertexList1P1UV* vert_lists, uint vert_list_count,
                       GLuint vbo_name, GLuint vao_name,
                       GLuint position_attrib_loc,
                       GLuint tex_coord_attrib_loc) {
  assert(vert_lists);

  uint vert_count = 0;
  for (uint i = 0; i < vert_list_count; ++i) {
    vert_count += vert_lists[i].count;
  }

  // Upload vertex attributes to VBO.

  glBindBuffer(GL_ARRAY_BUFFER, vbo_name);

  {
    GLsizeiptr size = vert_count * (sizeof(glm::vec3) + sizeof(glm::vec2));
    glBufferData(GL_ARRAY_BUFFER, size, NULL, GL_STATIC_DRAW);
  }

  {
    GLintptr offset = 0;

    for (uint i = 0; i < vert_list_count; ++i) {
      GLsizeiptr size = vert_lists[i].count * sizeof(glm::vec3);
      glBufferSubData(GL_ARRAY_BUFFER, offset, size, vert_lists[i].positions);
      offset += size;
    }

    for (uint i = 0; i < vert_list_count; ++i) {
      GLsizeiptr size = vert_lists[i].count * sizeof(glm::vec2);
      glBufferSubData(GL_ARRAY_BUFFER, offset, size, vert_lists[i].uv);
      offset += size;
    }
  }

  glBindBuffer(GL_ARRAY_BUFFER, 0);

  // Setup VAO.

  glBindVertexArray(vao_name);

  glBindBuffer(GL_ARRAY_BUFFER, vbo_name);

  glVertexAttribPointer(position_attrib_loc, 3, GL_FLOAT, GL_FALSE,
                        sizeof(glm::vec3), BUFFER_OFFSET(0));
  glVertexAttribPointer(tex_coord_attrib_loc, 2, GL_FLOAT, GL_FALSE,
                        sizeof(glm::vec2),
                        BUFFER_OFFSET(vert_count * sizeof(glm::vec3)));

  glEnableVertexAttribArray(position_attrib_loc);
  glEnableVertexAttribArray(tex_coord_attrib_loc);

  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void SubmitVertexLists(const VertexList1P1C* vert_lists, uint vert_list_count,
                       GLuint vbo_name, GLuint vao_name,
                       GLuint position_attrib_loc, GLuint color_attrib_loc) {
  assert(vert_lists);

  uint vert_count = 0;
  for (uint i = 0; i < vert_list_count; ++i) {
    vert_count += vert_lists[i].count;
  }

  // Upload vertex attributes to VBO.

  glBindBuffer(GL_ARRAY_BUFFER, vbo_name);

  {
    GLsizeiptr size = vert_count * (sizeof(glm::vec3) + sizeof(glm::vec4));
    glBufferData(GL_ARRAY_BUFFER, size, 0, GL_STATIC_DRAW);
  }

  GLintptr offset = 0;

  for (uint i = 0; i < vert_list_count; ++i) {
    GLsizeiptr size = vert_lists[i].count * sizeof(glm::vec3);
    glBufferSubData(GL_ARRAY_BUFFER, offset, size, vert_lists[i].positions);
    offset += size;
  }

  for (uint i = 0; i < vert_list_count; ++i) {
    GLsizeiptr size = vert_lists[i].count * sizeof(glm::vec4);
    glBufferSubData(GL_ARRAY_BUFFER, offset, size, vert_lists[i].colors);
    offset += size;
  }

  glBindBuffer(GL_ARRAY_BUFFER, 0);

  // Setup VAO.

  glBindVertexArray(vao_name);

  glBindBuffer(GL_ARRAY_BUFFER, vbo_name);

  glVertexAttribPointer(position_attrib_loc, 3, GL_FLOAT, GL_FALSE,
                        sizeof(glm::vec3), BUFFER_OFFSET(0));
  glVertexAttribPointer(color_attrib_loc, 4, GL_FLOAT, GL_FALSE,
                        sizeof(glm::vec4),
                        BUFFER_OFFSET(vert_count * sizeof(glm::vec3)));

  glEnableVertexAttribArray(position_attrib_loc);
  glEnableVertexAttribArray(color_attrib_loc);

  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}