/*
Copyright (c) 2018, Tom Bannink
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "BalltrackCore.h"
#include "BallAnalysis.h"
#include <string.h>

// For writing to the FIFO python thing
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

// Divisions by 2 of 720p with correct aspect ratio
// 1280,720
//  640,360
//  320,180
//  160, 90
//   80, 45

// Every step maintains 720p aspect ratio
// -- Source is 720p
static int width0  = 1280;
static int height0 = 720;
// -- From source to phase 1: 2x2 sampler-average, then hue filter
// -- 2x2 pixels to 1 pixel
static int width1  = 640 / 2; // RGBA packs two pairs
static int height1 = 360;

#ifdef THREE_PHASES
// -- From phase 1 to phase 2: dilate 'red' filter, do not erode 'orange filter' but rescale?
// -- 4x4 pixels to 1 pixel (but sample 12x12 pixels to 1 pixel)
static int width2  = 160 / 2; // RGBA packs two pairs
static int height2 = 90;
// -- From phase 2 to phase 3: average for easier cpu handling ???
// -- 2x2 pixels to 1 pixel
static int width3  = 80 / 2; // RGBA packs two pairs
static int height3 = 45;
#else
// -- From phase 1 to phase 2: average
// -- 8x8 pixels to 1 pixel (but sample 12x12 pixels to 1 pixel)
static int width2  = 80 / 2; // RGBA packs two pairs
static int height2 = 45;
static int width3  = 80 / 2;
static int height3 = 45;
#endif


static GLfloat quad_varray[] = {
   -1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f,
   -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f,
};

static GLuint quad_vbo; // vertex buffer object
static GLuint fbo;      // frame buffer object for render-to-texture
static GLuint rtt_tex1; // Texture for render-to-texture
static GLuint rtt_tex2; // Texture for render-to-texture
static GLuint rtt_tex3; // Texture for render-to-texture
static GLuint rtt_copytex;

static uint8_t* pixelbuffer; // For reading out result

#define DEBUG 3
// Autogenerated file containing all shaders
#include "balltrackshaders/allshaders.h"

SHADER_PROGRAM_T balltrack_shader_1 =
{
    .vertex_source = (char*)vshader_vert,
    .fragment_source = (char*)phase1_frag,
    .uniform_names = {"tex", "tex_unit"},
    .attribute_names = {"vertex"},
};

static SHADER_PROGRAM_T balltrack_shader_2 =
{
    .vertex_source = (char*)vshader_vert,
    .fragment_source = (char*)phase2_frag,
    .uniform_names = {"tex", "tex_unit"},
    .attribute_names = {"vertex"},
};

static SHADER_PROGRAM_T balltrack_shader_3 =
{
    .vertex_source = (char*)vshader_vert,
    .fragment_source = (char*)phase3_frag,
    .uniform_names = {"tex", "tex_unit"},
    .attribute_names = {"vertex"},
};

static SHADER_PROGRAM_T balltrack_shader_display =
{
    .vertex_source = (char*)vshader_vert,
    .fragment_source = (char*)display_frag,
    .uniform_names = {"tex_camera", "tex_unit", "tex_filter"},
    .attribute_names = {"vertex"},
};

static SHADER_PROGRAM_T balltrack_shader_fixedcolor =
{
    .vertex_source = (char*)vshader_vert,
    .fragment_source = (char*)fixedcolor_frag,
    .uniform_names = {"col"},
    .attribute_names = {"vertex"},
};

static SHADER_PROGRAM_T balltrack_shader_plain =
{
    .vertex_source = (char*)vshader_vert,
    .fragment_source = (char*)plain_frag,
    .uniform_names = {"tex_rgb", "tex_y", "tex_u", "tex_v"},
    .attribute_names = {"vertex"},
};

static SHADER_PROGRAM_T balltrack_shader_diff =
{
    .vertex_source = (char*)vshader_vert,
    .fragment_source = (char*)diff_frag,
    .uniform_names = {"tex1", "tex2"},
    .attribute_names = {"vertex"},
};

// Initialization of shader uniforms.
static int shader_set_uniforms(SHADER_PROGRAM_T *shader,
      int width, int height, int texunit, int extratex)
{
   GLCHK(glUseProgram(shader->program));
   GLCHK(glUniform1i(shader->uniform_locations[0], 0)); // Texture unit

   if (texunit) {
       // Dimensions of a single source pixel in texture co-ordinates
       GLCHK(glUniform2f(shader->uniform_locations[1], 1.0 / (float) width, 1.0 / (float) height));
   }
   
   if (extratex) {
       GLCHK(glUniform1i(shader->uniform_locations[2], 1)); // Extra texture
   }

   /* Enable attrib 0 as vertex array */
   GLCHK(glEnableVertexAttribArray(shader->attribute_locations[0]));
   return 0;
}

