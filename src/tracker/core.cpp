/*
Copyright (c) 2018, Tom Bannink
MIT License
*/
#include "core.h"
#include "util.h"
#include "analysis.h"
#include <cstring>
#include <cstdio>
#define VCOS_LOG_CATEGORY (&balltrack_log_category)
#include "interface/vcos/vcos.h" // For threads and semaphores
#include "interface/vcsm/user-vcsm.h" // For creating the videocore-shared-memory texture
VCOS_LOG_CAT_T balltrack_log_category;

// Update the size of the (green) field bounding box every X frames
constexpr int FieldUpdateDelay = 20;

// Whether to use bigger (more finegrained, but slower) textures
#define BIGTEX

// Debug feature, debugs a few frames to tga files.
//#define DO_FRAMEDUMPS

// Divisions by 2 of 720p with correct aspect ratio
// 1280,720
//  640,360
//  320,180
//  160, 90
//   80, 45

// Every step maintains the same aspect ratio
// -- Source is 720p
const int width0  = 1280;
const int height0 = 720;
#ifdef BIGTEX
// -- Phase 1: from source to tex1: hue filter on every pixel
const int width1  = 1280 / 4; // RGBA can store 4 values at once
const int height1 = 720;
// -- Phase 2: from tex1 to tex2: average 8x8 pixels to 1 pixel
const int width2  = 160 / 4;
const int height2 = 90;
#else
// -- Phase 1: from source to tex1: 2x2 sampler-average, then hue filter
const int width1  = 640 / 4; // RGBA can store 4 values at once
const int height1 = 360;
// -- Phase 2: from tex1 to tex2: average 8x8 pixels to 1 pixel
const int width2  = 80 / 4;
const int height2 = 45;
#endif


//
// Textures
// Instead of doing this, in series, every frame:
// source -> tex1 -> tex2 -> readout
//
// We do the three steps in parallel
// source  -> tex1[0]
// tex1[1] -> tex2[0]
// tex2[1] -> readout
// (then swap 0,1)
//
// So it takes 3 frames before the source data gets to the readout phase,
// but every transformation can now be done in parallel, instead of in series
//
// For the field textures, we do not need this, since this only happens
// every n frames, so we can simply do:
// Frame n-2: source    -> tex1field
// Frame n-1: tex1field -> tex2field
// Frame n  : tex2field -> readout

// These are swapped around every frame
Texture* texHueFilter_read = 0;
Texture* texHueFilter_write = 0;
ReadoutTexture* texDownscaled_read = 0;
ReadoutTexture* texDownscaled_write = 0;

Texture* texHueFilter[2];
Texture* texHueFilterField;
ReadoutTexture* texDownscaled[2];
ReadoutTexture* texDownscaledField;

#ifdef DO_DIFF
Texture* rtt_copytex;
#endif

#ifdef DO_FRAMEDUMPS
Texture* texFramedump;
#endif

enum PixelBufferType { BUFFERTYPE_BALL = 0, BUFFERTYPE_FIELD = 1 };

constexpr int PIXELBUFFER_COUNT = 4;
PixelBufferType pixelbufferType[PIXELBUFFER_COUNT];
uint8_t* pixelbuffers[PIXELBUFFER_COUNT]; // For reading out result

int nextEmptyBuffer = 0; // For writing buffers
int nextFullBuffer = 0;  // For reading buffers

void* analysis_thread(void *arg);
int analysis_stop = 0;
VCOS_THREAD_T analysis_thread_handle;
VCOS_SEMAPHORE_T semEmptyCount; // analysis -> GL signal
VCOS_SEMAPHORE_T semFullCount;  // GL -> analysis signal


GLfloat quad_varray[] = {
   -1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f,
   -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f,
};

GLuint quad_vbo; // vertex buffer object
GLuint fbo;      // frame buffer object for render-to-texture

bool allInitialized = false;


// Autogenerated file containing all shaders
#include "balltrackshaders/allshaders.h"

