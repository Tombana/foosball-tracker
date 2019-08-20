#include "analysis.h"
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
float goalWidth = 0.15f;
float goalHeight = 0.35f;


// Ball history
const int historyCount = 120;
POINT balls[historyCount]; // in field coordinates
int ballFrames[historyCount];
POINT ballsScreen[historyCount]; // in screen coordinates
int ballCur = 0;

int ballMissing = 1000;


FIELD field;
int frameNumber;


int analysis_send_to_server(const char* str) {
    int fd = open("/tmp/foos-debug.in", O_WRONLY | O_NONBLOCK);
    if (fd > 0) {
        write(fd, str, strlen(str));
        close(fd);
        return 1;
    }
    return 0;
}

int timeseriesfile = 0;

int analysis_init() {
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
int isInGoal(POINT ball) {
    if (ball.y > -goalHeight && ball.y < goalHeight) {
        if (ball.x < -1.0f + goalWidth) {
            return 1;
        } else if (ball.x > 1.0f - goalWidth) {
            return 2;
        }
    }
    return 0;
}

float distSq(POINT a, POINT b) {
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
int getPlayerBar(POINT ball) {
    if (ball.x < -1.0f || ball.x >= 1.0f)
        return 0;

    return (int)(1.0f + 8.0f * (0.5f * (ball.x + 1.0f)));
}

int playerBarFrameThreshold = 4;

int barTeams[9] = {0, 1, 1, 2, 1, 2, 1, 2, 2};

// team == 1 -> goal for red, scored by blue
// team == 2 -> goal for blue, scored by red
int getPlayerWhoScored(int team) {
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

int analysis_update(POINT ball, bool ballFound) {
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

    int prevIdx = (ballCur == 0 ? historyCount - 1 : ballCur - 1);
    if (ballFound) {
        if (ballMissing >= 30) {
            printf("Ball was gone for %d frames.\n", ballMissing);
        }
        ballMissing = 0;

        balls[ballCur] = ball;
        ballFrames[ballCur] = frameNumber;
        ballsScreen[ballCur].x = (field.xmax - field.xmin) * 0.5f * (1.0f + ball.x) + field.xmin;
        ballsScreen[ballCur].y = (field.ymax - field.ymin) * 0.5f * (1.0f + ball.y) + field.ymin;
        ++ballCur;

        // Check for fast shot to goal
        // This point and previous points should be at most 2 frames apart
        POINT prevBall = balls[prevIdx];
        int frameDiffs = frameNumber - ballFrames[prevIdx];
        if (frameDiffs <= 20) {
            // Distance should be large ??
            // At least x % of field width ( = 2.0f ) per frame
            float distThreshold = ((float)frameDiffs) * 0.10f * 2.0f;
            if (distSq(prevBall, ball) > distThreshold * distThreshold ) {
                if (ball.y > -goalHeight && ball.y < goalHeight && 
                        (ball.x < -1.0f + 3.0f * goalWidth || ball.x > 1.0f - 3.0f * goalWidth) ) {
                    sendSAVE = 1;
                } else {
                    //sendFAST = 1;
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
                if (frameNumber - lastGOAL >= 60) { // Check if the last goal was at least 60 frames ago
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

// TODO: This is called from the GL thread
// whereas the update function is called from a separate thread
// The `field` and `ballsScreen` are not yet properly protected
// from threading issues
int analysis_draw() {
    // Draw green bounding box
    draw_square(field.xmin, field.xmax, field.ymin, field.ymax, 0xff00ff00);
    float yAvg = 0.5f * (field.ymin + field.ymax);

    // Draw `goals`
    draw_square(field.xmin, field.xmin + goalWidth, yAvg - goalHeight, yAvg + goalHeight, 0xff00ff00);
    draw_square(field.xmax - goalWidth, field.xmax, yAvg - goalHeight, yAvg + goalHeight, 0xff00ff00);

    // Draw line for ball history
    // Be carefull with circular buffer
    draw_line_strip(&ballsScreen[0], ballCur, 0xffff0000);
    draw_line_strip(&ballsScreen[ballCur], historyCount - ballCur, 0xffff0000);

    // Draw squares on detection points
    for (int i = ballCur - 20; i < ballCur; ++i) {
        int time = ballCur - i;
        int blue = 0xff - time;
        // The bytes are R,G,B,A but little-endian so 0xAABBGGRR
        int color = 0xff000000 | (blue << 16);
        float size = 0.002f * time;

        int idx = (i < 0 ? i + historyCount : i);
        POINT* pt = &ballsScreen[idx];
        draw_square(pt->x - 0.5f * size, pt->x + 0.5f * size, pt->y - size, pt->y + size, color);
    }

    return 1;
}

// This runs in thread separate from the GL thread
int analysis_process_ball_buffer(uint8_t* pixelbuffer, int width, int height) {
    int fieldxmin = (int)(0.5f * (1.0f + field.xmin) * (float)width - 1.0f);
    int fieldxmax = (int)(0.5f * (1.0f + field.xmax) * (float)width + 1.0f);
    int fieldymin = (int)(0.5f * (1.0f + field.ymin) * (float)height - 1.0f);
    int fieldymax = (int)(0.5f * (1.0f + field.ymax) * (float)height + 1.0f);

    // TODO: BLUR ?

    // Find the max orange intensity
    int maxx = 0, maxy = 0;
    uint32_t maxValue = 0;
    uint8_t* ptr = pixelbuffer;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            uint32_t value = (uint32_t) *ptr++;

            if ( y < fieldymin || y > fieldymax ) continue;
            if ( x < fieldxmin || x > fieldxmax ) continue;
            if (value > maxValue) {
                maxx = x;
                maxy = y;
                maxValue = value;
            }
        }
    }

    // Take weighted average near the maximum
    int avgx = 0, avgy = 0;
    int weight = 0;
    ptr = pixelbuffer;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            uint32_t value = (uint32_t) *ptr++;
            if (y < maxy - 5 || y > maxy + 5) continue;
            if (x < maxx - 5 || x > maxx + 5) continue;
            avgx += x * value;
            avgy += y * value;
            weight += value;
        }
    }

    // avgx, avgy are the bottom-left corner of the macropixels
    // Shift them by half a pixel to fix
    float x = 0.5f + (((float)avgx) / ((float)weight));
    float y = 0.5f + (((float)avgy) / ((float)weight));

    int threshold1 = 30;
    int threshold2 = 60;

    // Map to [-1,1] range
    POINT ball;
    ball.x = (2.0f * x) / ((float)width) - 1.0f;
    ball.y = (2.0f * y) / ((float)height) - 1.0f;
    // Map to field coordinates
    ball.x = -1.0f + 2.0f * (ball.x - field.xmin) / (field.xmax - field.xmin);
    ball.y = -1.0f + 2.0f * (ball.y - field.ymin) / (field.ymax - field.ymin);

    bool ballFound = maxValue > threshold1 && weight > threshold2;
    analysis_update(ball, ballFound);

    return 0;
}

// This runs in thread separate from the GL thread
int analysis_process_field_buffer(uint8_t* pixelbuffer, int width, int height) {
    // Start with a small bounding box in the middle and stretch it out
    int fieldxmin = (int)(0.48f * width);
    int fieldxmax = (int)(0.52f * width);
    int fieldymin = (int)(0.48f * height);
    int fieldymax = (int)(0.52f * height);

    uint8_t* ptr = pixelbuffer;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            uint32_t value = (uint32_t) *ptr++;

            if (value > 200) {
                if (x < fieldxmin) fieldxmin = x;
                if (x > fieldxmax) fieldxmax = x;
                if (y < fieldymin) fieldymin = y;
                if (y > fieldymax) fieldymax = y;
            }
        }
    }

    fieldxmin -= 4;
    fieldxmax += 4;
    fieldymin -= 3;
    fieldymax += 3;
    if (fieldxmin < 0) fieldxmin = 0;
    if (fieldymin < 0) fieldymin = 0;
    if (fieldxmax >= width) fieldxmax = width - 1;
    if (fieldymax >= height) fieldymax = height - 1;

    // Map to [-1,1]
    FIELD newField;
    newField.xmin = (2.0f * fieldxmin) / ((float)width) - 1.0f;
    newField.xmax = (2.0f * fieldxmax) / ((float)width) - 1.0f;
    newField.ymin = (2.0f * fieldymin) / ((float)height) - 1.0f;
    newField.ymax = (2.0f * fieldymax) / ((float)height) - 1.0f;

    // Time average for field, because it fluctuates too much
    field.xmin = 0.92f * field.xmin + 0.08 * newField.xmin;
    field.xmax = 0.92f * field.xmax + 0.08 * newField.xmax;
    field.ymin = 0.92f * field.ymin + 0.08 * newField.ymin;
    field.ymax = 0.92f * field.ymax + 0.08 * newField.ymax;

    return 0;
}
