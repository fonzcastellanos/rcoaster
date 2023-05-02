#ifndef _OPENGLHEADER_H_
#define _OPENGLHEADER_H_

#ifdef linux
#include <GL/glew.h>
#elif defined(__APPLE__)
#include <OpenGL/gl3.h>
#include <OpenGL/gl3ext.h>
#endif

#endif
