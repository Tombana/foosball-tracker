// Mostly taken from RaspiTexUtil
#include "BalltrackUtil.h"
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
        if (! p->uniform_names[i])
            break;
        p->uniform_locations[i] = glGetUniformLocation(p->program, p->uniform_names[i]);
        if (p->uniform_locations[i] == -1)
        {
            printf("Failed to get location for uniform %s\n",
                  p->uniform_names[i]);
            goto fail;
        }
        else {
            //printf("Uniform for %s is %d\n", p->uniform_names[i], p->uniform_locations[i]);
        }
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

static void brga_to_rgba(uint8_t *buffer, size_t size)
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
    buffer = calloc(size, 1);
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

