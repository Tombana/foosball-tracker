cmake_minimum_required(VERSION 3.1)
project(foosballtracker)


# Comment this out for a release build
add_definitions(-DCHECK_GL_ERRORS) # Call glGetError after *every* GL call
add_definitions(-DDEBUG_TEXTURES) # Use debug shader to show intermediate phases on screen



#
# In case you want to get the libs and headers directly from the `userland` repository
# as a subdirectory of this repository.
#
#SET(USERLAND_DIR ${PROJECT_SOURCE_DIR}/userland)
#include_directories(${USERLAND_DIR}) # For interface and vcinclude
#include_directories(${USERLAND_DIR}/host_applications/linux/libs/bcm_host/include)
#include_directories(${USERLAND_DIR}/interface/khronos/include)
#link_directories(${USERLAND_DIR}/build/lib)

include_directories(/opt/vc/include)
link_directories(/opt/vc/lib)


# Set --no-as-needed to stop the linker discarding mmal_vc_client
# as it can't see that the constructor registers a load of functionality
# with the MMAL core.
SET( CMAKE_EXE_LINKER_FLAGS "-Wl,--no-as-needed" )

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Werror -Wno-multichar -Wno-unused-but-set-variable -fPIC -ftree-vectorize")

# These come from raspivid
add_definitions(-D_REENTRANT)
add_definitions(-DUSE_VCHIQ_ARM -DVCHI_BULK_ALIGN=1 -DVCHI_BULK_GRANULARITY=1)
add_definitions(-DOMX_SKIP64BIT)
add_definitions(-DEGL_SERVER_DISPMANX)
add_definitions(-D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64)
add_definitions(-D_GNU_SOURCE)

# These come from hello_videocube
add_definitions(-DSTANDALONE)
add_definitions(-D__STDC_CONSTANT_MACROS)
add_definitions(-D__STDC_LIMIT_MACROS)
add_definitions(-DTARGET_POSIX)
add_definitions(-D_LINUX)
add_definitions(-DPIC)
add_definitions(-DHAVE_LIBOPENMAX=2)
add_definitions(-DOMX)
add_definitions(-DUSE_EXTERNAL_OMX)
add_definitions(-DHAVE_LIBBCM_HOST)
add_definitions(-DUSE_EXTERNAL_LIBBCM_HOST)



set (COMMON_SOURCES
    src/tracker/tga.c
    src/tracker/balltrackshaders/allshaders.h
    src/tracker/core.cpp
    src/tracker/util.cpp
    src/tracker/analysis.cpp
)

set (SHADER_SOURCES
    src/tracker/balltrackshaders/debug.frag
    src/tracker/balltrackshaders/display.frag
    src/tracker/balltrackshaders/downsample.frag
    src/tracker/balltrackshaders/fixedcolor.frag
    src/tracker/balltrackshaders/huefilterball.frag
    src/tracker/balltrackshaders/huefilterfield.frag
    src/tracker/balltrackshaders/phase1.frag
    src/tracker/balltrackshaders/phase2.frag
    src/tracker/balltrackshaders/vshader.vert
)

set (RECORDER_SOURCES
    src/recorder/RaspiCamControl.c
    src/recorder/RaspiCLI.c
    src/recorder/RaspiPreview.c
    src/recorder/RaspiCommonSettings.c
    src/recorder/RaspiHelpers.c
    #src/recorder/RaspiGPS.c
    #src/recorder/libgps_loader.c
    src/recorder/RaspiVidBalls.c
    src/recorder/RaspiTexBalls.c
    src/recorder/RaspiTexUtil.c
    src/recorder/BalltrackScene.c
)

set (PLAYER_SOURCES
    src/player/triangle.c
    src/player/video.c
    src/player/libilclient/ilclient.c
    src/player/libilclient/ilcore.c
    )

add_custom_command(OUTPUT src/tracker/balltrackshaders/allshaders.h
                   COMMAND make allshaders.h
                   WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src/tracker/balltrackshaders
                   DEPENDS ${SHADER_SOURCES}
)

add_executable(raspiballs ${COMMON_SOURCES} ${RECORDER_SOURCES})
add_executable(videotracker ${COMMON_SOURCES} ${PLAYER_SOURCES})

set (RECORDER_LIBS
    mmal_core
    mmal_util
    mmal_vc_client
    vcos
    bcm_host
    brcmGLESv2
    brcmEGL
    m
    pthread
    dl
    rt
    )

set (PLAYER_LIBS
    openmaxil
    bcm_host
    vcos
    vchiq_arm
    brcmGLESv2
    brcmEGL
    pthread
    rt
    m
    )

target_link_libraries(raspiballs ${RECORDER_LIBS})
target_link_libraries(videotracker ${PLAYER_LIBS})

