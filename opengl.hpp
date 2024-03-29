#ifndef RCOASTER_OPENGL_HPP
#define RCOASTER_OPENGL_HPP

#ifdef linux
// clang-format off
#include <GL/glew.h>
#include <GL/freeglut.h>
// clang-format on
#elif defined(__APPLE__)
#include <OpenGL/gl3.h>
#include <OpenGL/gl3ext.h>
// `__gl_h_` prevents <GLUT/glut.h> from including `<OpenGL/gl.h>` because its
// inclusion would cause conflicts with `<OpenGL/gl3.h>`.
#define __gl_h_
#include <GLUT/glut.h>
#endif

#endif  // RCOASTER_OPENGL_HPP