ShaderProgram shader_huefilter_ball =
{
    .display_name = "huefilter_ball",
    .vertex_source = (char*)vshader_vert,
    .fragment_source = (char*)huefilterball_frag,
    .uniforms = {ShaderUniform("tex", 0), ShaderUniform("tex_unit", 1.0f / (float)width0, 1.0f / (float)height0)},
    .attribute_names = {"vertex"},
};

ShaderProgram shader_huefilter_field =
{
    .display_name = "huefilter_field",
    .vertex_source = (char*)vshader_vert,
    .fragment_source = (char*)huefilterfield_frag,
    .uniforms = {ShaderUniform("tex", 0), ShaderUniform("tex_unit", 1.0f / (float)width0, 1.0f / (float)height0)},
    .attribute_names = {"vertex"},
};

ShaderProgram shader_downsample =
{
    .display_name = "downsample",
    .vertex_source = (char*)vshader_vert,
    .fragment_source = (char*)downsample_frag,
    .uniforms = {ShaderUniform("tex", 0), ShaderUniform("tex_unit", 1.0f / (float)width1, 1.0f / (float)height1)},
    .attribute_names = {"vertex"},
};

ShaderProgram shader_simple =
{
    .display_name = "simple",
    .vertex_source = (char*)vshader_vert,
    .fragment_source = (char*)simple_frag,
    .uniforms = {ShaderUniform("tex", 0)},
    .attribute_names = {"vertex"},
};

ShaderProgram shader_debug =
{
    .display_name = "debug",
    .vertex_source = (char*)vshader_vert,
    .fragment_source = (char*)debug_frag,
    .uniforms = {
        ShaderUniform("tex_camera", 0),
        ShaderUniform("tex_dbg1", 1),
        ShaderUniform("tex_dbg2", 2),
        ShaderUniform("tex_dbg3", 3),
        ShaderUniform("pixelwidth1", 1.0f / (float)width1),
        ShaderUniform("pixelwidth2", 1.0f / (float)width2),
        ShaderUniform("pixelwidth3", 1.0f / (float)width2),
    },
    .attribute_names = {"vertex"},
};

ShaderProgram shader_fixedcolor =
{
    .display_name = "fixedcolor",
    .vertex_source = (char*)vshader_vert,
    .fragment_source = (char*)fixedcolor_frag,
    .uniforms = { ShaderUniform("col") },
    .attribute_names = {"vertex"},
};

#ifdef DO_YUV
ShaderProgram shader_yuv =
{
    .display_name = "yuv",
    .vertex_source = (char*)vshader_vert,
    .fragment_source = (char*)yuvsource_frag,
    .uniforms = { ShaderUniform("tex_rgb", 0),
        ShaderUniform("tex_y", 2),
        ShaderUniform("tex_u", 3),
        ShaderUniform("tex_v", 4)
    },
    .attribute_names = {"vertex"},
};
#endif

#ifdef DO_DIFF
ShaderProgram shader_diff =
{
    .display_name = "diff",
    .vertex_source = (char*)vshader_vert,
    .fragment_source = (char*)diff_frag,
    .uniforms = { ShaderUniform("tex1", 0), ShaderUniform("tex2", 1) },
    .attribute_names = {"vertex"},
};
#endif

void replace_sampler_string(const char* text) {
    // In text, find and replace:
    // "samplerExternalOES"
    // "sampler2D         "
    char* pos = 0;
    if ((pos = strstr((char*)text, "samplerExternalOES"))){
        memcpy(pos, "sampler2D         ", 18);
    }
}

