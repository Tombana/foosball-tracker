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

