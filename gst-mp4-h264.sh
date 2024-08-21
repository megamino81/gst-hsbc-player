#!/bin/sh

gst-launch-1.0 filesrc location=$1 ! qtdemux ! h264parse ! mfxh264dec ! mfxsink
