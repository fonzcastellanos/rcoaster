#ifndef _GLUT_HEADER_H_
#define _GLUT_HEADER_H_

#ifdef linux
#include <GL/glut.h>
#elif defined(__APPLE__)
// Assumes prior inclusion of `<OpenGL/gl3.h>`.
// `__gl_h_` prevents `<OpenGL/gl.h>` from being included because its inclusion
// would cause conflicts with `<OpenGL/gl3.h>`.
#define __gl_h_
#include <GLUT/glut.h>
#endif

#endif