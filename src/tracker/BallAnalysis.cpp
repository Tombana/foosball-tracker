#include "BallAnalysis.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// To communicate with the Python websocket server
// we use a named pipe (FIFO) stored at
//     /tmp/foos-debug.in
// These includes allow writing to such an object
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

// Constants
static float goalWidth = 0.15f;
static float goalHeight = 0.35f;


// Ball history
static int historyCount = 120;
static POINT balls[120];
static int ballFrames[120];
static int ballCur = 0;

static int frameNumber = 0;

static FIELD field;

static int ballMissing = 1000;

static int analysis_send_to_server(const char* str) {
    int fd = open("/tmp/foos-debug.in", O_WRONLY | O_NONBLOCK);
    if (fd > 0) {
        write(fd, str, strlen(str));
        close(fd);
        return 1;
    }
    return 0;
}

static int timeseriesfile = 0;

int analysis_init() {
    field.xmin = -0.8f;
    field.xmax =  0.8f;
    field.ymin = -0.8f;
    field.ymax =  0.8f;

#ifdef GENERATE_TIMESERIES
    timeseriesfile = open("/tmp/timeseries.txt", O_WRONLY);
    if (!timeseriesfile) {
        printf("Unable to open /tmp/timeseries.txt\n");
    } else {
        write(timeseriesfile, "(* framenumber, x, y *)\n", 24);
    }
#endif
    return 1;
}

// Returns 0 when not in goal
// Returns 1 when in left goal
// Returns 2 when in right goal
static int isInGoal(POINT ball) {
    float yAvg = 0.5f * (field.ymin + field.ymax);
    if (ball.y > yAvg - goalHeight && ball.y < yAvg + goalHeight) {
        if (ball.x < field.xmin + goalWidth) {
            return 1;
        } else if (ball.x > field.xmax - goalWidth) {
            return 2;
        }
    }
    return 0;
}

static float distSq(POINT a, POINT b) {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    return dx * dx + dy * dy;
}

// 
// 0 -- unknown
// 1 -- blue keeper
// 2 -- blue defender
// 3 -- red attacker
// 4 -- blue middle
// 5 -- red middle
// 6 -- blue attacker
// 7 -- red defender
// 8 -- red keeper
//
static int getPlayerBar(POINT ball) {
    float x = (ball.x - field.xmin) / (field.xmax - field.xmin);

    if (x < 0.0f || x >= 1.0f)
        return 0;

    return (int)(1.0f + 8.0f * x);
}

static int playerBarFrameThreshold = 4;

static int barTeams[9] = {0, 1, 1, 2, 1, 2, 1, 2, 2};

// team == 1 -> goal for red, scored by blue
// team == 2 -> goal for blue, scored by red
static int getPlayerWhoScored(int team) {
    int curIdx = ballCur;
    int player = 0;
    int hits = 0;
    for(int i = 0; i < 20; ++i) {
        // Go to previous index
        if (curIdx == 0)
            curIdx = historyCount - 1;
        else
            curIdx--;
        int p = getPlayerBar(balls[curIdx]);
        if (barTeams[p] == team)
            continue;
        if (p == player) {
            hits++;
            if (hits == playerBarFrameThreshold)
                break;
        } else {
            player = p;
            hits = 1;
        }
    }
    if (hits == playerBarFrameThreshold && player > 0 && player <= 8) {
        return player;
    }
    return 0;
}

