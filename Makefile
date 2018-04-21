LIBS+=`pkg-config --libs x11 imlib2`
CC=gcc

all:
	$(CC) xrootsel.c $(LIBS) -o xrootsel
clean:
	rm -f xrootsel
