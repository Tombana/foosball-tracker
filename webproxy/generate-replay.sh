#!/bin/sh

fragments_path="/dev/shm/replay/fragments"
replay_file="/dev/shm/replay/replay.h264"
ignore_recent_chunks=1
replay_chunks=14

fragments=`ls -tr $fragments_path/out*.h264 | head -n-$ignore_recent_chunks | tail -n$replay_chunks`

if [ -n "$fragments" ]; then
    cat $fragments > $replay_file
    #cp $replay_file /media/pi/FOOSDRIVE/`date +"%Y%m%d_%H%M%S"`.h264
fi
