gst-launch-1.0 filesrc location=/home/root/test/videoplayback.mp4 \
! tee name=aaa \
aaa. ! queue ! qtdemux ! h264parse ! mfxdecode ! mfxsink surface_name=video1 width=100 height=100 \
aaa. ! queue ! qtdemux ! h264parse ! mfxdecode ! mfxsink surface_name=video2 width=200 height=200 \
aaa. ! queue ! qtdemux ! h264parse ! mfxdecode ! mfxsink surface_name=video3 width=300 height=300 \
aaa. ! queue ! qtdemux ! h264parse ! mfxdecode ! mfxsink surface_name=video4 width=400 height=400