int analysis_update(FIELD newField, POINT ball, int ballFound) {
    ++frameNumber;

    static int sendSAVE = 0;
    static int lastGOAL = 0;

    if (sendSAVE) {
        // Only send the SAVE if it does not get interrupted by a goal
        // within 20 frames
        if (sendSAVE++ == 20) {
            analysis_send_to_server("SAVE\n");
            sendSAVE = 0;
        }
    }

    // Time average for field, because it fluctuates too much
    field.xmin = 0.98f * field.xmin + 0.02 * newField.xmin;
    field.xmax = 0.98f * field.xmax + 0.02 * newField.xmax;
    field.ymin = 0.98f * field.ymin + 0.02 * newField.ymin;
    field.ymax = 0.98f * field.ymax + 0.02 * newField.ymax;

    int prevIdx = (ballCur == 0 ? historyCount - 1 : ballCur - 1);
    if (ballFound) {
        if (ballMissing >= 30) {
            printf("Ball was gone for %d frames.\n", ballMissing);
        }
        ballMissing = 0;

        balls[ballCur] = ball;
        ballFrames[ballCur] = frameNumber;
        ++ballCur;

        // Check for fast shot to goal
        // This point and previous points should be at most 2 frames apart
        POINT prevBall = balls[prevIdx];
        int frameDiffs = frameNumber - ballFrames[prevIdx];
        if (frameDiffs <= 20) {
            // Distance should be large ??
            // At least x % of field width per frame
            float distThreshold = ((float)frameDiffs) * 0.10f * (field.xmax - field.xmin);
            if (distSq(prevBall, ball) > distThreshold * distThreshold ) {
                float yAvg = 0.5f * (field.ymin + field.ymax);
                if (ball.y > yAvg - goalHeight && ball.y < yAvg + goalHeight && 
                        (ball.x < field.xmin + 3.0f * goalWidth || ball.x > field.xmax - 3.0f * goalWidth) ) {
                    sendSAVE = 1;
                } else {
                    //analysis_send_to_server("FAST\n");
                }
            }
        }

        if(ballCur >= historyCount) {
            ballCur = 0;

            if (timeseriesfile) {
                char buffer[128];
                for (int i = 0; i < historyCount; ++i) {
                    int len = sprintf(buffer, "{%d, %f, %f},\n", ballFrames[i], balls[i].x, balls[i].y);
                    write(timeseriesfile, buffer, len);
                }
            }
        }
    } else {
        if (ballMissing++ == 15) {
            int goal = isInGoal(balls[prevIdx]);
            if (goal) {
                sendSAVE = 0; // Dont send a potential SAVE
                if (frameNumber - lastGOAL >= 50) { // Check if the last goal was at least 50 frames ago
                    lastGOAL = frameNumber;
                    if (goal == 1) {
                        printf("Goal for red!\n");
                        analysis_send_to_server("RG\n");
                    } else if (goal == 2) {
                        printf("Goal for blue!\n");
                        analysis_send_to_server("BG\n");
                    }
                    int player = getPlayerWhoScored(goal);
                    if (player) {
                        printf("TEST: Scored by \"bar\" %d\n", player);
                        char buffer[128];
                        sprintf(buffer, "SCOREDBY %d\n", player);
                        analysis_send_to_server(buffer);
                    }
                }
            }
        }	
    }
    return 1;
}

// From BalltrackCore
void draw_square(float xmin, float xmax, float ymin, float ymax, uint32_t color);
void draw_line_strip(POINT* xys, int count, uint32_t color);

int analysis_draw() {
    // Draw green bounding box
    draw_square(field.xmin, field.xmax, field.ymin, field.ymax, 0xff00ff00);
    float yAvg = 0.5f * (field.ymin + field.ymax);

    // Draw `goals`
    draw_square(field.xmin, field.xmin + goalWidth, yAvg - goalHeight, yAvg + goalHeight, 0xff00ff00);
    draw_square(field.xmax - goalWidth, field.xmax, yAvg - goalHeight, yAvg + goalHeight, 0xff00ff00);

    // Draw line for ball history
    // Be carefull with circular buffer
    draw_line_strip(&balls[0], ballCur, 0xffff0000);
    draw_line_strip(&balls[ballCur], historyCount - ballCur, 0xffff0000);

    // Draw squares on detection points
    for (int i = ballCur - 20; i < ballCur; ++i) {
        int time = ballCur - i;
        int blue = 0xff - time;
        // The bytes are R,G,B,A but little-endian so 0xAABBGGRR
        int color = 0xff000000 | (blue << 16);
        float size = 0.002f * time;

        int idx = (i < 0 ? i + historyCount : i);
        POINT* pt = &balls[idx];
        draw_square(pt->x - 0.5f * size, pt->x + 0.5f * size, pt->y - size, pt->y + size, color);
    }

    return 1;
}

