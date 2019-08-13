/*
Copyright (c) 2018, Tom Bannink
MIT License
*/

#ifndef BALLTRACKCORE_H
#define BALLTRACKCORE_H

#include <GLES/gl.h>

#ifdef __cplusplus
extern "C" {
#endif

//
// Initialize balltrack shaders
//
// @param externalSamplerExtension
//      Set to 1 if the source texture comes from the camera,
//      so that the shaders use `samplerExternalOES`
//      Set to 0 if otherwise, so that the shaders use `sampler2D`
//
// @param flipY whether to flip the y coordinate
//
int balltrack_core_init(int externalSamplerExtension, int flipY);

//
// Process an image
//
int balltrack_core_process_image(int width, int height, GLuint srctex, GLuint srctype);

#ifdef __cplusplus
}
#endif

#endif /* BALLTRACKCORE_H */
