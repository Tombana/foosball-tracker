#extension GL_OES_EGL_image_external : require
uniform samplerExternalOES tex_rgb;
uniform samplerExternalOES tex_y;
uniform samplerExternalOES tex_u;
uniform samplerExternalOES tex_v;
varying vec2 texcoord;

// Transpose of actual matrix
mat4 YUVtoRGB = mat4( 1.164,  1.164, 1.164, 0,
                          0, -0.391, 2.018, 0,
                      1.596, -0.813,     0, 0,
                     -0.871,  0.529,-1.082, 1);

void main(void) {
    vec4 col   = texture2D(tex_rgb, texcoord);
    vec4 col_y = texture2D(tex_y, texcoord);
    vec4 col_u = texture2D(tex_u, texcoord);
    vec4 col_v = texture2D(tex_v, texcoord);
    vec4 yuv = vec4(col_y.r, col_u.r, col_v.r, 1.0);
    vec4 rgb = YUVtoRGB * yuv;
    rgb = clamp(rgb, 0.0, 1.0);
    //float B = clamp(1.164 * (Y - 0.0625) + 2.018 * (U - 0.5)  , 0.0,1.0);
    //float G = clamp(1.164 * (Y - 0.0625) - 0.813 * (V - 0.5) - 0.391 * (U - 0.5) , 0.0,1.0);
    //float R = clamp(1.164 * (Y - 0.0625) + 1.596 * (V - 0.5)  , 0.0,1.0);
    //gl_FragColor = vec4(abs(R - col.r), abs(G - col.g), abs(B - col.b), 1.0);
    //gl_FragColor = rgb;
    gl_FragColor = col;
}


