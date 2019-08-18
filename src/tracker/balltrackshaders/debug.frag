#extension GL_OES_EGL_image_external : require
uniform samplerExternalOES tex_camera;

uniform sampler2D tex_dbg1;
uniform sampler2D tex_dbg2;
uniform sampler2D tex_dbg3;
//uniform sampler2D tex_dbg4;
uniform float pixelwidth1;
uniform float pixelwidth2;
uniform float pixelwidth3;
//uniform float pixelwidth4;

varying vec2 texcoord;


void main(void) {
    vec2 coord = 2.0 * mod(texcoord, 0.5);

    vec4 campixel = texture2D(tex_camera, coord);
    vec4 dbgpixel;
    float pixelwidth;

    if (texcoord.x < 0.5 && texcoord.y > 0.5) {
        // top-left
        dbgpixel = texture2D(tex_dbg1, coord);
        pixelwidth = pixelwidth1;
    } else if (texcoord.x > 0.5 && texcoord.y > 0.5) {
        // top-right
        dbgpixel = texture2D(tex_dbg2, coord);
        pixelwidth = pixelwidth2;
    } else if (texcoord.x < 0.5 && texcoord.y < 0.5) {
        // bottom-left
        dbgpixel = texture2D(tex_dbg3, coord);
        pixelwidth = pixelwidth3;
    } else {
        // bottom-right
        //dbgpixel = texture2D(tex_dbg4, coord);
        //pixelwidth = pixelwidth4;
        gl_FragColor = campixel;
        return;
    }

    // dbgpixel can hold 4 separate values in r,g,b,a
    // So we need to know the width of the dbg texture

    float dbgValue;
    float interpixelX = mod(coord.x, pixelwidth);
    if ( interpixelX < 0.25 * pixelwidth ) {
        dbgValue = dbgpixel.r;
    } else if (interpixelX < 0.5 * pixelwidth ) {
        dbgValue = dbgpixel.g;
    } else if (interpixelX < 0.75 * pixelwidth ) {
        dbgValue = dbgpixel.b;
    } else {
        dbgValue = dbgpixel.a;
    }

    gl_FragColor = (1.0 - dbgValue) * campixel + dbgValue * vec4(1.0, 0.0, 1.0, 1.0);
}
