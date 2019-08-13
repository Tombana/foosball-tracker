#extension GL_OES_EGL_image_external : require
uniform samplerExternalOES tex_camera;
uniform vec2 tex_unit;
uniform sampler2D tex_filter;
varying vec2 texcoord;
void main(void) {
    vec4 col = texture2D(tex_camera, texcoord);
    vec4 fil = texture2D(tex_filter, texcoord);
    // fil.rg is red/green filter for `left pixel'
    // fil.ba is red/green filter for `right pixel'
    float r, g;
    if ( mod(texcoord.x, tex_unit.x) < 0.5 * tex_unit.x ) {
        r = fil.r;
        g = fil.g;
    } else {
        r = fil.b;
        g = fil.a;
    }
    //if (r > 0.3) {
    //    gl_FragColor = 0.7 * col + 0.3 * vec4(1.0, 0.0, 1.0, 1.0);
    //} else if (g > 0.75) {
    //    gl_FragColor = 0.9 * col + 0.1 * vec4(0.0, 1.0, 0.0, 1.0);
    //} else {
    //    gl_FragColor = col;
    //}
    //gl_FragColor = 0.3 * col + 0.7 * vec4(r,r,r,1.0);
    gl_FragColor = col;
}
