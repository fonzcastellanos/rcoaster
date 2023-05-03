#include "shader.hpp"

#include <sys/stat.h>

#include <cassert>
#include <cstdio>
#include <cstring>

#define INFO_LOG_BUFFER_SIZE 512

const char *const kShaderTypeStrings[kShaderType__Count] = {"vertex",
                                                            "fragment"};

static int HasPostfix(const char *str, const char *postfix) {
  std::size_t str_len = std::strlen(str);
  std::size_t postfix_len = std::strlen(postfix);

  if (str_len < postfix_len) {
    return 0;
  }

  const char *str_end = str + str_len - postfix_len;

  int res = std::strcmp(str_end, postfix) == 0;
  return res;
}

GLenum GlShaderType(ShaderType t) {
  switch (t) {
    case kShaderType_Vertex: {
      return GL_VERTEX_SHADER;
    }
    case kShaderType_Fragment: {
      return GL_FRAGMENT_SHADER;
    }
    default: {
      assert(false);
    }
  }
}

const char *String(ShaderType t) {
  assert(t < kShaderType__Count);
  return kShaderTypeStrings[t];
}

ShaderType FromFilepath(const char *path) {
  int rc = HasPostfix(path, "vert.glsl");
  if (rc) {
    return kShaderType_Vertex;
  }
  rc = HasPostfix(path, "frag.glsl");
  if (rc) {
    return kShaderType_Fragment;
  }
  assert(false);
}

Status LoadFile(const char *path, std::string *content) {
  assert(path);
  assert(content);

  std::FILE *file = std::fopen(path, "rb");
  if (!file) {
    std::fprintf(stderr, "Failed to open file %s.\n", path);
    return kStatus_IoError;
  }

  struct stat stat_buf;
  {
    int rc = stat(path, &stat_buf);
    if (rc) {
      std::fprintf(stderr, "Failed to get status of file %s.\n", path);
      std::fclose(file);
      return kStatus_IoError;
    }
  }

  content->resize(stat_buf.st_size);

  std::size_t rc =
      std::fread(content->data(), sizeof((*content)[0]), content->size(), file);
  if (rc < content->size()) {
    std::fprintf(stderr, "Failed to read file %s.\n", path);
    std::fclose(file);
    return kStatus_IoError;
  }

  std::fclose(file);

  return kStatus_Ok;
}

Status MakeShader(const std::string *content, ShaderType type, GLuint *name) {
  assert(content);
  assert(name);

  *name = glCreateShader(GlShaderType(type));
  if (*name == 0) {
    std::fprintf(stderr,
                 "Failed to create GL shader object of type %s from file.\n",
                 String(type));
    return kStatus_GlError;
  }

  const GLchar *srcs[1] = {content->data()};
  const GLint lens[1] = {(GLint)content->size()};

  glShaderSource(*name, 1, srcs, lens);
  glCompileShader(*name);

  GLint status = GL_FALSE;
  glGetShaderiv(*name, GL_COMPILE_STATUS, &status);
  if (status == GL_FALSE) {
    std::fprintf(stderr, "Failed to compile shader.\n");

    GLchar info_log[INFO_LOG_BUFFER_SIZE];
    glGetShaderInfoLog(*name, INFO_LOG_BUFFER_SIZE, 0, info_log);
    std::fprintf(stderr, "%s", info_log);

    return kStatus_GlError;
  }

  return kStatus_Ok;
}

Status MakeProgram(const std::vector<GLuint> *shader_names, GLuint *name) {
  assert(shader_names);
  assert(name);

  *name = glCreateProgram();
  if (*name == 0) {
    std::fprintf(stderr, "Failed to create GL program.\n");
    return kStatus_GlError;
  }

  for (GLuint n : *shader_names) {
    glAttachShader(*name, n);
  }

  glLinkProgram(*name);

  GLint status;
  glGetProgramiv(*name, GL_LINK_STATUS, &status);
  if (status == GL_FALSE) {
    std::fprintf(stderr, "Failed to link program.\n");

    GLchar info_log[INFO_LOG_BUFFER_SIZE];
    glGetProgramInfoLog(*name, INFO_LOG_BUFFER_SIZE, 0, info_log);
    std::fprintf(stderr, "%s", info_log);

    return kStatus_GlError;
  }

  for (GLuint n : *shader_names) {
    glDeleteShader(n);
  }

  return kStatus_Ok;
}