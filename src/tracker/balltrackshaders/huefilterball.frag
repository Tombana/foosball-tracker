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

// Ball hue lower bound is important to distinguish from red players
// Ball sat lower bound is important to distinguish it from pure white
//
// These (Hue,Saturation,Value) tuples are all in range [0-360]x[0,100]x[0,100]
// This is all from video. Not always the same as from camera.
// Normal ball:             (16-20, 50-70, 70-80)
// Fast blur ball in light: (?-60 , 15+  , 50+)
// Ball in goal:            (17-30, 60-80, 30-50)
// Ball in goal dark:       (16-24, 75-95, 14-21) low Value!
// Ball on white corner:    ( 9-12, 65-85, 40-55)
// Red player edges:        (?-15 , 50-70, 35-54)
//                          (47   , 42   , 31)
// Red player spinning:     ( 25  , 50   , 50)   :(
//
// For the replay video, seperating the Hue as  0.18 < neutral < 0.25 is fine.
// For the camera, the bound has to be much lower. Like  0.06 < neutral < 0.14
//
// Yellow ball from video: (45-60, 50-70, 50-80)
// Yellow ball from framedump: (50,53,84)
// Yellow ball from framedump near dark goal: (48-53, 50-65, 22-37)
//
// Field: (125-175, 15-75, 13-70)
// Rescaling table
// Hue [0-360] : 4     6    7     8     9     10    11    12   14    15    16    17    18   45
// Hue [0-6]   : 0.067 0.10 0.117 0.133 0.150 0.167 0.183 0.20 0.233 0.250 0.267 0.283 0.30 0.75
#extension GL_OES_EGL_image_external : require

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
