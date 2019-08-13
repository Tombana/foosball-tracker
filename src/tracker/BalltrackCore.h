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

int balltrack_core_init(int externalSamplerExtension, int flipY);
int balltrack_core_redraw(int width, int height, GLuint srctex, GLuint srctype);

#ifdef __cplusplus
}
#endif

#endif /* BALLTRACKCORE_H */
