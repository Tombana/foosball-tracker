// Balltrack shader third phase
// Ouput is 2X smaller in both directions
// In the height dimension, where we have one output:
// tex_unit:-8 -7 -6 -5 -4 -3 -2 -1  0  1  2  3  4  5  6  7  8
// Input:    |--|--|--|--|--|--|--|--|--|--|--|--|--|--|--|--|
// texcoord:                      |--*--|
// samples:                       |--*--|
//
// In the width dimension, where we have two *outputs*:
// tex_unit:-6    -5    -4    -3    -2    -1     0     1     2     3     4     5     6
// Input:    |RG|BA|RG|BA|RG|BA|RG|BA|RG|BA|RG|BA|RG|BA|RG|BA|RG|BA|RG|BA|RG|BA|RG|BA|
// texcoord:                               |-R-G-*-B-A-|
// Output:                                 |<R G>|
// Output:                                       |<B A>|
// samples:                                |--*--|--*--|  do not use GL_LINEAR ??
uniform sampler2D tex;
varying vec2 texcoord;
uniform vec2 tex_unit;
void main(void) {
    //bool foundRed = false;
    //for (int i = -2; i <= 2; i += 2) {
    //   vec4 col = texture2D(tex, texcoord + vec2(i,0) * tex_unit);
    //   if (min(col.r,col.b) < 0.78) {
    //       foundRed = true;
    //   }
    //}
    vec4 col1 = texture2D(tex, texcoord - vec2(0.5,0) * tex_unit );
    vec4 col2 = texture2D(tex, texcoord + vec2(0.5,0) * tex_unit );
    gl_FragColor.rg = 0.5 * (col1.rg + col1.ba);
    gl_FragColor.ba = 0.5 * (col2.rg + col2.ba);
    //if (foundRed) {
    //    gl_FragColor.r = 0.0;
    //    gl_FragColor.b = 0.0;
    //} else {
    //    // Rescale from [0.8,1] to [0,1] 
    //    gl_FragColor.r = 5.0 * gl_FragColor.r - 4.0;
    //    gl_FragColor.b = 5.0 * gl_FragColor.b - 4.0;
    //}
}
