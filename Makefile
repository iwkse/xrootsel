CFLAGS=`pkg-config --cflags x11 imlib2`
LIBS+=`pkg-config --libs x11 imlib2`
CC=gcc

all:
	$(CC) xrootsel.c $(CFLAGS) $(LIBS) -o xrootsel
clean:
	rm -f xrootsel
