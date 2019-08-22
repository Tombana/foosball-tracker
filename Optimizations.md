
# Benchmarks and possible optimizations

## FBO switching

In order to render to a texture, one needs to create a FrameBuffer Object (FBO) and *attach* a texture to it.
One can then *bind* the framebuffer to render to the texture.

To render to different textures, as we do, there are two options

- Use a single FBO and *attach* a different texture every time
- Have a separate FBO for each texture which always remains attached, and *bind* a different FBO every time

Currently we use the first method.

According to the internet, one method can be a lot faster or slower than the other but this depends very heavily on the hardware and driver.
For embedded systems, like the Raspberry Pi, things are different than for desktop GPUs.
For example, they say that the driver can store all of the rendering tasks in an FBO object but delay the actual work until later. When you then bind a different framebuffer, you force a flush and have to wait for the GPU to finish all the work instead of it being done in the background. Therefore binding a different FBO could be slower. *However*, in our case we want to read out the resulting texture, so we have to flush anyway. Maybe therefore doesnt matter in our case.
We should benchmark which of these two options is faster (taking into account the next section on flushing).

## Flushing OpenGL

Reading out the result of a render-to-texture operation requires a flush of all the OpenGL GPU operations.
It might therefore be better to have two (or even more) textures, and alternate between them.
At the start of one frame, one can read out the result of the texture used in the previous frame. This way there is no time wasted waiting for the GPU.

## VideoCore Shared Memory

Reading out textures can be done with either VCSM (VideoCore Shared Memory) or GLRP (glReadPixels).

For VCSM, we allocate a 'shared memory' texture that can be accessed by the both CPU and GPU. We have to call glFinish first to flush all GL operations.
For GLRP, we use a normal (gpu-based) texture, and then use glReadPixels to get the result. Note that glReadPixels will flush GL internally.

## VCSM simple benchmark

Comparing the performance of VCSM with GLRP.

The size of the texture in question is
20 (width) x 45 (height) RGBA pixels, which represents an 80x45 normal image.
Since VCSM requires power-of-two sizes between 64 and 2048, it is a 64 x 64 texture of which we only use the 20x45 part.

- GLRP: 44 FPS
- VCSM: 39 FPS
- Writing to VCSM-type texture but not reading it: 46 FPS
- Writing to GLRP-type texture but not reading it: 48 FPS

So even though VCSM might be faster in theory, just writing to these type of textures seems to be slower already.

This benchmark is *without* the idea of alternating between two textures as described in the Flushing section above.

