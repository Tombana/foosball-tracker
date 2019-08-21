// Mostly taken from RaspiTexUtil
#include "util.h"
#include "tga.h"
#include <stdio.h>

/**
 * Takes a description of shader program, compiles it and gets the locations
 * of uniforms and attributes.
 *
 * @param p The shader program state.
 * @return Zero if successful.
 */
int balltrack_build_shader_program(SHADER_PROGRAM_T *p)
{
    GLint status;
    int i = 0;
    char log[1024];
    int logLen = 0;

    if (! (p && p->vertex_source && p->fragment_source))
        goto fail;

    p->vs = p->fs = 0;

    p->vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(p->vs, 1, &p->vertex_source, NULL);
    glCompileShader(p->vs);
    glGetShaderiv(p->vs, GL_COMPILE_STATUS, &status);
    if (! status) {
        glGetShaderInfoLog(p->vs, sizeof(log), &logLen, log);
        printf("Program info log %s\n", log);
        goto fail;
    }

    p->fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(p->fs, 1, &p->fragment_source, NULL);
    glCompileShader(p->fs);

    glGetShaderiv(p->fs, GL_COMPILE_STATUS, &status);
    if (! status) {
        glGetShaderInfoLog(p->fs, sizeof(log), &logLen, log);
        printf("Program info log %s\n", log);
        goto fail;
    }

    p->program = glCreateProgram();
    glAttachShader(p->program, p->vs);
    glAttachShader(p->program, p->fs);
    glLinkProgram(p->program);
    glGetProgramiv(p->program, GL_LINK_STATUS, &status);
    if (! status)
    {
        printf("Failed to link shader program\n");
        glGetProgramInfoLog(p->program, sizeof(log), &logLen, log);
        printf("Program info log %s\n", log);
        goto fail;
    }

    for (i = 0; i < 16; ++i)
    {
        if (! p->attribute_names[i])
            break;
        p->attribute_locations[i] = glGetAttribLocation(p->program, p->attribute_names[i]);
        if (p->attribute_locations[i] == -1)
        {
            printf("Failed to get location for attribute %s\n",
                  p->attribute_names[i]);
            goto fail;
        }
        else {
            //printf("Attribute for %s is %d\n", p->attribute_names[i], p->attribute_locations[i]);
        }
    }

    for (i = 0; i < 16; ++i)
    {
        ShaderUniform& u = p->uniforms[i];
        if (!u.name)
            break;
        u.location = glGetUniformLocation(p->program, u.name);
        if (u.location == -1) {
            printf("Failed to get location for uniform %s\n", u.name);
            u.type = ShaderUniform::UNIFORM_NOTYPE;
        }
    }

    glUseProgram(p->program);
    for (i = 0; i < 16; ++i)
    {
        ShaderUniform& u = p->uniforms[i];
        if (!u.name)
            break;

        switch (u.type) {
            case ShaderUniform::UNIFORM_INT:
                glUniform1i(u.location, u.value.i);
                break;
            case ShaderUniform::UNIFORM_FLOAT:
                glUniform1f(u.location, u.value.f);
                break;
            case ShaderUniform::UNIFORM_FLOAT2:
                glUniform2f(u.location, u.value.f2[0], u.value.f2[1]);
                break;
            case ShaderUniform::UNIFORM_NOTYPE:
                // No default value wanted
                break;
            default:
                printf("Unknown type for shader uniform %s\n", u.name);
                break;
        };
    }

    return 0;

fail:
    printf("Failed to build shader program\n");
    if (p)
    {
        glDeleteProgram(p->program);
        glDeleteShader(p->fs);
        glDeleteShader(p->vs);
    }
    return -1;
}

int balltrack_delete_shader(SHADER_PROGRAM_T* p) {
    if (!p)
        return 0;
    GLCHK(glDeleteShader(p->fs));
    GLCHK(glDeleteShader(p->vs));
    GLCHK(glDeleteProgram(p->program));
    return 0;
}

