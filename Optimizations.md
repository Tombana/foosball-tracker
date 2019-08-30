
# Benchmarks and possible optimizations

## FBO switching

In order to render to a texture, one needs to create a FrameBuffer Object (FBO) and *attach* a texture to it.
One can then *bind* the framebuffer to render to the texture.

To render to different textures, as we do, there are two options

- Use a single FBO and *attach* a different texture every time
- Have a separate FBO for each texture which always remains attached, and *bind* a different FBO every time

Currently we use the first method. I have not tested the second method yet.

According to the internet, one method can be a lot faster or slower than the other but this depends very heavily on the hardware and driver.
For embedded systems, like the Raspberry Pi, things are different than for desktop GPUs.
For example, they say that the driver can store all of the rendering tasks in an FBO object but delay the actual work until later. When you then bind a different framebuffer, you force a flush and have to wait for the GPU to finish all the work instead of it being done in the background. Therefore binding a different FBO could be slower. *However*, in our case we want to read out the resulting texture, so we have to flush anyway. Maybe therefore doesnt matter in our case.
We should benchmark which of these two options is faster (taking into account the next section on flushing).

## Parallel textures and flushing OpenGL

Reading out the result of a render-to-texture operation requires a flush of all the OpenGL GPU operations.
It can therefore be better to have two textures, and alternate between them.
Instead of doing this in series:

    source -> texture1
              texture1 -> texture2
                          texture2 -> readout

We can do this in parallel

    source    -> texture1a
    texture1b -> texture2a
    texture2b -> readout

and swap a/b every frame, so that none of the three operations depends on another operation in that frame.
I call this method *parallel textures*.
Note: after every frame, we call `eglSwapBuffers`, but this does apparently *not* finish all OpenGL operations,
because it can happen that even *two* frames after we request a render to a texture, it is still empty!

## VideoCore Shared Memory

Reading out textures can be done with either VCSM (VideoCore Shared Memory) or GLRP (glReadPixels).

For VCSM, we allocate a 'shared memory' texture that can be accessed by the both CPU and GPU. We have to call glFinish first to flush all GL operations.
For GLRP, we use a normal (gpu-based) texture, and then use glReadPixels to get the result. Note that glReadPixels will flush GL internally.

## VCSM vs GLRP simple benchmark

Comparing the performance of VCSM with GLRP.

Small texture:
20 (width) x 45 (height) RGBA pixels, which represents an 80x45 normal image.
Since VCSM requires power-of-two sizes between 64 and 2048, it is a 64 x 64 texture of which we only use the 20x45 part.

Big texture:
40 (width) x 90 (height) RGBA pixels, representing a 160x90 image
The VCSM allocated texture is 64x128

|       Framerates       | Small texture | Big texture |
|:----------------------:|:-------------:|:-----------:|
| VCSM                   |            39 |          34 |
| GLRP                   |            44 |        37.5 |
| VCSM no readout        |            46 |        44.5 |
| GLRP no readout        |            48 |        46.2 |
| VCSM parallel textures |            46 |          45 |
| GLRP parallel textures |            42 |          37 |

**Observations**

- The `no readout` results are without parallel textures. They write to the textures in series but do not readout the result to the CPU. This shows that writing without reading is faster to normal, non-VCSM textures.
- For GLRP the parallel textures do not help at all. They do matter a lot for VCSM textures.