int balltrack_core_init(int externalSamplerExtension, int flipY)
{
    vcos_log_register("Balltracker", VCOS_LOG_CATEGORY);
    vcos_log_set_level(VCOS_LOG_CATEGORY, VCOS_LOG_INFO);

#ifdef USE_VCSM
    // Initialize VideoCore Shared Memory
    // So that we can readout the result of the GPU using the CPU,
    // directly accessing the memory instead of using glReadPixels.
    // This should be faster, however, it access to `dev/vcsm`.
    if (vcsm_init()) {
        printf("ERROR: Could not access /dev/vcsm (VideoCore Shared Memory).\n");
        printf("       Use sudo or read the README for better ways to access /dev/vcsm.\n");
        return -1;
    }
#endif

    const char* glRenderer = (const char*)glGetString(GL_RENDERER);
    printf("OpenGL renderer string: %s\n", glRenderer);

    // When the source data comes from the camera, the shaders have
    //     samplerExternalOES
    // and when the data comes from a video file, the shaders need
    //     sampler2D
    // So this code replaces those when needed.
    if (externalSamplerExtension == 0) {
        replace_sampler_string(shader_huefilter_ball.fragment_source);
        replace_sampler_string(shader_huefilter_field.fragment_source);
        replace_sampler_string(shader_debug.fragment_source);
        replace_sampler_string(shader_simple.fragment_source);
#ifdef DO_YUV
        replace_sampler_string(shader_yuv.fragment_source);
#endif
#ifdef DO_DIFF
        replace_sampler_string(shader_diff.fragment_source);
#endif
    }

    // Camera source is Y-flipped, replay videos are not.
    // So flip it back in the vertex shader
    if (flipY) {
        // TODO:
        // Do not flip the render-to-texture textures ??
        //balltrack_shader_1.vertex_source = BALLTRACK_VSHADER_YFLIP_SOURCE;
        //balltrack_shader_2.vertex_source = BALLTRACK_VSHADER_YFLIP_SOURCE;
        //balltrack_shader_3.vertex_source = BALLTRACK_VSHADER_YFLIP_SOURCE;
    }

    if (shader_huefilter_ball.build())
        return -1;
    if (shader_huefilter_field.build())
        return -1;
    if (shader_downsample.build())
        return -1;
#ifdef DEBUG_TEXTURES
    if (shader_debug.build())
        return -1;
#endif
    if (shader_simple.build())
        return -1;
    if (shader_fixedcolor.build())
        return -1;
#ifdef DO_YUV
    if (shader_yuv.build())
        return -1;
#endif
#ifdef DO_DIFF
    if (shader_diff.build())
        return -1;
#endif

    // Buffer to read out pixels from last texture
    uint32_t buffer_size = width2 * height2 * 4;
    for (int i = 0; i < PIXELBUFFER_COUNT; ++i) {
        pixelbuffers[i] = (uint8_t*)malloc(buffer_size);
        if (!pixelbuffers[i]) {
            printf("Could not allocate pixelbuffer.\n");
            return -1;
        }
    }

    // Create frame buffer object for render-to-texture
    printf("Generating framebuffer object\n");
    GLCHK(glGenFramebuffersOES(1, &fbo));
    GLCHK(glBindFramebufferOES(GL_FRAMEBUFFER_OES, fbo));
    GLCHK(glBindFramebufferOES(GL_FRAMEBUFFER_OES, 0)); // unbind it

    printf("Creating render-to-texture targets\n");
    for (int i = 0; i < 2; ++i) {
        texHueFilter[i] = new Texture(width1, height1, GL_LINEAR);
        texDownscaled[i] = new ReadoutTexture(width2, height2);
    }
    texHueFilter_read = texHueFilter[0];
    texHueFilter_write = texHueFilter[1];
    texDownscaled_read = texDownscaled[0];
    texDownscaled_write = texDownscaled[1];

    texHueFilterField = new Texture(width1, height1, GL_LINEAR);
    texDownscaledField = new ReadoutTexture(width2, height2);

#ifdef DO_DIFF
    rtt_copytex = new Texture(width0, height0, GL_NEAREST);
#endif
#ifdef DO_FRAMEDUMPS
    texFramedump = new Texture(width0, height0, GL_NEAREST);
#endif

    printf("Creating vertex-buffer object\n");
    GLCHK(glGenBuffers(1, &quad_vbo));
    GLCHK(glBindBuffer(GL_ARRAY_BUFFER, quad_vbo));
    GLCHK(glBufferData(GL_ARRAY_BUFFER, sizeof(quad_varray), quad_varray, GL_STATIC_DRAW));
    GLCHK(glClearColor(0.0f, 0.0f, 0.0f, 0.0f));

    GLCHK(glDisable(GL_BLEND));
    GLCHK(glDisable(GL_DEPTH_TEST));

    // Create a binary and N-ary semaphore and start an analysis thread
    // Also allocate N pixelbuffers
    // For every glReadPixels, use semaphores to get a free buffer
    // and read into there.
    // Then the analysis thread can consume the buffers from there,
    // while the GL thread continues.
    VCOS_STATUS_T status;

    status = vcos_semaphore_create(&semFullCount, "analysis_fullcount", 0);
    if (status != VCOS_SUCCESS) {
        printf("Failed to create balltrack semaphore %d\n", status);
        return -1;
    }

    status = vcos_semaphore_create(&semEmptyCount, "analysis_emptycount", PIXELBUFFER_COUNT);
    if (status != VCOS_SUCCESS) {
        printf("Failed to create balltrack semaphore %d\n", status);
        return -1;
    }

    status = vcos_thread_create(&analysis_thread_handle, "analysis-thread", NULL, analysis_thread, 0);
    if (status != VCOS_SUCCESS) {
        printf("Failed to start balltrack analysis thread %d\n", status);
        return -1;
    }

    allInitialized = true;
    return 0;
}

