#ifndef BALLANALYSIS_H
#define BALLANALYSIS_H

// All coordinates are in [-1,1] range, the OpenGL standard

typedef struct {
    float x;
    float y;
} POINT;

typedef struct {
    float xmin;
    float xmax;
    float ymin;
    float ymax;
} FIELD;

int analysis_init();

// ballFound can be 0 or 1, dependinding on whether the ball was found
int analysis_update(FIELD field, POINT ball, int ballFound);

int analysis_draw();

#endif
