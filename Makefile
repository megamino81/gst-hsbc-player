CFLAGS=-g -Wall -Wextra $(shell pkg-config --cflags gstreamer-1.0)
LDFLAGS=$(shell pkg-config --libs gstreamer-1.0)

all:
	gcc -o pipeline pipeline.c $(CFLAGS) $(LDFLAGS)
	gcc -o videotestsrc videotestsrc.c $(CFLAGS) $(LDFLAGS)

clean:
	rm pipeline videotestsrc
