#extension GL_OES_EGL_image_external : require

// See huefilterball.frag for more explanation

float getFilter(vec4 col) {
    // We use a piecewise definition for Hue.
    // We only compute one of the three parts.
    // The `red piece' lies in [-1,1]. The green in [1,3]. The blue in [3,5].
    // Multiply by 60 to get degrees.
    float value = max(col.r, max(col.g, col.b));
    float chroma= value - min(col.r, min(col.g, col.b));
    float sat = (value > 0.0 ? (chroma / value) : 0.0); 
    float greenfilter = 0.0;
    if (col.g == value) {
        float hue = (col.b - col.r) / chroma;
        if (hue > 0.0 && hue < 0.9 && sat > 0.15 && value > 0.10 && value < 0.70 ) {
           greenfilter = 1.0;
        }
    }
    return greenfilter;
}

uniform samplerExternalOES tex;
uniform vec2 tex_unit;
varying vec2 texcoord;
void main(void) {
    vec4 col1 = texture2D(tex, texcoord - vec2(3,0) * tex_unit);
    vec4 col2 = texture2D(tex, texcoord - vec2(1,0) * tex_unit);
    vec4 col3 = texture2D(tex, texcoord + vec2(1,0) * tex_unit);
    vec4 col4 = texture2D(tex, texcoord + vec2(3,0) * tex_unit);
    gl_FragColor.r = getFilter(col1);
    gl_FragColor.g = getFilter(col2);
    gl_FragColor.b = getFilter(col3);
    gl_FragColor.a = getFilter(col4);
}

