#ifndef BALLTRACKUTIL_H
#define BALLTRACKUTIL_H

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES/gl.h>
#include <GLES/glext.h>
// This is already included by eglext.h
//#include "interface/khronos/include/EGL/eglext_brcm.h"
#include <cstdio>

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
#endif


//
// OpenGL texture that we can use for a render pass (render-to-texture)
//
// @param scaling How the texture is interpreted when it is a source texture
//      GL_NEAREST no interpolation for scaling down and up.
//      GL_LINEAR  interpolate between source pixels
class Texture {
  public:
    Texture() : id(0) {};
    Texture(int w, int h, GLint scaling = GL_NEAREST);
    virtual ~Texture() {
        if (id)
            GLCHK(glDeleteTextures(1, &id));
    };

    GLuint id;
    int width;
    int height;
    GLuint type; // Always GL_TEXTURE_2D, except GL_TEXTURE_EXTERNAL_OES for cam
};

// Texture that does not delete in the deconstructor
class TextureWrapper : public Texture {
  public:
    TextureWrapper(GLuint _id, int w, int h, GLuint _type) {
        id = _id;
        width = w;
        height = h;
        type = _type;
    }
    virtual ~TextureWrapper() { id = 0; } // avoid deletion
};

class SharedMemTexture : public Texture {
  public:
    SharedMemTexture() : Texture(), eglImage(EGL_NO_IMAGE_KHR) {};
    SharedMemTexture(int w, int h);
    virtual ~SharedMemTexture() {
        if (eglImage != EGL_NO_IMAGE_KHR)
            eglDestroyImageKHR(eglGetDisplay(EGL_DEFAULT_DISPLAY), (EGLImageKHR)eglImage);
    }

    int potWidth;  // power-of-two width
    int potHeight; // power-of-two height
    egl_image_brcm_vcsm_info vcsm_info;
    EGLImageKHR eglImage;
};

#ifdef USE_VCSM
typedef SharedMemTexture ReadoutTexture;
#else
typedef Texture ReadoutTexture;
#endif


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
class ShaderProgram {
  public:
    const char* display_name;    // For debug messages
    const char* vertex_source;   // Pointer to vertex shader source
    const char* fragment_source; // Pointer to fragment shader source

    // Array of uniforms for raspitex_build_shader_program to process
    ShaderUniform uniforms[16];
    // Array of attribute names for raspitex_build_shader_program to process
    const char* attribute_names[16];

    // Assumes that all the above info is valid
    int build();

    GLint vs;      // Vertex shader handle
    GLint fs;      // Fragment shader handle
    GLint program; // Shader program handle

    // The locations for attributes defined in attribute_names
    GLint attribute_locations[16];
};

void cleanupShaders();

int dump_frame(int width, int height, const char* filename);

int dump_buffer_to_console(int width, int height);

#endif /* BALLTRACKUTIL_H */
