#extension GL_OES_EGL_image_external : require
uniform samplerExternalOES tex;
varying vec2 texcoord;
void main(void) {
    gl_FragColor = texture2D(tex, texcoord);
}
