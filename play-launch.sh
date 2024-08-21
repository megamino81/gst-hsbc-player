gst-launch-1.0 filesrc location=/home/root/test/videoplayback.mp4 \
! qtdemux ! h264parse ! mfxdecode ! mfxsink hue=100 \
2>&1 
