#include "analysis.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <vector>
#include <fstream>

// To communicate with the Python websocket server
// we use a named pipe (FIFO) stored at
//     /tmp/fooballtrackerpipe.in
// These includes allow writing to such an object
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

// Size of the goal in field-coordinates,
// i.e., 1.0f is the full field width/height
float goalWidth = 0.10f;
float goalHeight = 0.38f;


// Ball history
const int historyCount = 256;
POINT balls[historyCount]; // in [0,1]x[0,1] field coordinates
int ballFrames[historyCount];
POINT ballsScreen[historyCount]; // in [-1,1]x[-1,1] screen coordinates
int ballCur = 0;

int ballMissing = 1000;


FIELD field;
int frameNumber = 0;

// Size of the green field:
// 120.5 cm x 61.4 cm
// Size of the field including the white bars:
// 120.5 cm x 70.2 cm
// Height factor for including the white bars: 1.1433

// For ball speeds
constexpr float fieldWidth  = 1.205f; // in meters
constexpr float fieldHeight = 0.702f; // in meters
extern float stableFPS; // Computed in core.cpp
constexpr int BallSpeedCount = 5 * 60; // 5 seconds at 60 fps
float ballSpeeds[BallSpeedCount];
int ballSpeedIndex = 0;
int ballSpeedFramesSinceLastUpdate = 0; // To prevent flooding the server


int analysis_send_to_server(const char* str) {
    int fd = open("/tmp/foosballtrackerpipe.in", O_WRONLY | O_NONBLOCK);
    if (fd > 0) {
        write(fd, str, strlen(str));
        close(fd);
        return 1;
    }
    return 0;
}

void sendMaxSpeed(float speed) {
    char buffer[64];
    sprintf(buffer, "MAXSPEED %.1f\n", speed);
    analysis_send_to_server(buffer);
    ballSpeedFramesSinceLastUpdate = 0;
}

std::ofstream timeseriesfile;

int analysis_init() {
#ifdef GENERATE_TIMESERIES
    timeseriesfile.open("/tmp/timeseries.txt");
    if (!timeseriesfile.is_open()) {
        printf("Unable to open /tmp/timeseries.txt\n");
    } else {
        timeseriesfile << "(* framenumber, x, y *)" << std::endl;
    }
#endif
    return 1;
}

// Returns 0 when not in goal
// Returns 1 when in left goal
// Returns 2 when in right goal
int isInGoal(POINT ball) {
    if (ball.y > 0.5f - 0.5f * goalHeight && ball.y < 0.5f + 0.5f * goalHeight) {
        if (ball.x < 0.0f + goalWidth) {
            return 1;
        } else if (ball.x > 1.0f - goalWidth) {
            return 2;
        }
    }
    return 0;
}

// Input in field coordinates
// Output in meters
float dist(POINT a, POINT b) {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    // x,y are in [0,1] range
    dx *= fieldWidth;
    dy *= fieldHeight;
    return std::sqrt(dx * dx + dy * dy);
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
    if (ball.x < 0.0f || ball.x >= 1.0f)
        return 0;

    return (int)(1.0f + 8.0f * ball.x);
}

int playerBarFrameThreshold = 6;

int barTeams[9] = {0, 1, 1, 2, 1, 2, 1, 2, 2};

