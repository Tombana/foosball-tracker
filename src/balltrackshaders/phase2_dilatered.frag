// Balltrack shader second phase
// Ouput is 4X smaller in both directions
// We will use GL_LINEAR so that the GPU samples 4 texels at once.
//
// In the height dimension, where we have one output:
// tex_unit:-8 -7 -6 -5 -4 -3 -2 -1  0  1  2  3  4  5  6  7  8
// Input:    |--|--|--|--|--|--|--|--|--|--|--|--|--|--|--|--|
// texcoord:                   |-----*-----|
// samples:        |--*--|--*--|--*--|--*--|--*--|--*--|
//
// In the width dimension, where we have two *outputs*:
// tex_unit:-6    -5    -4    -3    -2    -1     0     1     2     3     4     5     6
// Input:    |RG|BA|RG|BA|RG|BA|RG|BA|RG|BA|RG|BA|RG|BA|RG|BA|RG|BA|RG|BA|RG|BA|RG|BA|
// texcoord:                         |----R-G----*----B-A----|
// Output:               |<--             R G             -->|
// Output:                           |<--             B A             -->|
// samples:              |-----*-----|-----*-----|-----*-----|-----*-----|

uniform sampler2D tex;
varying vec2 texcoord; // center of output pixel
uniform vec2 tex_unit; // size of input texel
void main(void) {
    bool foundRed = false;
    vec4 avg1 = vec4(0,0,0,0);
    vec4 avg2 = vec4(0,0,0,0);
    for (int i = -3; i <= 3; i += 2) {
        for (int j = -5; j <= 5; j += 2) {
            vec4 col = texture2D(tex, texcoord + vec2(i,j) * tex_unit);
            if (min(col.r,col.b) < 0.78) {
                foundRed = true;
            }
            // I *really* hope it unrolls this loop
            if (j == -1 || j == 1) {
                if (i == -1) {
                    avg1 += col;
                } else if (i == 1) {
                    avg2 += col;
                }
            }
        }
    }
    gl_FragColor.rg = (1.0/4.0) * (avg1.rg + avg1.ba);
    gl_FragColor.ba = (1.0/4.0) * (avg2.rg + avg2.ba);
    if (foundRed) {
        gl_FragColor.r = 0.0;
        gl_FragColor.b = 0.0;
    } else {
        // Scale [0.8,1] to [0,1]
        // 5.0 * r - 4.0 
        // where r = (avg.r + avg.b) / 4.0 
        gl_FragColor.r = 5.0 * gl_FragColor.r - 4.0;
        gl_FragColor.b = 5.0 * gl_FragColor.b - 4.0;
    }
}