void balltrack_core_term()
{
    allInitialized = false;

    // Wait for analysis thread to finish
    analysis_stop = 1;
    vcos_thread_join(&analysis_thread_handle, NULL);
    vcos_semaphore_delete(&semFullCount);
    vcos_semaphore_delete(&semEmptyCount);

    cleanupShaders();

    for (int i = 0; i < PIXELBUFFER_COUNT; ++i) {
        if (pixelbuffers[i])
            free(pixelbuffers[i]);
    }

    GLCHK(glBindFramebufferOES(GL_FRAMEBUFFER_OES, 0));
    GLCHK(glDeleteFramebuffersOES(1, &fbo));

    for (int i = 0; i < 2; ++i) {
        if (texHueFilter[i])
            delete texHueFilter[i];
        if (texDownscaled[i])
            delete texDownscaled[i];
        texHueFilter[i] = 0;
        texDownscaled[i] = 0;
    }
    if (texHueFilterField)
        delete texHueFilterField;
    if (texDownscaledField)
        delete texDownscaledField;
    texHueFilterField = 0;
    texDownscaledField = 0;

#ifdef DO_DIFF
    if (rtt_copytex)
        delete rtt_copytex;
#endif

    GLCHK(glDeleteBuffers(1, &quad_vbo));
    return;
}


void* analysis_thread(void *arg)
{
    printf("Balltrack analysis thread started.\n");
    while (analysis_stop == 0) {
        // Wait for GL thread till there is a full buffer
        vcos_semaphore_wait(&semFullCount);

        // Get the buffer
        auto type = pixelbufferType[nextFullBuffer];
        uint8_t* buffer = pixelbuffers[nextFullBuffer];
        ++nextFullBuffer;
        if (nextFullBuffer == PIXELBUFFER_COUNT)
            nextFullBuffer = 0;

        // Process the buffer
        if (type == BUFFERTYPE_BALL)
            analysis_process_ball_buffer(buffer, 4 * width2, height2);
        else
            analysis_process_field_buffer(buffer, 4 * width2, height2);

        // Notify GL thread that a buffer is available
        vcos_semaphore_post(&semEmptyCount);
    }
    printf("Balltrack analysis thread ending.\n");
    return 0;
}

