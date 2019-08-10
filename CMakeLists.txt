cmake_minimum_required(VERSION 3.1)
project(raspiballs)

#set(THREADS_PREFER_PTHREAD_FLAG ON)
#find_package(Threads REQUIRED)

SET(COMPILE_DEFINITIONS -Werror)
# Set --no-as-needed to stop the linker discarding mmal_vc_client
# as it can't see that the constructor registers a load of functionality
# with the MMAL core.
SET( CMAKE_EXE_LINKER_FLAGS "-Wl,--no-as-needed" )

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-multichar -Wall -Wno-unused-but-set-variable -fPIC")
add_definitions(-D_REENTRANT)
add_definitions(-DUSE_VCHIQ_ARM -DVCHI_BULK_ALIGN=1 -DVCHI_BULK_GRANULARITY=1)
add_definitions(-DOMX_SKIP64BIT)
add_definitions(-DEGL_SERVER_DISPMANX)
add_definitions(-D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64)
add_definitions(-D_GNU_SOURCE)

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
    src/RaspiCommonSettings.c
    src/RaspiHelpers.c
    #src/RaspiGPS.c
    #src/libgps_loader.c
    src/RaspiVidBalls.c
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
    mmal_core
    mmal_util
    mmal_vc_client
    vcos
    bcm_host
    brcmGLESv2
    brcmEGL
    m
    #Threads::Threads
    pthread
    dl
    rt
    )

target_link_libraries(raspiballs ${LIBS})