static GLuint createFilterTexture(int w, int h, GLint scaling) {
    GLuint id;
    GLCHK(glGenTextures(1, &id));
    glBindTexture(GL_TEXTURE_2D, id);
    //Scaling: GL_NEAREST no interpolation for scaling down and up.
    //Scaling: GL_LINEAR  interpolate between source pixels
    GLCHK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, scaling));
    GLCHK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, scaling));
    //Wrapping: clamp. Only use (s,t) as we are using a 2D texture
    GLCHK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    GLCHK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
    GLCHK(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL));
    return id;
}

int balltrack_core_init(int externalSamplerExtension, int flipY)
{
    int rc = 0;

    const char* glRenderer = (const char*)glGetString(GL_RENDERER);
    printf("OpenGL renderer string: %s\n", glRenderer);

    if (externalSamplerExtension == 0) {
        // Replace
        // "samplerExternalOES"
        // "sampler2D         "
        char* pos = 0;
        if ((pos = strstr(balltrack_shader_1.fragment_source, "samplerExternalOES"))){
            memcpy(pos, "sampler2D         ", 18);
        }
        if ((pos = strstr(balltrack_shader_display.fragment_source, "samplerExternalOES"))){
            memcpy(pos, "sampler2D         ", 18);
        }
        if ((pos = strstr(balltrack_shader_plain.fragment_source, "samplerExternalOES"))){
            memcpy(pos, "sampler2D         ", 18);
        }
        if ((pos = strstr(balltrack_shader_diff.fragment_source, "samplerExternalOES"))){
            memcpy(pos, "sampler2D         ", 18);
        }
    }

    // Camera source is Y-flipped.
    // So flip it back in the vertex shader
    if (flipY) {
        // TODO:
        // Do not flip the render-to-texture textures ??
        //balltrack_shader_1.vertex_source = BALLTRACK_VSHADER_YFLIP_SOURCE;
        //balltrack_shader_2.vertex_source = BALLTRACK_VSHADER_YFLIP_SOURCE;
        //balltrack_shader_3.vertex_source = BALLTRACK_VSHADER_YFLIP_SOURCE;
    }

    printf("Building shader `phase 1`\n");
    rc = balltrack_build_shader_program(&balltrack_shader_1);
    if (rc != 0)
        goto end;
    rc = shader_set_uniforms(&balltrack_shader_1, width0, height0, 1, 0);
    if (rc != 0)
        goto end;

    printf("Building shader `phase 2`\n");
    rc = balltrack_build_shader_program(&balltrack_shader_2);
    if (rc != 0)
        goto end;
    rc = shader_set_uniforms(&balltrack_shader_2, width1, height1, 1, 0);
    if (rc != 0)
        goto end;

    printf("Building shader `phase 3`\n");
    rc = balltrack_build_shader_program(&balltrack_shader_3);
    if (rc != 0)
        goto end;
    rc = shader_set_uniforms(&balltrack_shader_3, width2, height2, 1, 0);
    if (rc != 0)
        goto end;

    printf("Building shader `display`\n");
    rc = balltrack_build_shader_program(&balltrack_shader_display);
    if (rc != 0)
        goto end;
#if DEBUG == 1
    rc = shader_set_uniforms(&balltrack_shader_display, width1, height1, 1, 1);
#elif DEBUG == 2
    rc = shader_set_uniforms(&balltrack_shader_display, width2, height2, 1, 1);
#else
    rc = shader_set_uniforms(&balltrack_shader_display, width3, height3, 1, 1);
#endif
    if (rc != 0)
        goto end;

    printf("Building shader `fixedcolor`\n");
    rc = balltrack_build_shader_program(&balltrack_shader_fixedcolor);
    if (rc != 0)
        goto end;

    printf("Building shader `plain`\n");
    rc = balltrack_build_shader_program(&balltrack_shader_plain);
    if (rc != 0)
        goto end;
    rc = shader_set_uniforms(&balltrack_shader_plain, width0, height0, 0, 0);
    if (rc != 0)
        goto end;
#ifdef DO_YUV
    GLCHK(glUniform1i(balltrack_shader_plain.uniform_locations[1], 2)); // Texture unit
    GLCHK(glUniform1i(balltrack_shader_plain.uniform_locations[2], 3)); // Texture unit
    GLCHK(glUniform1i(balltrack_shader_plain.uniform_locations[3], 4)); // Texture unit
#endif

    printf("Building shader `diff`\n");
    rc = balltrack_build_shader_program(&balltrack_shader_diff);
    if (rc != 0)
        goto end;
    rc = shader_set_uniforms(&balltrack_shader_diff, width0, height0, 0, 0);
    if (rc != 0)
        goto end;
    GLCHK(glUniform1i(balltrack_shader_diff.uniform_locations[1], 1)); // Texture unit

    // Buffer to read out pixels from last texture
    uint32_t buffer_size = width3 * height3 * 4;
    pixelbuffer = calloc(buffer_size, 1);
    if (!pixelbuffer) {
        rc = -1;
        goto end;
    }

    printf("Generating framebuffer object\n");
    // Create frame buffer object for render-to-texture
    GLCHK(glGenFramebuffersOES(1, &fbo));
    GLCHK(glBindFramebufferOES(GL_FRAMEBUFFER_OES, fbo));
    GLCHK(glBindFramebufferOES(GL_FRAMEBUFFER_OES, 0)); // unbind it

    printf("Creating render-to-texture targets\n");
    GLint tex1scaling = GL_LINEAR;
    GLint tex2scaling = GL_LINEAR;
    GLint tex3scaling = GL_NEAREST;
#if DEBUG == 1
    tex1scaling = GL_NEAREST;
#elif DEBUG == 2
    tex2scaling = GL_NEAREST;
#endif
    rtt_tex1 = createFilterTexture(width1, height1, tex1scaling);
    rtt_tex2 = createFilterTexture(width2, height2, tex2scaling);
#ifdef THREE_PHASES
    rtt_tex3 = createFilterTexture(width3, height3, tex3scaling);
#else
    (void)tex3scaling;
    rtt_tex3 = rtt_tex2; // hmmmm....
#endif
    rtt_copytex = createFilterTexture(width0, height0, GL_NEAREST);

    printf("Creating vertex-buffer object\n");
    GLCHK(glGenBuffers(1, &quad_vbo));
    GLCHK(glBindBuffer(GL_ARRAY_BUFFER, quad_vbo));
    GLCHK(glBufferData(GL_ARRAY_BUFFER, sizeof(quad_varray), quad_varray, GL_STATIC_DRAW));
    GLCHK(glClearColor(0.0f, 0.0f, 0.0f, 0.0f));

    GLCHK(glDisable(GL_BLEND));
    GLCHK(glDisable(GL_DEPTH_TEST));

end:
    return rc;
}

