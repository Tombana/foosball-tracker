// Balltrack shader second phase
// Ouput is 8X smaller in both directions!
// We will use GL_LINEAR so that the GPU samples 4 texels at once.
// The center of the output pixel (=texcoord) is at the intersection
// of four input pixels.
// tex_unit is size of input texel
// NOTE: Trying to sample 16x16 pixels yields an out-of-memory error! 12x12 still works!
// In the height dimension, where we have one output:
// tex_unit:-8 -7 -6 -5 -4 -3 -2 -1  0  1  2  3  4  5  6  7  8
// Input:    |--|--|--|--|--|--|--|--|--|--|--|--|--|--|--|--|
// texcoord:             |-----------*-----------|
// samples1:             |--*--|--*--|--*--|--*--|
// samples2:       |--*--|--*--|--*--|--*--|--*--|--*--|
//
// In the width dimension, where we have two *outputs*:
// tex_unit:-6    -5    -4    -3    -2    -1     0     1     2     3     4     5     6
// Input:    |RG|BA|RG|BA|RG|BA|RG|BA|RG|BA|RG|BA|RG|BA|RG|BA|RG|BA|RG|BA|RG|BA|RG|BA|
// texcoord:             |-----------------------*-----------------------|
// samples1:             |-----*-----|-----*-----|-----*-----|-----*-----|
// samples2:       |-----*-----|-----*-----|-----*-----|-----*-----|-----*-----|
// Output:         |<--             R G             -->|
// Output:                                 |<--             B A             -->|
// One of the x-sample points is used in both results!
// OOM:      |-----*-----|-----*-----|-----*-----|-----*-----|-----*-----|-----*-----| (out-of-memory)
uniform sampler2D tex;
varying vec2 texcoord;
uniform vec2 tex_unit;
void main(void) {
    vec4 avg1 = vec4(0,0,0,0);
    vec4 avg2 = vec4(0,0,0,0);
    vec4 avgboth= vec4(0,0,0,0);
    for (int j = -5; j <= 5; j += 2) {
        avgboth += texture2D(tex, texcoord + vec2(0,j) * tex_unit);
    }
    for (int i = -4; i <=-2; i += 2) {
        for (int j = -5; j <= 5; j += 2) {
            avg1 += texture2D(tex, texcoord + vec2(i,j) * tex_unit);
        }
    }
    for (int i =  2; i <= 4; i += 2) {
        for (int j = -5; j <= 5; j += 2) {
            avg2 += texture2D(tex, texcoord + vec2(i,j) * tex_unit);
        }
    }
    avg1 += avgboth;
    avg2 += avgboth;
    gl_FragColor.rg = (1.0/36.0) * (avg1.rg + avg1.ba);
    gl_FragColor.ba = (1.0/36.0) * (avg2.rg + avg2.ba);
}
