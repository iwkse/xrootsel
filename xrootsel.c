/* xrootsel.c

Copyright (C) 1999,2000 Tom Gilbert.
Copyright (C) 2018 iwkse.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to
deal in the Software without restriction, including without limitation the
rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
sell copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies of the Software and its documentation and acknowledgment shall be
given in the documentation and software packages that this Software was
used.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/

#include <X11/Xlib.h>
#include <X11/cursorfont.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

/* get geometry of window and use that */

Window root = 0;
Display *disp = NULL;
Visual *vis = NULL;
int depth;
Colormap cm;
Screen *scr;

void
init_x(char *dispstr, int screen_num)
{
   disp = XOpenDisplay(dispstr);
   if (!disp)
      printf("Can't open X display. It *is* running, yeah?");
   if (screen_num)
      scr = ScreenOfDisplay(disp, screen_num);
   else
      scr = ScreenOfDisplay(disp, DefaultScreen(disp));

   vis = DefaultVisual(disp, XScreenNumberOfScreen(scr));
   depth = DefaultDepth(disp, XScreenNumberOfScreen(scr));
   cm = DefaultColormap(disp, XScreenNumberOfScreen(scr));
   root = RootWindow(disp, XScreenNumberOfScreen(scr));

}

/* clip rectangle nicely */
void
scrot_nice_clip(int *rx, 
		int *ry, 
		int *rw, 
		int *rh)
{
  if (*rx < 0) {
    *rw += *rx;
    *rx = 0;
  }
  if (*ry < 0) {
    *rh += *ry;
    *ry = 0;
  }
  if ((*rx + *rw) > scr->width)
    *rw = scr->width - *rx;
  if ((*ry + *rh) > scr->height)
    *rh = scr->height - *ry;
}

int
scrot_get_geometry(Window target,
		   int *rx,
		   int *ry,
		   int *rw,
		   int *rh)
{
  Window child;
  XWindowAttributes attr;
  int stat;

  /* get windowmanager frame of window */
  if (target != root) {
    unsigned int d, x;
    int status;

    status = XGetGeometry(disp, target, &root, &x, &x, &d, &d, &d, &d);
    if (status != 0) {
      Window rt, *children, parent;

      for (;;) {
	/* Find window manager frame. */
	status = XQueryTree(disp, target, &rt, &parent, &children, &d);
	if (status && (children != None))
	  XFree((char *) children);
	if (!status || (parent == None) || (parent == rt))
	  break;
	target = parent;
      }
      XRaiseWindow(disp, target);
    }
  }
  stat = XGetWindowAttributes(disp, target, &attr);
  if ((stat == False) || (attr.map_state != IsViewable))
    return 0;
  *rw = attr.width;
  *rh = attr.height;
  XTranslateCoordinates(disp, target, root, 0, 0, rx, ry, &child);
  return 1;
}

Window
scrot_get_window(Display * display,
                 Window window,
                 int x,
                 int y)
{
  Window source, target;

  int status, x_offset, y_offset;

  source = root;
  target = window;
  if (window == None)
    window = root;
  while (1) {
    status =
      XTranslateCoordinates(display, source, window, x, y, &x_offset,
                            &y_offset, &target);
    if (status != True)
      break;
    if (target == None)
      break;
    source = window;
    window = target;
    x = x_offset;
    y = y_offset;
  }
  if (target == None)
    target = window;
  return (target);
}