// Readout the buffer and send it to the analysis thread
void send_buffer_to_analysis(PixelBufferType buffertype, ReadoutTexture* tex) {
    int width = width2;
    int height = height2;
    uint8_t* buf = 0;

    // Wait for analysis thread for an empty buffer
    vcos_semaphore_wait(&semEmptyCount);

    // Claim it
    buf = pixelbuffers[nextEmptyBuffer];
    pixelbufferType[nextEmptyBuffer] = buffertype;
    ++nextEmptyBuffer;
    if (nextEmptyBuffer == PIXELBUFFER_COUNT)
        nextEmptyBuffer = 0;

#ifdef USE_VCSM
    // Not needed anymore, since we read the texture that was written in the previous frame
    //GLCHK(glFinish());

    // Make the buffer CPU addressable with host cache enabled
    VCSM_CACHE_TYPE_T cache_type;
    uint8_t* vcsm_buffer = (uint8_t*)vcsm_lock_cache(tex->vcsm_info.vcsm_handle, VCSM_CACHE_TYPE_HOST, &cache_type);
    if (!vcsm_buffer) {
        printf("ERROR: Failed to lock VCSM buffer.\n");
    } else {
        // Copy it into the available buffer
        // vcsm_buffer has size tex.potWidth x tex.potHeight
        // so we have to take that into account
        uint8_t* src = vcsm_buffer;
        uint8_t* dst = buf;
        for (int y = 0; y < tex->height; ++y) {
            memcpy(dst, src, 4 * tex->width);
            src += 4 * tex->potWidth;
            dst += 4 * tex->width;
        }

        // Release the locked texture memory to flush the CPU cache and allow GPU to use it
        vcsm_unlock_ptr(vcsm_buffer);
    }
#else
    GLCHK(glBindFramebufferOES(GL_FRAMEBUFFER_OES, fbo));
    GLCHK(glFramebufferTexture2DOES(GL_FRAMEBUFFER_OES, GL_COLOR_ATTACHMENT0_OES, GL_TEXTURE_2D, tex->id, 0));
    GLCHK(glViewport(0, 0, tex->width, tex->height));
    GLCHK(glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, buf));
#endif

    // Notify analysis thread
    vcos_semaphore_post(&semFullCount);
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

    ShaderProgram* shader = &shader_fixedcolor;
    GLCHK(glUseProgram(shader->program));
    GLCHK(glUniform4f(shader->uniforms[0].location, r, g, b, a));

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

// If target.id is zero, then target is the screen
int render_pass(ShaderProgram* shader, Texture* source, Texture* target) {
    GLCHK(glUseProgram(shader->program));
    if (target->id) {
        // Enable Render-to-texture and set the output texture
        GLCHK(glBindFramebufferOES(GL_FRAMEBUFFER_OES, fbo));
        GLCHK(glFramebufferTexture2DOES(GL_FRAMEBUFFER_OES, GL_COLOR_ATTACHMENT0_OES, GL_TEXTURE_2D, target->id, 0));
        GLCHK(glViewport(0, 0, target->width, target->height));
        // According to the open source GL driver for the VC4 chip,
        // [ https://github.com/anholt/mesa/wiki/VC4-Performance-Tricks ],
        // it is faster to clear the buffer even when writing to the complete screen
        glClear(GL_COLOR_BUFFER_BIT);
    } else {
        // Unset frame buffer object. Now draw to screen
        GLCHK(glBindFramebufferOES(GL_FRAMEBUFFER_OES, 0));
        GLCHK(glViewport(0, 0, target->width, target->height));
        glClear(GL_COLOR_BUFFER_BIT); // See above comment
    }
    // Bind the input texture
    GLCHK(glActiveTexture(GL_TEXTURE0));
    GLCHK(glBindTexture(source->type, source->id));
    // Bind the vertex buffer
    GLCHK(glBindBuffer(GL_ARRAY_BUFFER, quad_vbo));
    GLCHK(glEnableVertexAttribArray(shader->attribute_locations[0]));
    GLCHK(glVertexAttribPointer(shader->attribute_locations[0], 2, GL_FLOAT, GL_FALSE, 0, 0));
    // Draw
    GLCHK(glDrawArrays(GL_TRIANGLES, 0, 6));
    return 0;
}

template <typename T>
void swap(T& a, T& b) {
    T& tmp = a;
    a = b;
    b = tmp;
}

