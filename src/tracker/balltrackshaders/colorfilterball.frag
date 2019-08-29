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

mat4 weights0 = mat4(
 -34.5803,-222.2452,  -7.2026,-198.8427,  // column 1
-133.4599,   8.2426,-185.2599, -38.5274,  // column 2
 181.7046, 224.2931, 111.3470, 227.9662,  // column 3
  16.9938,  -0.9921,   1.5175,   5.4740); // last column (biases)
vec4 weights1 = vec4(42.2840, 4.9263, 1.5692,24.5940);
float b1 = -9.4811;

vec4 ReLu(vec4 x) {
    return max(vec4(0.0), x);
}

float sigmoid(float x) {
    return 1.0 / (1.0 + exp(-1.0 * x));
}

float getFilter(vec4 col) {
    // two-layer neural network
    // Assumes col[3] is equal to 1.0 for the bias to work
    float neuron = b1 + dot(weights1, ReLu(weights0 * col));
    // return sigmoid(neuron);
    // The neural network was trained with a sigmoid, but this should be faster and good enough
    if (neuron < 0.0)
        return 1.0;
    else
        return 0.0;
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
