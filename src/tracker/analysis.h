#pragma once

#include <cstdint> // for uint8_t

// Field is given in screen coordinates; [-1,1]x[-1,1] range, the OpenGL standard
struct FIELD {
    float xmin;
    float xmax;
    float ymin;
    float ymax;

    FIELD() : xmin(-0.9f), xmax(0.9f), ymin(-0.9f), ymax(0.9f) {};
};

// All ball positions are given in field coordinates; [-1,1]x[-1,1] range, normalized by the green field size
// So (-1,y) is at the left edge of the field and (+1,y) is at the right edge of the field.

struct POINT {
    float x;
    float y;
};

// Called from GL thread
int analysis_init();

// Called from separate analysis thread
int analysis_process_field_buffer(uint8_t* pixelbuffer, int width, int height);
int analysis_process_ball_buffer(uint8_t* pixelbuffer, int width, int height);

// Called from GL thread
int analysis_draw();

