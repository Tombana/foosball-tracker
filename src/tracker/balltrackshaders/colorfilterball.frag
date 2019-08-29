// The output pixel coordinate (=texcoord) is always the center of an output pixel.

// Balltrack shader first phase
// To pack to four values into an RGBA we have in width-direction:
// tex_unit:0  1  2  3  4  5  6  7  8
// Input:   |--|--|--|--|--|--|--|--|  each is size tex_unit
// texcoord:|-----------*-----------|
// samples: |--*--|--*--|--*--|--*--|
// Output:     R     G     B     A
// For the height direction it is simpler:
// tex_unit:0  1  2
// Input:   |--|--|
// texcoord:|--*--|
// samples: |--*--|
#extension GL_OES_EGL_image_external : require

//mat4 weights0 = mat4(
//        -0.6283, 2.1687,-0.6527,-0.8721,  // column 1
//        -0.6121, 3.9540,-2.5657,-2.5035,  // column 2
//        -0.5640,-4.6584, 4.7184, 4.5599,  // column 3
//        -0.0000,-0.5383, 1.3320, 1.2500); // last column (biases)
//vec4 weights1 = vec4( 1.1078,33.6169,-8.0648,-8.7316);
//float b0 = -2.9163;

float getFilter(vec4 col) {
    // We use a piecewise definition for Hue.
    // We only compute one of the three parts.
    // The `red piece' lies in [-1,1]. The green in [1,3]. The blue in [3,5].
    // Multiply by 60 to get degrees.
    float value = max(col.r, max(col.g, col.b));
    float chroma= value - min(col.r, min(col.g, col.b));
    float sat = (value > 0.0 ? (chroma / value) : 0.0); 
    float ballfilter = 0.0; // 0.8;
    if (col.r == value) {
        if (sat > 0.35 && value > 0.15 && value < 0.95 ) {
            float hue = (col.g - col.b) / chroma;
            // Hue upper bound of 1.0 is automatic.
            if (hue > 0.70) {
                ballfilter = 1.0;
            }
            //else if (hue < 0.30) {
            //    ballfilter = 0.0;
            //}
        }
    }
    return ballfilter;
    // two-layer neural network
    //col[3] = 1.0; // bias (alpha) component
    //// max is ReLu
    //float neuron = b0 + dot(weights1, max(vec4(0.0), weights0 * col));
    //if (neuron > 0.0)
    //    return 1.0;
    //else
    //    return 0.0;
}

uniform samplerExternalOES tex;
uniform vec2 tex_unit;
varying vec2 texcoord;
void main(void) {
    vec4 col1 = texture2D(tex, texcoord - vec2(3,0) * tex_unit);
    vec4 col2 = texture2D(tex, texcoord - vec2(1,0) * tex_unit);
    vec4 col3 = texture2D(tex, texcoord + vec2(1,0) * tex_unit);
    vec4 col4 = texture2D(tex, texcoord + vec2(3,0) * tex_unit);
    gl_FragColor[0] = getFilter(col1);
    gl_FragColor[1] = getFilter(col2);
    gl_FragColor[2] = getFilter(col3);
    gl_FragColor[3] = getFilter(col4);
}

// The below version samples the source in the center of each pixel,
// then applies the Hue filter, and *then* takes the average.
//void main(void) {
//    int x = -7;
//    for (int i = 0; i < 4; ++i) {
//        float f = 0.0;
//        f += getFilter(texture2D(tex, texcoord + vec2(x,-1) * 0.5 * tex_unit));
//        f += getFilter(texture2D(tex, texcoord + vec2(x, 1) * 0.5 * tex_unit));
//        x += 2;
//        f += getFilter(texture2D(tex, texcoord + vec2(x,-1) * 0.5 * tex_unit));
//        f += getFilter(texture2D(tex, texcoord + vec2(x, 1) * 0.5 * tex_unit));
//        x += 2;
//        gl_FragColor[i] = f / 4.0;
//    }
//}
