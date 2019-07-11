#ifndef BALLTRACKUTIL_H
#define BALLTRACKUTIL_H

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES/gl.h>
#include <GLES/glext.h>
//#include "interface/khronos/include/EGL/eglext_brcm.h"

#include <stdio.h>

//#define CHECK_GL_ERRORS
#ifdef CHECK_GL_ERRORS
#define GLCHK(X) \
{ \
    GLenum err = GL_NO_ERROR; \
    X; \
   if ((err = glGetError())) \
   { \
      printf("GL error 0x%x in " #X "file %s line %d\n", err, __FILE__,__LINE__); \
   } \
}
#else
#define GLCHK(X) X
#endif /* CHECK_GL_ERRORS */

/**
 * Container for a simple shader program. The uniform and attribute locations
 * are automatically setup by raspitex_build_shader_program.
 */
typedef struct SHADER_PROGRAM_T
{
   const char *vertex_source;       /// Pointer to vertex shader source
   const char *fragment_source;     /// Pointer to fragment shader source

   /// Array of uniform names for raspitex_build_shader_program to process
   const char *uniform_names[16];
   /// Array of attribute names for raspitex_build_shader_program to process
   const char *attribute_names[16];

   GLint vs;                        /// Vertex shader handle
   GLint fs;                        /// Fragment shader handle
   GLint program;                   /// Shader program handle

   /// The locations for uniforms defined in uniform_names
   GLint uniform_locations[16];

   /// The locations for attributes defined in attribute_names
   GLint attribute_locations[16];
} SHADER_PROGRAM_T;


int balltrack_build_shader_program(SHADER_PROGRAM_T *p);

int dump_frame(int width, int height, const char* filename);

#endif /* BALLTRACKUTIL_H */