// x,y are coordinates in [-1,1]x[-1,1] range
void draw_line_strip(POINT* xys, int count, uint32_t color) {
    if (count == 0)
        return;

    float r, g, b, a;
    r = (1.0/255.0) * ((color      ) & 0xff);
    g = (1.0/255.0) * ((color >>  8) & 0xff);
    b = (1.0/255.0) * ((color >> 16) & 0xff);
    a = (1.0/255.0) * ((color >> 24) & 0xff);

    SHADER_PROGRAM_T* shader = &balltrack_shader_fixedcolor;
    GLCHK(glUseProgram(shader->program));
    GLCHK(glUniform4f(shader->uniform_locations[0], r, g, b, a));

    // Unbind the vertex buffer --> use client memory
    GLCHK(glBindBuffer(GL_ARRAY_BUFFER, 0));
    GLCHK(glEnableVertexAttribArray(shader->attribute_locations[0]));
    GLCHK(glVertexAttribPointer(shader->attribute_locations[0], 2, GL_FLOAT, GL_FALSE, 0, (GLfloat*)xys));
    // Draw
    GLCHK(glDrawArrays(GL_LINE_STRIP, 0, count));
}

// x,y are coordinates in [-1,1]x[-1,1] range
void draw_square(float xmin, float xmax, float ymin, float ymax, uint32_t color) {
    // Draw a square
    POINT vertexBuffer[5];
    vertexBuffer[0].x = xmin;
    vertexBuffer[0].y = ymin;
    vertexBuffer[1].x = xmax;
    vertexBuffer[1].y = ymin;
    vertexBuffer[2].x = xmax;
    vertexBuffer[2].y = ymax;
    vertexBuffer[3].x = xmin;
    vertexBuffer[3].y = ymax;
    vertexBuffer[4].x = xmin;
    vertexBuffer[4].y = ymin;
    draw_line_strip(vertexBuffer, 5, color);
}

