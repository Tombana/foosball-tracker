// Mostly taken from RaspiTexUtil
#include "util.h"
#include "tga.h"
#include <cstdio>
#include <vector>

Texture::Texture(int w, int h, GLint scaling)
    : width(w), height(h), type(GL_TEXTURE_2D) {
    GLCHK(glGenTextures(1, &id));
    GLCHK(glBindTexture(GL_TEXTURE_2D, id));
    GLCHK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, scaling));
    GLCHK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, scaling));
    GLCHK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    GLCHK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
    GLCHK(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA,
                       GL_UNSIGNED_BYTE, NULL));
};

SharedMemTexture::SharedMemTexture(int w, int h) : Texture() {
    // Width and height must be a power of two between 64 and 2048
    // So find the smallest pot that is at least w,h
    type = GL_TEXTURE_2D;
    width = w;
    height = h;
    potWidth = 0;
    potHeight = 0;
    for (int pot = 64; pot <= 2048; pot *= 2) {
        if (pot >= w && potWidth == 0)
            potWidth = pot;
        if (pot >= h && potHeight == 0)
            potHeight = pot;
    }
    vcsm_info.width = potWidth;
    vcsm_info.height = potHeight;
    eglImage =
        eglCreateImageKHR(eglGetDisplay(EGL_DEFAULT_DISPLAY), EGL_NO_CONTEXT,
                          EGL_IMAGE_BRCM_VCSM, &vcsm_info, NULL);
    if (eglImage == EGL_NO_IMAGE_KHR || vcsm_info.vcsm_handle == 0) {
        printf("ERROR: Failed to create EGL VCSM image\n");
        return;
    }

    GLCHK(glGenTextures(1, &id));
    GLCHK(glBindTexture(GL_TEXTURE_2D, id));
    GLCHK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    GLCHK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));

    GLCHK(glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, eglImage));
    return;
}

std::vector<ShaderProgram*> loadedShaders;

int ShaderProgram::build()
{
    GLint status;
    int i = 0;
    char log[1024];
    int logLen = 0;

    printf("Building shader `%s`\n", display_name);

    if (! (vertex_source && fragment_source)) {
        printf("No shader sources specified.\n");
        return -1;
    }

    vs = fs = 0;

    vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vertex_source, NULL);
    glCompileShader(vs);
    glGetShaderiv(vs, GL_COMPILE_STATUS, &status);
    if (! status) {
        glGetShaderInfoLog(vs, sizeof(log), &logLen, log);
        printf("Program info log %s\n", log);
        goto fail;
    }

    fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fragment_source, NULL);
    glCompileShader(fs);

    glGetShaderiv(fs, GL_COMPILE_STATUS, &status);
    if (! status) {
        glGetShaderInfoLog(fs, sizeof(log), &logLen, log);
        printf("Program info log %s\n", log);
        goto fail;
    }

    program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    glGetProgramiv(program, GL_LINK_STATUS, &status);
    if (! status)
    {
        printf("Failed to link shader program\n");
        glGetProgramInfoLog(program, sizeof(log), &logLen, log);
        printf("Program info log %s\n", log);
        goto fail;
    }

    for (i = 0; i < 16; ++i)
    {
        if (! attribute_names[i])
            break;
        attribute_locations[i] = glGetAttribLocation(program, attribute_names[i]);
        if (attribute_locations[i] == -1)
        {
            printf("Failed to get location for attribute %s\n",
                  attribute_names[i]);
            goto fail;
        }
        else {
            //printf("Attribute for %s is %d\n", attribute_names[i], attribute_locations[i]);
        }
    }

    for (i = 0; i < 16; ++i)
    {
        ShaderUniform& u = uniforms[i];
        if (!u.name)
            break;
        u.location = glGetUniformLocation(program, u.name);
        if (u.location == -1) {
            printf("Failed to get location for uniform %s\n", u.name);
            u.type = ShaderUniform::UNIFORM_NOTYPE;
        }
    }

    glUseProgram(program);
    for (i = 0; i < 16; ++i)
    {
        ShaderUniform& u = uniforms[i];
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

    loadedShaders.push_back(this);

    return 0;

fail:
    printf("Failed to build shader program\n");
    glDeleteProgram(program);
    glDeleteShader(fs);
    glDeleteShader(vs);
    return -1;
}

void cleanupShaders() {
    for (auto& p : loadedShaders) {
        GLCHK(glDeleteShader(p->fs));
        GLCHK(glDeleteShader(p->vs));
        GLCHK(glDeleteProgram(p->program));
    }
    loadedShaders.clear();
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

