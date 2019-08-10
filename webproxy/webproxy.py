#!/usr/bin/env python3

from websocket_server import WebsocketServer

import os
import threading
import time

import subprocess

def call_and_log(*args, **kwargs):
    """Run process, gather output and write to log"""
    p = subprocess.Popen(*args, stdout=subprocess.PIPE, stderr=subprocess.PIPE, **kwargs)
    stdout, stderr = p.communicate()
    if len(stdout) > 0:
        print(stdout.decode("utf-8").strip())
    if len(stderr) > 0:
        print(stderr.decode("utf-8").strip())
    if p.returncode != 0:
        print("{} returned {}".format(p.args, p.returncode))

#def long_running(*args, **kwargs):
#    """Run process redirecting stderr to stdout and write output to log"""
#    p = subprocess.Popen(*args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, bufsize=1, **kwargs)
#    with p.stdout:
#        for line in iter(p.stdout.readline, b''):
#            print(line.decode("utf-8").strip())
#
#    p.wait()
#    if p.returncode != 0:
#        print("{} returned {}".format(p.args, p.returncode))

camprocess = None
replayprocess = None
heartbeatTimer = 1000 
heartbeatLock = threading.Lock()

def startTracking():
    global camprocess
    if camprocess is None:
        camprocess = subprocess.Popen(["./run-camera.sh"])

def stopTracking():
    global camprocess
    print("Stop tracking request!")
    if camprocess is not None:
        camprocess.terminate()
    camprocess = None

def doReplay():
    global replayprocess
    print("Replay request!")
    call_and_log(["./generate-replay.sh"])
    # replayprocess.terminate()
    replayprocess = subprocess.Popen(["./replay.sh"])


# Called for every client connecting (after handshake)
def new_client(client, server):
    print("New client connected and was given id %d" % client['id'])
    server.send_message_to_all("Hey all, a new client has joined us")


# Called for every client disconnecting
def client_left(client, server):
    print("Client(%d) disconnected" % client['id'])
    stopTracking()


# Called when a client sends a message
def message_received(client, server, message):
    global heartbeatTimer
    global heartbeatLock
    if (message == "start"):
        startTracking()
    elif (message == "stop"):
        stopTracking()
    elif (message == "replay"):
        doReplay()
    elif (message == "heartbeat"):
        heartbeatLock.acquire()
        heartbeatTimer = 0
        #startTracking()
        heartbeatLock.release()
    else:
        if len(message) > 200:
            message = message[:200]+'..'
        print("Client(%d) said: %s" % (client['id'], message))


PORT=8420
server = WebsocketServer(PORT)
server.set_fn_new_client(new_client)
server.set_fn_client_left(client_left)
server.set_fn_message_received(message_received)


class MyReadThread(threading.Thread):
    def run(self):
        print("Read thread started.")
        fifo_file = "/tmp/foos-debug.in"
        try:
            os.mkfifo(fifo_file)
        except:
            pass
        while True:
            f = open(fifo_file, "r")
            if not f:
                print("Error opening fifo file %s" % fifo_file)
                time.sleep(5)
                continue
            print("Fifo file opened.")
            while True:
                line = f.readline()
                if not line:
                    break
                line = line.strip()
                print("Message from fifo file: %s" % line)
                server.send_message_to_all(line);

mythread = MyReadThread()
mythread.start()


class HeartbeatThread(threading.Thread):
    def run(self):
        global heartbeatTimer
        global heartbeatLock
        print("Heartbeat thread started.")
        while True:
            stopCam = False
            heartbeatLock.acquire()
            heartbeatTimer = heartbeatTimer + 1
            if (heartbeatTimer == 5): # No heartbeats for 5 seconds
                stopCam = True
            heartbeatLock.release()
            if (stopCam == True):
                print("Did not receive heartbeat for 5 seconds!")
                stopTracking()
            time.sleep(1)

hbthread = HeartbeatThread()
hbthread.start()


server.run_forever()
