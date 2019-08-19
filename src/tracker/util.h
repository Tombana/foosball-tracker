#ifndef BALLTRACKUTIL_H
#define BALLTRACKUTIL_H

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES/gl.h>
#include <GLES/glext.h>
//#include "interface/khronos/include/EGL/eglext_brcm.h"

#include <stdio.h>

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

struct Texture {
    GLuint id;
    int width;
    int height;
    GLuint type; // Always GL_TEXTURE_2D, except for camera input which is GL_TEXTURE_EXTERNAL_OES
};

class ShaderUniform {
  public:
    const char* name;
    enum { UNIFORM_NOTYPE, UNIFORM_INT, UNIFORM_FLOAT, UNIFORM_FLOAT2 } type;
    union {
        GLint i;
        GLfloat f;
        GLfloat f2[2];
    } value;

    GLint location; // Uniform location, given by OpenGL

    // The constructors allow for initial values to be given to the uniforms
    ShaderUniform() : name(0), type(UNIFORM_NOTYPE) {};
    ShaderUniform(const char* n) : name(n), type(UNIFORM_NOTYPE) {};
    ShaderUniform(const char* n, GLint x) : name(n), type(UNIFORM_INT) { value.i = x; };
    ShaderUniform(const char* n, GLfloat x) : name(n), type(UNIFORM_FLOAT) { value.f = x; };
    ShaderUniform(const char* n, GLfloat x1, GLfloat x2) : name(n), type(UNIFORM_FLOAT2) { value.f2[0] = x1; value.f2[1] = x2; };
};

/**
 * Container for a simple shader program. The uniform and attribute locations
 * are automatically setup by raspitex_build_shader_program.
 */
typedef struct SHADER_PROGRAM_T
{
   const char *vertex_source;       /// Pointer to vertex shader source
   const char *fragment_source;     /// Pointer to fragment shader source

   /// Array of uniforms for raspitex_build_shader_program to process
   ShaderUniform uniforms[16];
   /// Array of attribute names for raspitex_build_shader_program to process
   const char *attribute_names[16];

   GLint vs;                        /// Vertex shader handle
   GLint fs;                        /// Fragment shader handle
   GLint program;                   /// Shader program handle

   /// The locations for attributes defined in attribute_names
   GLint attribute_locations[16];
} SHADER_PROGRAM_T;

int balltrack_build_shader_program(SHADER_PROGRAM_T *p);

Texture createFilterTexture(int w, int h, GLint scaling);

int dump_frame(int width, int height, const char* filename);

int dump_buffer_to_console(int width, int height);

#endif /* BALLTRACKUTIL_H */
