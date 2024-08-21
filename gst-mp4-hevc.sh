#!/bin/sh

gst-launch-1.0 filesrc location=$1 ! qtdemux ! h265parse ! mfxhevcdec ! mfxsink
