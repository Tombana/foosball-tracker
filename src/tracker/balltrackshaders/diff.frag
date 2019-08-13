#extension GL_OES_EGL_image_external : require
uniform samplerExternalOES tex1;
uniform sampler2D tex2;
varying vec2 texcoord;

void main(void) {
    vec4 col1 = texture2D(tex1, texcoord);
    vec4 col2 = texture2D(tex2, texcoord);
    vec4 diff = col1 - col2;
    float dist = dot(diff, diff);
    if (dist > 0.2) {
        gl_FragColor = vec4(1.0, 1.0, 0, 1.0);
    } else {
        gl_FragColor = 0.2 * col1;
    }
    gl_FragColor.a = 1.0;
}

