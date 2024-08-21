#!/bin/sh -x

gst-launch-1.0 icamerasrc device-name="mondello-3" io-mode=0 interlace-mode=alternate deinterlace-method=hw_weaving \
! video/x-raw,format=UYVY,width=720,height=480 ! mfxvpp \
! tee name=t \
t. ! queue ! mfxsink \
t. ! queue ! mfxsink
