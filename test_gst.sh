#!/bin/sh -x

gst-launch-1.0 filesrc location=/home/root/media/H264_Main@L3.1_1280x0720_1.99Mbps_30.0fps_Progressive_Sound_Football.mp4 ! qtdemux ! h264parse ! mfxdecode ! mfxsink
