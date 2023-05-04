#ifndef RCOASTER_SHADER_HPP
#define RCOASTER_SHADER_HPP

#include <string>
#include <vector>

#include "opengl.hpp"
#include "status.hpp"

enum ShaderType {
  kShaderType_Vertex,
  kShaderType_Fragment,
  kShaderType__Count
};

GLenum GlShaderType(ShaderType t);

const char *String(ShaderType t);

ShaderType FromFilepath(const char *path);

Status LoadFile(const char *path, std::string *content);

Status MakeShaderObj(const std::string *src, ShaderType type, GLuint *name);

Status MakeShaderProg(const std::vector<GLuint> *shader_obj_names,
                      GLuint *name);

#endif  // RCOASTER_SHADER_HPP