// If target_tex is zero, then target is the screen
static int render_pass(SHADER_PROGRAM_T* shader, GLuint source_type, GLuint source_tex, GLuint target_tex, int targetWidth, int targetHeight) {
    GLCHK(glUseProgram(shader->program));
    if (target_tex) {
        // Enable Render-to-texture and set the output texture
        GLCHK(glBindFramebufferOES(GL_FRAMEBUFFER_OES, fbo));
        GLCHK(glFramebufferTexture2DOES(GL_FRAMEBUFFER_OES, GL_COLOR_ATTACHMENT0_OES, GL_TEXTURE_2D, target_tex, 0));
        GLCHK(glViewport(0, 0, targetWidth, targetHeight));
        // According to the open source GL driver for the VC4 chip,
        // [ https://github.com/anholt/mesa/wiki/VC4-Performance-Tricks ],
        // it is faster to clear the buffer even when writing to the complete screen
        glClear(GL_COLOR_BUFFER_BIT);
    } else {
        // Unset frame buffer object. Now draw to screen
        GLCHK(glBindFramebufferOES(GL_FRAMEBUFFER_OES, 0));
        GLCHK(glViewport(0, 0, targetWidth, targetHeight));
        glClear(GL_COLOR_BUFFER_BIT); // See above comment
    }
    // Bind the input texture
    GLCHK(glActiveTexture(GL_TEXTURE0));
    GLCHK(glBindTexture(source_type, source_tex));
    // Bind the vertex buffer
    GLCHK(glBindBuffer(GL_ARRAY_BUFFER, quad_vbo));
    GLCHK(glEnableVertexAttribArray(shader->attribute_locations[0]));
    GLCHK(glVertexAttribPointer(shader->attribute_locations[0], 2, GL_FLOAT, GL_FALSE, 0, 0));
    // Draw
    GLCHK(glDrawArrays(GL_TRIANGLES, 0, 6));
    return 0;
}

