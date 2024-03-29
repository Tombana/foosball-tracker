// Ouput is 8X smaller in both directions!
// We will use GL_LINEAR so that the GPU samples 4 texels at once.
// The center of the output pixel (=texcoord) is at the intersection
// of four input pixels.
// tex_unit is size of input texel
// In the height dimension, where we have one output:
// tex_unit:-8 -7 -6 -5 -4 -3 -2 -1  0  1  2  3  4  5  6  7  8
// Input:    |--|--|--|--|--|--|--|--|--|--|--|--|--|--|--|--|
// texcoord:             |-----------*-----------|
// samples:              |--*--|--*--|--*--|--*--|
//
// In the width dimension, where we have four *outputs*:
// tex_unit: -4   -3   -2   -1    0    1    2    3    4
// Input:     |RGBA|RGBA|RGBA|RGBA|RGBA|RGBA|RGBA|RGBA|
// texcoord:  |-------------------*-------------------|
// samples:   |----R----|----G----|----B----|----A----|

uniform sampler2D tex;
varying vec2 texcoord;
uniform vec2 tex_unit;
void main(void) {
    int x = -3;
    for (int i = 0; i < 4; ++i) {
        float avg = 0.0;
        for (int y = -3; y <= 3; y += 2) {
            vec4 v = texture2D(tex, texcoord + vec2(x, y) * tex_unit);
            avg += v[0];
            avg += v[1];
            avg += v[2];
            avg += v[3];
        }
        gl_FragColor[i] = 0.0625 * avg;
        x += 2;
    }
}
