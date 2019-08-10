#!/bin/sh

pkill player 2> /dev/null

./player "/dev/shm/replay/replay.h264" 20