static int frameNumber = 0;

static int balltrack_readout(int width, int height) {
    // Read texture
    // It packs two pixels into one:
    // RGBA is red,green,red,green filter values for neighbouring pixels
    if (pixelbuffer) {
        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixelbuffer);
        if (glGetError() == GL_NO_ERROR) {
            // pixelbuffer[i*height + j] is i pixels from bottom and j from left
            int gxmin = (int)(0.45f * 2.0f * width);
            int gxmax = (int)(0.55f * 2.0f * width);
            int gymin = (int)(0.45f * height);
            int gymax = (int)(0.55f * height);

            uint32_t* ptr = (uint32_t*)pixelbuffer;
            for (int i = 0; i < height; ++i) {
                for (int j = 0; j < width; ++j) {
                    // R,G,B,A = FF, 00, FF, FF
                    uint32_t rgba = *ptr++;
                    //int R1 = (rgba      ) & 0xff;
                    int G1 = (rgba >>  8) & 0xff;
                    //int R2 = (rgba >> 16) & 0xff;
                    int G2 = (rgba >> 24) & 0xff;

                    int y = i;
                    int x1 = 2*j;
                    int x2 = 2*j + 1;
                    // This part could be optimized...
                    // - y min/max check only once
                    // - if x1<gxmin then x2 does not need to be checked
                    // - ...
                    if (G1 > 140) {
                        if (x1 < gxmin) gxmin = x1;
                        if (x1 > gxmax) gxmax = x1;
                        if (y < gymin) gymin = y;
                        if (y > gymax) gymax = y;
                    }
                    if (G2 > 140) {
                        if (x2 < gxmin) gxmin = x2;
                        if (x2 > gxmax) gxmax = x2;
                        if (y < gymin) gymin = y;
                        if (y > gymax) gymax = y;
                    }
                }
            }
            gxmin -= 4;
            gxmax += 4;
            gymin -= 3;
            gymax += 3;
            if (gxmin < 0) gxmin = 0;
            if (gymin < 0) gymin = 0;
            if (gxmax > 2*width-1) gxmax = 2*width - 1;
            if (gymax > height) gymax = height;

            // Map to [-1,1]
            FIELD field;
            field.xmin = gxmin / ((float)width) - 1.0f;
            field.xmax = gxmax / ((float)width) - 1.0f;
            field.ymin = (2.0f * gymin) / ((float)height) - 1.0f;
            field.ymax = (2.0f * gymax) / ((float)height) - 1.0f;

            // Find the max orange intensity
            uint32_t maxx = 0, maxy = 0;
            uint32_t maxR = 0;
            ptr = (uint32_t*)pixelbuffer;
            for (int i = 0; i < height; ++i) {
                for (int j = 0; j < width; ++j) {
                    // R,G,B,A = FF, 00, FF, FF
                    uint32_t rgba = *ptr++;
                    int R1 = (rgba      ) & 0xff;
                    //int G1 = (rgba >>  8) & 0xff;
                    int R2 = (rgba >> 16) & 0xff;
                    //int G2 = (rgba >> 24) & 0xff;
                    int y = i;
                    int x1 = 2*j;
                    int x2 = 2*j + 1;
                    if ( y < gymin || y > gymax ) continue;
                    if ( x1 < gxmin || x2 > gxmax ) continue;
                    if (R1 > maxR) {
                        maxx = x1;
                        maxy = y;
                        maxR = R1;
                    }
                    if (R2 > maxR) {
                        maxx = x2;
                        maxy = y;
                        maxR = R2;
                    }
                }
            }
            // Take weighted average near the maximum
            int searchImin = maxy - 4;
            int searchImax = maxy + 4;
            int searchJmin = maxx/2 - 3;
            int searchJmax = (maxx+1)/2 + 3;
            if (searchImin < 0) searchImin = 0;
            if (searchJmin < 0) searchJmin = 0;
            if (searchImax > height) searchImax = height;
            if (searchJmax > width ) searchJmax = width;

            uint32_t avgx = 0, avgy = 0;
            uint32_t weight = 0;
            uint32_t count = 0;
            ptr = (uint32_t*)pixelbuffer;
            for (int i = searchImin; i < searchImax; ++i) {
                for (int j = searchJmin; j < searchJmax; ++j) {
                    // R,G,B,A = FF, 00, FF, FF
                    uint32_t rgba = ptr[i * width + j];
                    int R1 = (rgba      ) & 0xff;
                    //int G1 = (rgba >>  8) & 0xff;
                    int R2 = (rgba >> 16) & 0xff;
                    //int G2 = (rgba >> 24) & 0xff;
                    int y = i;
                    int x1 = 2*j;
                    int x2 = 2*j + 1;
                    avgx += x1 * R1;
                    avgy += y  * R1;
                    weight += R1;
                    avgx += x2 * R2;
                    avgy += y  * R2;
                    weight += R2;
                    count++;
                }
            }

            int threshold1 = 30;
            int threshold2 = 60;

            // avgx, avgy are the bottom-left corner of the macropixels
            // Shift them by half a pixel to fix
            // Then, map them to [-1,1] range
            float x = 0.5f + (((float)avgx) / ((float)weight));
            float y = 0.5f + (((float)avgy) / ((float)weight));
            POINT ball;
            ball.x = x / ((float)width) - 1.0f;
            ball.y = (2.0f * y) / ((float)height) - 1.0f;

            if (maxR > threshold1 && weight > threshold2) {
                analysis_update(field, ball, 1);
            } else {
                analysis_update(field, ball, 0);
            }
        } else {
            printf("glReadPixels failed!");
        }
    }
    return 0;
}

