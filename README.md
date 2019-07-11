# Raspberry Pi balltracker

This repository contains the code for running the balltracking software on the Raspberry Pi GPU.
Only tested on 32 bit OS, the libraries will probably not work on 64 bit.

## Prerequisites

On a Raspberry Pi, the correct libraries should already be present in `/opt/vc` so there is no need to do anything.
If the files are not present, then you can build and install them yourself as follows (from a separate directory).

    git clone https://github.com/raspberrypi/userland
    cd userland
    ./buildme

It might ask for a sudo password after building, in order to install the files into `/opt/vc`.

## Compiling

Use the `buildme` script to build