// team == 1 -> goal for red, scored by blue
// team == 2 -> goal for blue, scored by red
int getPlayerWhoScored(int team) {
    int curIdx = ballCur;
    int player = 0;
    int hits = 0;

    // Look back 2.5 seconds
    int maxI = int(2.5f * stableFPS);
    if (maxI > historyCount)
        maxI = historyCount;
    for(int i = 0; i < maxI; ++i) {
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
    static int sendFAST = 0;
    static int lastGOAL = 0;

    // Only send the signals if they do not get interrupted by a goal within 0.5 seconds
    if (sendSAVE) {
        if (sendSAVE++ > int(0.5f * stableFPS)) {
            analysis_send_to_server("SAVE\n");
            sendSAVE = 0;
        }
    }
    if (sendFAST) {
        if (sendFAST++ > int(0.5f * stableFPS)) {
            analysis_send_to_server("FAST\n");
            sendFAST = 0;
        }
    }

    int prevIdx = (ballCur == 0 ? historyCount - 1 : ballCur - 1);
    if (ballFound) {
        if (ballMissing >= 30 && ballMissing != 1000) {
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
        if (frameDiffs <= 10 && frameNumber > 100) {
            float ballDist = dist(prevBall, ball);
            float ballSpeed = ballDist * stableFPS / float(frameDiffs);
            // ballDist is in meters
            ballSpeed *= 3.6f;
            // ballSpeed is in km/h

            ballSpeeds[ballSpeedIndex] = ballSpeed;
            ++ballSpeedIndex;
            if (ballSpeedIndex == BallSpeedCount)
                ballSpeedIndex = 0;

            if (ballSpeed > 15.0f &&
                (ball.y > 0.5f - 0.5f * goalHeight &&
                 ball.y < 0.5f + 0.5f * goalHeight) &&
                (ball.x < 0.0f + 3.0f * goalWidth ||
                 ball.x > 1.0f - 3.0f * goalWidth)) {
                sendSAVE = 1;
            } else if (ballSpeed > 30.0f) { // km/h
                sendFAST = 1;
            }
        }

        if(ballCur >= historyCount) {
            ballCur = 0;

            if (timeseriesfile.is_open()) {
                for (int i = 0; i < historyCount; ++i) {
                    timeseriesfile << '{' << ballFrames[i] << ',' << balls[i].x << ',' << balls[i].y << '}' << std::endl;
                }
            }
        }

    } else {
        ballSpeeds[ballSpeedIndex] = 0.0f;
        ++ballSpeedIndex;
        if (ballSpeedIndex == BallSpeedCount)
            ballSpeedIndex = 0;

        if (ballMissing++ == 15) {
            int goal = isInGoal(balls[prevIdx]);
            if (goal) {
                sendSAVE = 0; // Dont send a potential SAVE
                sendFAST = 0;
                if (frameNumber - lastGOAL >= int(1.5f * stableFPS)) { // Check if the last goal was at least 1.5 seconds ago
                    lastGOAL = frameNumber;
                    int player = getPlayerWhoScored(goal);
                    char buffer[64];
                    if (goal == 1) {
                        printf("Goal for red scored by \"bar\" %d\n", player);
                        sprintf(buffer, "RG %d\n", player);
                    } else if (goal == 2) {
                        printf("Goal for blue scored by \"bar\" %d\n", player);
                        sprintf(buffer, "BG %d\n", player);
                    }
                    analysis_send_to_server(buffer);
                }
            }
        }
    }

    if (frameNumber > 100 && ballSpeedFramesSinceLastUpdate > int(0.5f * stableFPS)) {
        // Only look at the last 5 seconds
        int numFrames = int(5.0f * stableFPS);
        float max = 0.0f;
        for (int i = ballSpeedIndex - numFrames; i < ballSpeedIndex; ++i) {
            int idx = (i < 0 ? i + BallSpeedCount : i);
            if (ballSpeeds[idx] > max) {
                max = ballSpeeds[idx];
            }
        }
        sendMaxSpeed(max);
    }

    ++ballSpeedFramesSinceLastUpdate;

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
    // Draw `player bar regions`
    // (#bar - 1)/8 <= (x+1)/2 < #bar / 8
    for (int bar = 1; bar < 8; ++bar) {
        // These are still `field coordinates`
        float barMin = ((float)(bar-1))/4.0f - 1.0f;
        float barMax = ((float)bar)/4.0f - 1.0f;
        // Go to real coordinates
        barMin = (field.xmax - field.xmin) * 0.5f * (1.0f + barMin) + field.xmin;
        barMax = (field.xmax - field.xmin) * 0.5f * (1.0f + barMax) + field.xmin;
        draw_square( barMin, barMax, field.ymin, field.ymax, 0xff00c000);
    }

    // Draw green bounding box, after drawing player regions because this
    // has to be on top
    draw_square(field.xmin, field.xmax, field.ymin, field.ymax, 0xff00ff00);

    // Compute goal in screen coordinates
    float yAvg = 0.5f * (field.ymin + field.ymax);
    float goalTop = yAvg + 0.5f * goalHeight * (field.ymax - field.ymin);
    float goalBot = yAvg - 0.5f * goalHeight * (field.ymax - field.ymin);
    float goalW = goalWidth * (field.xmax - field.xmin);

    // Draw `goals`
    draw_square(field.xmin, field.xmin + goalW, goalBot, goalTop, 0xff00ff00);
    draw_square(field.xmax - goalW, field.xmax, goalBot, goalTop, 0xff00ff00);

    // Draw line for ball history
    // Be carefull with circular buffer
    if (ballCur - 60 >= 0) {
        draw_line_strip(&ballsScreen[ballCur-60], 60, 0xffff0000);
    } else {
        draw_line_strip(&ballsScreen[historyCount + ballCur - 60], 60 - ballCur, 0xffff0000);
        draw_line_strip(&ballsScreen[0], ballCur, 0xffff0000);
    }

    // Draw squares on detection points
    for (int i = ballCur - 10; i < ballCur; ++i) {
        int time = ballCur - i;
        int blue = 0xff - 3 * time;
        // The bytes are R,G,B,A but little-endian so 0xAABBGGRR
        int color = 0xff000000 | (blue << 16);
        float size = 0.003f * time;

        int idx = (i < 0 ? i + historyCount : i);
        POINT* pt = &ballsScreen[idx];
        draw_square(pt->x - 0.5f * size, pt->x + 0.5f * size, pt->y - size, pt->y + size, color);
    }

    return 1;
}

// This runs in thread separate from the GL thread
int analysis_process_ball_buffer(uint8_t* pixelbuffer, int width, int height) {
    int fieldxmin = (int)(0.5f * (1.0f + field.xmin) * (float)width - 1.5f);
    int fieldxmax = (int)(0.5f * (1.0f + field.xmax) * (float)width + 1.5f);
    int fieldymin = (int)(0.5f * (1.0f + field.ymin) * (float)height - 1.5f);
    int fieldymax = (int)(0.5f * (1.0f + field.ymax) * (float)height + 1.5f);

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

    // Total should be at least T pixels (where T is taken from neural network)
    // But it was first averaged over 8x8 = 64 pixels
    // And that is rescaled to the 256 range
    // So (T/64) * 255 ~= threshold2

    int threshold1 = 100; // The max pixel should be at least this
    int threshold2 = 270; // The total in the neighborhood should be at least this
    bool ballFound = maxValue > threshold1 && weight > threshold2;

    POINT ball;
    // First map to [-1,1] screen coordinate range and save it
    ball.x = (2.0f * x) / ((float)width) - 1.0f;
    ball.y = (2.0f * y) / ((float)height) - 1.0f;
    if (ballFound)
        ballsScreen[ballCur] = ball;

    // Then map to [0,1]x[0,1] field coordinates
    ball.x = (ball.x - field.xmin) / (field.xmax - field.xmin);
    ball.y = (ball.y - field.ymin) / (field.ymax - field.ymin);

    analysis_update(ball, ballFound);
    return 0;
}

std::vector<int> xSums, ySums;

// This runs in thread separate from the GL thread
int analysis_process_field_buffer(uint8_t* pixelbuffer, int width, int height) {
    // Row and column sums
    xSums.resize(height);
    ySums.resize(width);
    for (auto& x : xSums)
        x = 0;
    for (auto& x : ySums)
        x = 0;

    int totalValue = 0;

    uint8_t* ptr = pixelbuffer;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            uint32_t value = (uint32_t) *ptr++;
            if (value > 20) { // Ignore *really* small noise
                xSums[y] += value;
                ySums[x] += value;
                totalValue += value;
            }
        }
    }

    // The shape of the row sums is something like
    //
    //     ______
    //     |     |
    //     |     |
    //    /       |
    // \__|       \___/
    // ----------------
    //     <----->
    //
    // We want to know the location of the main peak
    // and ignore the noise on the sides.
    //
    // Let M be the mean of the above thing.
    //
    // We simply take the leftmost and righmost points
    // that are above 0.5 * M;
    //
    // The shape of the column sums has a lot of peaks
    // because of the player bars that interrupt it.
    // And near the goal the green field is also smaller
    // which brings it much closer to the 'noise level'.
    // However, 0.5 * M still seems to work, judging from plots.

    int fieldxmin = 0;
    int fieldxmax = 0;
    int fieldymin = 0;
    int fieldymax = 0;

    int ySumBound = (totalValue / (2 * width));
    for (int x = 0; x < width; ++x) {
        if (ySums[x] > ySumBound) {
            fieldxmin = x;
            break;
        }
    }
    for (int x = width - 1; x > 0; --x) {
        if (ySums[x] > ySumBound) {
            fieldxmax = x;
            break;
        }
    }

    int xSumBound = (totalValue / (2 * height));
    for (int y = 0; y < height; ++y) {
        if (xSums[y] > xSumBound) {
            fieldymin = y;
            break;
        }
    }
    for (int y = height - 1; y > 0; --y) {
        if (xSums[y] > xSumBound) {
            fieldymax = y;
            break;
        }
    }

#ifdef DEBUG_SUMS
    {
        // For plotting in Mathematica
        // xs = Import["sums.m"];
        // rowsums = ListPlot[xs[[All, 1]], Joined -> True]
        // colsums = ListPlot[xs[[All, 2]], Joined -> True]
        static FILE* sumfile = 0;
        if (!sumfile) {
            sumfile = fopen("/tmp/sums.m", "w");
            if (!sumfile)
                printf("Unable to open /tmp/sums.m\n");
            else
                fprintf(sumfile, "(* {row sums, column sums} *)\n{\n");
        } else {
            fprintf(sumfile, "{{%d", xSums[0]);
            for (auto i = 1u; i < xSums.size(); i++) {
                fprintf(sumfile, ", %d", xSums[i]);
            }
            fprintf(sumfile, "},\n");

            fprintf(sumfile, "{%d", ySums[0]);
            for (auto i = 1u; i < ySums.size(); i++) {
                fprintf(sumfile, ", %d", ySums[i]);
            }
            fprintf(sumfile, "}},\n");
            fflush(sumfile);
        }
    }
#endif

    fieldxmin -= 1;
    fieldxmax += 1;
    fieldymin -= 1;
    fieldymax += 1;

    // The above values are at lower-left pixel corners
    // Take the max values at top-right pixel corners:
    fieldxmax++;
    fieldymax++;

    // Map to [-1,1]
    FIELD newField;
    newField.xmin = (2.0f * fieldxmin) / ((float)width) - 1.0f;
    newField.xmax = (2.0f * fieldxmax) / ((float)width) - 1.0f;
    newField.ymin = (2.0f * fieldymin) / ((float)height) - 1.0f;
    newField.ymax = (2.0f * fieldymax) / ((float)height) - 1.0f;

    // Include the white bars at the top and bottom
    // Physical measurement says the height factor is about 1.143
    // a -> (1/2) ( (1-factor)*b + (1+factor) * a )
    // b -> (1/2) ( (1+factor)*b + (1-factor) * a )
    float fplus = 0.5f *  2.143f;
    float fmin  = 0.5f * -0.143f;
    float a,b;
    a = fmin * newField.ymax + fplus * newField.ymin;
    b = fplus * newField.ymax + fmin * newField.ymin;
    newField.ymin = a;
    newField.ymax = b;
    a = fmin * newField.xmax + fplus * newField.xmin;
    b = fplus * newField.xmax + fmin * newField.xmin;
    newField.xmin = a;
    newField.xmax = b;

    if (newField.xmin < -1.0f)
        newField.xmin = -1.0f;
    if (newField.ymin < -1.0f)
        newField.ymin = -1.0f;
    if (newField.xmax > 1.0f)
        newField.xmax = 1.0f;
    if (newField.ymax > 1.0f)
        newField.ymax = 1.0f;

    // Time average for field, because it fluctuates too much
    field.xmin = 0.80f * field.xmin + 0.20 * newField.xmin;
    field.xmax = 0.80f * field.xmax + 0.20 * newField.xmax;
    field.ymin = 0.80f * field.ymin + 0.20 * newField.ymin;
    field.ymax = 0.80f * field.ymax + 0.20 * newField.ymax;

    return 0;
}