int main() {
  static int xfd = 0;
  static int fdsize = 0;
  XEvent ev;
  fd_set fdset;
  int count = 0, done = 0;
  int rx = 0, ry = 0, rw = 0, rh = 0, btn_pressed = 0;
  int rect_x = 0, rect_y = 0, rect_w = 0, rect_h = 0;
  Cursor cursor, cursor2;
  Window target = None;
  GC gc;
  XGCValues gcval;
  
  init_x(NULL, 0);

  xfd = ConnectionNumber(disp);
  fdsize = xfd + 1;

  cursor = XCreateFontCursor(disp, XC_left_ptr);
  cursor2 = XCreateFontCursor(disp, XC_lr_angle);

  gcval.foreground = XWhitePixel(disp, 0);
  gcval.function = GXxor;
  gcval.background = XBlackPixel(disp, 0);
  gcval.plane_mask = gcval.background ^ gcval.foreground;
  gcval.subwindow_mode = IncludeInferiors;

  gc =
    XCreateGC(disp, root,
              GCFunction | GCForeground | GCBackground | GCSubwindowMode,
              &gcval);

  if ((XGrabPointer
       (disp, root, False,
        ButtonMotionMask | ButtonPressMask | ButtonReleaseMask, GrabModeAsync,
        GrabModeAsync, root, cursor, CurrentTime) != GrabSuccess))
            printf("couldn't grab pointer:");

  if ((XGrabKeyboard
       (disp, root, False, GrabModeAsync, GrabModeAsync,
        CurrentTime) != GrabSuccess))
            printf("couldn't grab keyboard:");


  while (1) {
    /* handle events here */
    while (!done && XPending(disp)) {
      XNextEvent(disp, &ev);
      switch (ev.type) {
        case MotionNotify:
          if (btn_pressed) {
            if (rect_w) {
              /* re-draw the last rect to clear it */
              XDrawRectangle(disp, root, gc, rect_x, rect_y, rect_w, rect_h);
            } else {
              /* Change the cursor to show we're selecting a region */
              XChangeActivePointerGrab(disp,
                                       ButtonMotionMask | ButtonReleaseMask,
                                       cursor2, CurrentTime);
            }

            rect_x = rx;
            rect_y = ry;
            rect_w = ev.xmotion.x - rect_x;
            rect_h = ev.xmotion.y - rect_y;

            if (rect_w < 0) {
              rect_x += rect_w;
              rect_w = 0 - rect_w;
            }
            if (rect_h < 0) {
              rect_y += rect_h;
              rect_h = 0 - rect_h;
            }
            /* draw rectangle */
            XDrawRectangle(disp, root, gc, rect_x, rect_y, rect_w, rect_h);
            XFlush(disp);
          }
          break;
        case ButtonPress:
          btn_pressed = 1;
          rx = ev.xbutton.x;
          ry = ev.xbutton.y;
          target =
            scrot_get_window(disp, ev.xbutton.subwindow, ev.xbutton.x,
                             ev.xbutton.y);
          if (target == None)
            target = root;
          break;
        case ButtonRelease:
          done = 1;
          break;
        case KeyPress:
          fprintf(stderr, "Key was pressed, aborting shot\n");
          done = 2;
          break;
        case KeyRelease:
          /* ignore */
          break;
        default:
          break;
      }
    }
    if (done)
      break;

    /* now block some */
    FD_ZERO(&fdset);
    FD_SET(xfd, &fdset);
    errno = 0;
    count = select(fdsize, &fdset, NULL, NULL, NULL);
    if ((count < 0)
        && ((errno == ENOMEM) || (errno == EINVAL) || (errno == EBADF)))
            printf("Connection to X display lost");
  }
  if (rect_w) {
    XDrawRectangle(disp, root, gc, rect_x, rect_y, rect_w, rect_h);
    XFlush(disp);
  }
  XUngrabPointer(disp, CurrentTime);
  XUngrabKeyboard(disp, CurrentTime);
  XFreeCursor(disp, cursor);
  XFreeGC(disp, gc);
  XSync(disp, True);


  if (done < 2) {
    if (rect_w > 5) {
      /* if a rect has been drawn, it's an area selection */
      rw = ev.xbutton.x - rx;
      rh = ev.xbutton.y - ry;

      if (rw < 0) {
        rx += rw;
        rw = 0 - rw;
      }
      if (rh < 0) {
        ry += rh;
        rh = 0 - rh;
      }
    } 
    scrot_nice_clip(&rx, &ry, &rw, &rh);
    printf("%d %d %d %d\n", rx,ry,rw,rh);
  }
  return 0;
}
