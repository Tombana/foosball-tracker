/*
Copyright (c) 2013, Broadcom Europe Ltd
Copyright (c) 2013, Tim Gover
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

#include "../tracker/BalltrackCore.h"
#include "RaspiTex.h"
#include "RaspiTexUtil.h"
#include <GLES2/gl2.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>


static const EGLint balltrack_egl_config_attribs[] =
{
   EGL_RED_SIZE,   8,
   EGL_GREEN_SIZE, 8,
   EGL_BLUE_SIZE,  8,
   EGL_ALPHA_SIZE, 8,
   EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
   EGL_NONE
};

/**
 * Creates the OpenGL ES 2.X context and builds the shaders.
 * @param raspitex_state A pointer to the GL preview state.
 * @return Zero if successful.
 */
static int balltrack_init(RASPITEX_STATE *raspitex_state)
{
    vcos_log_trace("%s", VCOS_FUNCTION);
    raspitex_state->egl_config_attribs = balltrack_egl_config_attribs;
    int rc = raspitexutil_gl_init_2_0(raspitex_state);
    if (rc != 0)
        return rc;

    return balltrack_core_init(1, 0);
}

/* Redraws the scene with the latest luma buffer.
 *
 * @param raspitex_state A pointer to the GL preview state.
 * @return Zero if successful.
 */
static int balltrack_redraw(RASPITEX_STATE* state)
{
#ifdef DO_YUV
    GLCHK(glActiveTexture(GL_TEXTURE2));
    GLCHK(glBindTexture(GL_TEXTURE_EXTERNAL_OES, state->y_texture));
    GLCHK(glActiveTexture(GL_TEXTURE3));
    GLCHK(glBindTexture(GL_TEXTURE_EXTERNAL_OES, state->u_texture));
    GLCHK(glActiveTexture(GL_TEXTURE4));
    GLCHK(glBindTexture(GL_TEXTURE_EXTERNAL_OES, state->v_texture));
#endif
    return balltrack_core_process_image(state->width, state->height, state->texture, GL_TEXTURE_EXTERNAL_OES);
}

int balltrack_open(RASPITEX_STATE *state)
{
   state->ops.gl_init = balltrack_init;
   state->ops.redraw = balltrack_redraw;
#ifdef DO_YUV
   state->ops.update_y_texture = raspitexutil_update_y_texture;
   state->ops.update_u_texture = raspitexutil_update_u_texture;
   state->ops.update_v_texture = raspitexutil_update_v_texture;
#endif
   state->ops.update_texture = raspitexutil_update_texture;
   return 0;
}