// Same but called from video player version
int balltrack_core_redraw(int width, int height, GLuint srctex, GLuint srctype)
{
    ++frameNumber;
    // Width,height is the size of the preview window

#ifdef DO_FRAMEDUMP
    if (frameNumber == 60) {
        render_pass(&balltrack_shader_plain, srctype, srctex, 0, width, height);
        dump_frame(width, height, "framedump.tga");
        printf("Frame dumped to framedump.tga\n");
    }
#endif

#if DO_DIFF
    // Diff with previous
    GLCHK(glActiveTexture(GL_TEXTURE1));
    GLCHK(glBindTexture(GL_TEXTURE_2D, rtt_copytex));
    render_pass(&balltrack_shader_diff, srctype, srctex, 0, width, height);

    // Copy source to previous
    render_pass(&balltrack_shader_plain, srctype, srctex, rtt_copytex, width0, height0);
#endif

    // First pass: hue filter into smaller texture
    render_pass(&balltrack_shader_1, srctype,       srctex,   rtt_tex1, width1, height1);
    // Second pass: dilate red players
    render_pass(&balltrack_shader_2, GL_TEXTURE_2D, rtt_tex1, rtt_tex2, width2, height2);
#ifdef THREE_PHASES
    // Third pass: downsample
    render_pass(&balltrack_shader_3, GL_TEXTURE_2D, rtt_tex2, rtt_tex3, width3, height3);
#endif
    // Readout result
    balltrack_readout(width3, height3);
    // Third pass: render to screen
    GLCHK(glActiveTexture(GL_TEXTURE1));
#if DEBUG == 1
    GLCHK(glBindTexture(GL_TEXTURE_2D, rtt_tex1));
#elif DEBUG == 2
    GLCHK(glBindTexture(GL_TEXTURE_2D, rtt_tex2));
#else
    GLCHK(glBindTexture(GL_TEXTURE_2D, rtt_tex3));
#endif
    render_pass(&balltrack_shader_display, srctype, srctex, 0, width, height);

    analysis_draw();

    return 0;
}

