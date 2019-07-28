# Raspberry Pi balltracker

This repository contains the code for running the balltracking software on the Raspberry Pi GPU.
Only tested on 32 bit OS, the libraries will probably not work on 64 bit.

## Prerequisites

This code requires `cmake`, `make`, `gcc` and `xxd` to be installed. The program `xxd` can be found in the `vim` package.

On a Raspberry Pi, the correct libraries should already be present in `/opt/vc` so there is no need to do anything.
If the files are not present, then you can build and install them yourself as follows (from a separate directory).

    git clone https://github.com/raspberrypi/userland
    cd userland
    ./buildme

It might ask for a sudo password after building, in order to install the files into `/opt/vc`.

## Compiling

Use the `buildme` script to build

## Running

To run without recording

    build/raspiballs -w 1280 -h 720 -fps 40 -t 0 -g 10 --ev 5 --glwin 450,700,640,480

To record video (in fragments)

    mkdir -p "/dev/shm/replay/fragments"
    build/raspiballs -o /dev/shm/replay/fragments/out%04d.h264 -w 1280 -h 720 -fps 40 -t 0  -sg 100 -wr 100 -g 10 --ev 5 --glwin 450,700,640,480


## Branching from newer userland repository

In the userland repository, go the directory `host_applications/linux/apps/raspicam`.
In `RaspiVid.c` is the required code to record video. In `RaspiStill.c` and `RaspiTex*` there is the required code to use the camera together with OpenGL.
We require both recording and OpenGL, so we have to merge those sources and the easiest way is to use `RaspiVid.c` as a base and include some `raspitex` calls in there.
We furthermore have to make some changes to get from `RaspiTex.c` to `RaspiTexBalls.c`.

### Changes from `RaspiVid.c` to `RaspiVidBalls.c`

Somewhere at the start, add an include:

    #include "RaspiTex.h"

In `struct RASPIVID_STATE_S { ... }` add:

    RASPITEX_STATE raspitex_state;

In `static void default_status(RASPIVID_STATE *state)` at the very end of the function add:

    raspitex_set_defaults(&state->raspitex_state);
    state->preview_parameters.previewWindow.width = 640;
    state->preview_parameters.previewWindow.height = 360;
    state->raspitex_state.width = 640;
    state->raspitex_state.height = 360;

In `static int parse_cmdline(int argc, const char **argv, RASPIVID_STATE *state)` under `parms_used = raspipreview_parse_cmdline(&state->preview_parameters, &argv[i][1], second_arg);` add:

    if (!parms_used)
        parms_used = raspitex_parse_cmdline(&state->raspitex_state, &argv[i][1], second_arg);

and at the very end of that same function, before return, add:

    if (! state->raspitex_state.gl_win_defined)
    {
       state->raspitex_state.x       = state->preview_parameters.previewWindow.x;
       state->raspitex_state.y       = state->preview_parameters.previewWindow.y;
       state->raspitex_state.width   = state->preview_parameters.previewWindow.width;
       state->raspitex_state.height  = state->preview_parameters.previewWindow.height;
    }
    state->raspitex_state.preview_x       = state->preview_parameters.previewWindow.x;
    state->raspitex_state.preview_y       = state->preview_parameters.previewWindow.y;
    state->raspitex_state.preview_width   = state->preview_parameters.previewWindow.width;
    state->raspitex_state.preview_height  = state->preview_parameters.previewWindow.height;
    state->raspitex_state.opacity         = state->preview_parameters.opacity;
    state->raspitex_state.verbose         = state->verbose;

In `static void application_help_message(char *app_name)` under `raspicli_display_help(cmdline_commands, cmdline_commands_size);` add:

    raspitex_display_help();

In `static MMAL_STATUS_T create_camera_component(RASPIVID_STATE *state)` after `raspicamcontrol_set_all_parameters(camera, &state->camera_parameters);` and before `state->camera_component = camera;`,
add:

    status = raspitex_configure_preview_port(&state->raspitex_state, preview_port);
    if (status != MMAL_SUCCESS)
    {
        fprintf(stderr, "Failed to configure preview port for GL rendering");
        goto error;
    }

In `int main(int argc, const char **argv)`, after the `if (state.common_settings.gps) { ... }` block, right before `if ((status = create_camera_component(&state)) != MMAL_SUCCESS)`, add:

    raspitex_init(&state.raspitex_state);

and a few lines lower, comment out the block `else if ((status = raspipreview_create(&state.preview_parameters)) != MMAL_SUCCESS) { ... }`.

    //else if ((status = raspipreview_create(&state.preview_parameters)) != MMAL_SUCCESS)
    //{
    //   vcos_log_error("%s: Failed to create preview component", __func__);
    //   destroy_camera_component(&state);
    //   exit_code = EX_SOFTWARE;
    //}

A few lines lower, comment out the line:

    //preview_input_port  = state.preview_parameters.preview_component->input[0];

A few lines lower there is `if (state.preview_parameters.wantPreview ) { ... } else { ... }`. Comment out the _whole_ if-else block, both the if and else paths. Both these blocks correspond to the `if (! state.useGL) { ... }` block that is in `RaspiStill.c` at that same location. The if-else block will connect the `preview_port` but now we used the `raspitex_configure_preview_port` instead, and therefore we don't need it.

    //if (state.preview_parameters.wantPreview )
    //{
    //    if (state.raw_output)
    //    ...
    //    ...
    //}
    //else
    //{
    //    if (state.raw_output)
    //    ...
    //    ...
    //}

In the same function, quite a bit lower, there is `status = mmal_port_enable(encoder_output_port, encoder_buffer_callback);`. After that, but before `if (state.demoMode)`, add:
 
    if (raspitex_start(&state.raspitex_state) != 0)
        goto error;

In the same function, near the end, there is `fprintf(stderr, "Closing down\n");`. After that, before `check_disable_port(camera_still_port);`, add:

      raspitex_stop(&state.raspitex_state);
      raspitex_destroy(&state.raspitex_state);

### Changes from `RaspiTex.c` to `RaspiTexBalls.c`

The main edits are:

1. Fix scene id to balltrack scene
2. Limit drawing framerate to source framerate. In `preview_process_returned_bufs` only call `raspitex_draw` when a new buffer is available from the camera source.

Remove all the `gl_scenes/*` includes and replace them by only the `balltrack.h` file:

    //#include "gl_scenes/mirror.h"
    //#include "gl_scenes/sobel.h"
    //#include "gl_scenes/square.h"
    //#include "gl_scenes/teapot.h"
    //#include "gl_scenes/vcsm_square.h"
    //#include "gl_scenes/yuv.h"
    #include "gl_scenes/balltrack.h"

