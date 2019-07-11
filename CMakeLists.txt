cmake_minimum_required(VERSION 3.1)
project(raspiballs)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

SET(COMPILE_DEFINITIONS -Werror)

include_directories(${PROJECT_SOURCE_DIR}/src)

#
# In case you want to get the libs and headers directly from the `userland` repository
# as a subdirectory of this repository.
#
#SET(USERLAND_DIR ${PROJECT_SOURCE_DIR}/userland)
#include_directories(${USERLAND_DIR}) # For interface and vcinclude
#include_directories(${USERLAND_DIR}/host_applications/linux/libs/bcm_host/include)
#include_directories(${USERLAND_DIR}/interface/khronos/include)
#link_directories(${PROJECT_SOURCE_DIR}/userland/build/lib)

include_directories(/opt/vc/include)
link_directories(/opt/vc/lib)

set (SOURCES
    src/RaspiCamControl.c
    src/RaspiCLI.c
    src/RaspiPreview.c
    src/RaspiBalls.c
    src/RaspiTexBalls.c
    src/RaspiTexUtil.c
    src/tga.c
    src/gl_scenes/balltrack.c
    src/balltrackshaders/allshaders.h
    src/BalltrackCore.c
    src/BalltrackUtil.c
    src/BallAnalysis.c
)

add_custom_command(OUTPUT src/balltrackshaders/allshaders.h
                   COMMAND make allshaders.h
                   WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src/balltrackshaders
)

add_executable(raspiballs ${SOURCES})

set (LIBS
    Threads::Threads
    mmal_core
    mmal_util
    mmal_vc_client
    vcos
    bcm_host
    brcmGLESv2
    brcmEGL
    m
    )

target_link_libraries(raspiballs ${LIBS})