int balltrack_core_process_image(int width, int height, GLuint srctex, GLuint srctype)
{
    if (!allInitialized)
        return -1;

    static int frameNumber = -5;
    ++frameNumber;

    // Width,height is the size of the preview window on screen
    auto input = TextureWrapper(srctex, 0, 0, srctype);
    auto screen = TextureWrapper(0, width, height, 0);

#ifdef DO_FRAMEDUMPS
    if (frameNumber == 50 || frameNumber == 60 || frameNumber == 70) {
        render_pass(&shader_simple, &input, texFramedump);
        char filename[128];
        sprintf(filename, "framedump_%d.tga", frameNumber);
        dump_frame(texFramedump->width, texFramedump->height, filename);
        printf("Frame dumped to %s\n", filename);
    }
#endif

#ifdef DO_DIFF
    // Diff with previous
    GLCHK(glActiveTexture(GL_TEXTURE1));
    GLCHK(glBindTexture(GL_TEXTURE_2D, rtt_copytex));
    render_pass(&shader_diff, &input, &screen);

    // Copy source to previous
    render_pass(&shader_simple, &input, rtt_copytex);
#endif

    // Every X steps, we update the size of the green field bounding box
    static int fieldUpdateSteps = FieldUpdateDelay; // Countdown
    if (fieldUpdateSteps == 2) {
        render_pass(&shader_huefilter_field, &input, texHueFilterField);
    } else if(fieldUpdateSteps == 1) {
        render_pass(&shader_downsample, texHueFilterField, texDownscaledField);
    } else if(fieldUpdateSteps == 0) {
        send_buffer_to_analysis(BUFFERTYPE_FIELD, texDownscaledField);
        fieldUpdateSteps = FieldUpdateDelay;
    }
    --fieldUpdateSteps;

    // The read texture of last frame now becomes the write texture
    // and vice versa
    swap(texHueFilter_write, texHueFilter_read);
    swap(texDownscaled_write, texDownscaled_read);

    // Ball color filter, downsample, and readout in parallel
    render_pass(&shader_huefilter_ball, &input, texHueFilter_write);
    render_pass(&shader_downsample, texHueFilter_read, texDownscaled_write);
    if (frameNumber >= 0) // The first 3 frames there is no valid buffer yet
        send_buffer_to_analysis(BUFFERTYPE_BALL, texDownscaled_read);

    // Last render pass: render to screen
#ifdef DEBUG_TEXTURES
    GLCHK(glActiveTexture(GL_TEXTURE1));
    GLCHK(glBindTexture(GL_TEXTURE_2D, texHueFilter_read->id));
    GLCHK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
    GLCHK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));

    GLCHK(glActiveTexture(GL_TEXTURE2));
    GLCHK(glBindTexture(GL_TEXTURE_2D, texDownscaled_read->id));
    GLCHK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
    GLCHK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));

    GLCHK(glActiveTexture(GL_TEXTURE3));
    GLCHK(glBindTexture(GL_TEXTURE_2D, texDownscaledField->id));
    GLCHK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
    GLCHK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));

    render_pass(&shader_debug, &input, &screen);

    // Set scaling back to linear
    GLCHK(glActiveTexture(GL_TEXTURE1));
    GLCHK(glBindTexture(GL_TEXTURE_2D, texHueFilter_read->id));
    GLCHK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    GLCHK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    GLCHK(glActiveTexture(GL_TEXTURE2));
    GLCHK(glBindTexture(GL_TEXTURE_2D, texDownscaled_read->id));
    GLCHK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    GLCHK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    GLCHK(glActiveTexture(GL_TEXTURE3));
    GLCHK(glBindTexture(GL_TEXTURE_2D, texDownscaledField->id));
    GLCHK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    GLCHK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
#else
    render_pass(&shader_simple, &input, &screen);
#endif

    // Draw field and ball positions on top
    // TODO: This is currently quite slow
    analysis_draw();

    return 0;
}