//
// Creates an OpenGL texture that we can use for a render pass (render-to-texture)
//
// @param scaling How the texture is interpreted when it is a source texture
//      GL_NEAREST no interpolation for scaling down and up.
//      GL_LINEAR  interpolate between source pixels
// @return The OpenGL texture id
//
Texture createFilterTexture(int w, int h, GLint scaling) {
    Texture tex;
    tex.width = w;
    tex.height = h;
    tex.type = GL_TEXTURE_2D;
    GLCHK(glGenTextures(1, &tex.id));
    glBindTexture(GL_TEXTURE_2D, tex.id);
    GLCHK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, scaling));
    GLCHK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, scaling));
    //Wrapping: clamp. Only use (s,t) as we are using a 2D texture
    GLCHK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    GLCHK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
    GLCHK(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL));
    return tex;
}

SharedMemTexture createSharedMemTexture(int w, int h, GLint scaling) {
    SharedMemTexture tex;
    // Width and height must be a power of two between 64 and 2048
    // So find the smallest pot that is at least w,h
    tex.type = GL_TEXTURE_2D;
    tex.width = w;
    tex.height = h;
    tex.potWidth = 0;
    tex.potHeight = 0;
    //for (int pot = 64; pot <= 2048; pot *= 2) {
    //    if (pot >= w && tex.potWidth == 0)
    //        tex.potWidth = pot;
    //    if (pot >= h && tex.potHeight == 0)
    //        tex.potHeight = pot;
    //}
    tex.potWidth = 1024;
    tex.potHeight = 1024;

    GLCHK(glGenTextures(1, &tex.id));
    glBindTexture(GL_TEXTURE_2D, tex.id);
    GLCHK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, scaling));
    GLCHK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, scaling));
    GLCHK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    GLCHK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));

    tex.vcsm_info.width = tex.potWidth;
    tex.vcsm_info.height = tex.potHeight;
    tex.eglImage =
        eglCreateImageKHR(eglGetDisplay(EGL_DEFAULT_DISPLAY), EGL_NO_CONTEXT,
                          EGL_IMAGE_BRCM_VCSM, &tex.vcsm_info, NULL);
    if (tex.eglImage == EGL_NO_IMAGE_KHR || tex.vcsm_info.vcsm_handle == 0) {
        printf("Failed to create EGL VCSM image\n");
        GLCHK(glDeleteTextures(1, &tex.id));
        tex.id = 0;
        return tex;
    }

    GLCHK(glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, tex.eglImage));
    return tex;
}

void brga_to_rgba(uint8_t *buffer, size_t size)
{
   uint8_t* out = buffer;
   uint8_t* end = buffer + size;

   while (out < end)
   {
      uint8_t tmp = out[0];
      out[0] = out[2];
      out[2] = tmp;
      out += 4;
   }
}

int dump_frame(int width, int height, const char* filename) {
    FILE* output_file = fopen(filename, "wb");
    if (!output_file)
        return 1;

    uint8_t* buffer = NULL;
    size_t size = width * height * 4;
    buffer = (uint8_t*)calloc(size, 1);
    if (!buffer) {
        fclose(output_file);
        return 1;
    }

    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
    if (glGetError() == GL_NO_ERROR) {
        brga_to_rgba(buffer, size);
        write_tga(output_file, width, height, buffer, size);
    }

    fflush(output_file);
    fclose(output_file);
    free(buffer);

    return 0;
}

int dump_buffer_to_console(int width, int height) {
    uint8_t* buffer = (uint8_t*)malloc(width * height * 4);
    GLCHK(glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, buffer));

    uint8_t* ptr = buffer;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width*4; ++x) {
            uint8_t value = *ptr++;
            char o = '.';
            if (value > 150)
                o = 'X';
            else if (value > 50)
                o = 'x';
            else if (value > 10)
                o = ',';
            printf("%c", o);
        }
        printf("\n");
    }
    printf("\n");

    free(buffer);

    return 0;
}

