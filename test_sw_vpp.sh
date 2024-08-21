#!/bin/sh -x                                                                    

gst-launch-1.0 filesrc location=$1 ! qtdemux ! h264parse ! avdec_h264 ! mfxvpp ! mfxsink                                                                        
#gst-launch-1.0 filesrc location=$1 ! qtdemux ! h264parse ! avdec_h264 !\       
# mfxvpp hue=2 saturation=2 contrast=2 brightness=2 \                           
# mfxsink
