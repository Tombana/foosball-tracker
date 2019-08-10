#!/bin/bash

pkill raspiballs 2>/dev/null

fragments_path="/dev/shm/replay/fragments"
mkdir -p $fragments_path

exec ./raspiballs -o $fragments_path/out%04d.h264 -w 1280 -h 720 -fps 40 -t 0  -sg 100 -wr 100 -g 10 --ev 5 --glwin 450,700,640,480
#exec /opt/vc/bin/raspivid -o $fragments_path/out%04d.h264 -w 1280 -h 720 -fps 60 -t 0  -sg 100 -wr 100 -g 10 --ev 5 -p 450,700,640,480
