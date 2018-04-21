# xrootsel

Small utility which returns coordinates and size of the selection. I'm using this in a script to allow byzanz recording selections as well.

Sources are taken from scrot sources, needs some polish and add a makefile as well.

#Compilation

gcc xrootsel.c $(pkg-config --cflags --libs x11 imlib2) -o xrootsel
