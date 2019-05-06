/* See LICENSE file for license details. */
#define _XOPEN_SOURCE 500

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/dpms.h>
#include <X11/keysym.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/vt.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>
#include "hotp.c"


static void die(const char *errstr, ...) {
  va_list ap;

  va_start(ap, errstr);
  vfprintf(stderr, errstr, ap);
  va_end(ap);
  exit(EXIT_FAILURE);
}

int main(int argc, char **argv) {
  // HOTP
  struct HotpRuntime runtime;
  struct HotpData data;

  int res = hotpLoadDataPath("./hotpSettings", &data);
  if (res < 0) {
    die("[err] cannot load hotp data from file\n");
  }

  res = hotpInitRuntime(&runtime, &data);
  if (res < 0) {
    die("[err] cannot init hmac runtime\n");
  }

  char curs[] = {0, 0, 0, 0, 0, 0, 0, 0};
  char buf[32], passwd[256], passdisp[256];
  int num, screen, width, height, update, sleepmode, pid;

  unsigned int len;
  Bool running = True;
  Cursor invisible;
  Display *dpy;
  KeySym ksym;
  Pixmap pmap;
  Window root, w;
  XColor black, red, dummy;
  XEvent ev;
  XSetWindowAttributes wa;
  XFontStruct *font;
  GC gc;
  XGCValues values;

  // defaults
  char *passchar = "*";
  char *fontname = "-*-dejavu sans-bold-r-*-*-*-420-100-100-*-*-iso8859-1";
  char *username = "... HOTP ...";
  int showline = 1;
  int xshift = 0;

  for (int i = 0; i < argc; i++) {
    if (!strcmp(argv[i], "-c")) {
      if (i + 1 < argc)
        passchar = argv[i + 1];
      else
        die("error: no password character given.\n");
    } else if (!strcmp(argv[i], "-f")) {
      if (i + 1 < argc)
        fontname = argv[i + 1];
      else
        die("error: font not specified.\n");
    } else if (!strcmp(argv[i], "-v"))
      die("sflock-" VERSION ", © 2015 Ben Ruijl\n");
    else if (!strcmp(argv[i], "-h"))
      showline = 0;
    else if (!strcmp(argv[i], "-xshift")) {
      if (i + 1 == argc)
        die("error: missing xshift value\n");
      xshift = atoi(argv[i + 1]);
    } else if (!strcmp(argv[i], "?"))
      die("usage: sflock [-v] [-c passchars] [-f fontname] [-xshift horizontal "
          "shift]\n");
  }

  // fill with password characters
  for (int i = 0; i < sizeof passdisp; i += strlen(passchar))
    for (int j = 0; j < strlen(passchar) && i + j < sizeof passdisp; j++)
      passdisp[i + j] = passchar[j];

  /* deamonize */
  pid = fork();
  if (pid < 0)
    die("Could not fork sflock.");
  if (pid > 0)
    exit(0); // exit parent

  if (!(dpy = XOpenDisplay(0)))
    die("sflock: cannot open dpy\n");

  screen = DefaultScreen(dpy);
  root = RootWindow(dpy, screen);
  width = DisplayWidth(dpy, screen);
  height = DisplayHeight(dpy, screen);

  wa.override_redirect = 1;
  wa.background_pixel = XBlackPixel(dpy, screen);
  w = XCreateWindow(dpy, root, 0, 0, width, height, 0,
                    DefaultDepth(dpy, screen), CopyFromParent,
                    DefaultVisual(dpy, screen),
                    CWOverrideRedirect | CWBackPixel, &wa);

  XAllocNamedColor(dpy, DefaultColormap(dpy, screen), "orange red", &red,
                   &dummy);
  XAllocNamedColor(dpy, DefaultColormap(dpy, screen), "black", &black, &dummy);
  pmap = XCreateBitmapFromData(dpy, w, curs, 8, 8);
  invisible = XCreatePixmapCursor(dpy, pmap, pmap, &black, &black, 0, 0);
  XDefineCursor(dpy, w, invisible);
  XMapRaised(dpy, w);

  font = XLoadQueryFont(dpy, fontname);

  if (font == 0) {
    die("error: could not find font. Try using a full description.\n");
  }

  gc = XCreateGC(dpy, w, (unsigned long)0, &values);
  XSetFont(dpy, gc, font->fid);
  XSetForeground(dpy, gc, XWhitePixel(dpy, screen));

  for (len = 1000; len; len--) {
    if (XGrabPointer(dpy, root, False,
                     ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
                     GrabModeAsync, GrabModeAsync, None, invisible,
                     CurrentTime) == GrabSuccess)
      break;
    usleep(1000);
  }
  if ((running = running && (len > 0))) {
    for (len = 1000; len; len--) {
      if (XGrabKeyboard(dpy, root, True, GrabModeAsync, GrabModeAsync,
                        CurrentTime) == GrabSuccess)
        break;
      usleep(1000);
    }
    running = (len > 0);
  }

  len = 0;
  XSync(dpy, False);
  update = True;
  sleepmode = False;

  /* main event loop */
  while (running && !XNextEvent(dpy, &ev)) {
    if (sleepmode) {
      DPMSEnable(dpy);
      DPMSForceLevel(dpy, DPMSModeOff);
      XFlush(dpy);
    }

    if (update) {
      int x, y, dir, ascent, descent;
      XCharStruct overall;

      XClearWindow(dpy, w);
      XTextExtents(font, passdisp, len, &dir, &ascent, &descent, &overall);
      x = (width - overall.width) / 2;
      y = (height + ascent - descent) / 2;

      XDrawString(dpy, w, gc,
                  (width - XTextWidth(font, username, strlen(username))) / 2 +
                      xshift,
                  y - ascent - 20, username, strlen(username));

      if (showline)
        XDrawLine(dpy, w, gc, width * 3 / 8 + xshift, y - ascent - 10,
                  width * 5 / 8 + xshift, y - ascent - 10);

      XDrawString(dpy, w, gc, x + xshift, y, passdisp, len);
      update = False;
    }

    if (ev.type == MotionNotify) {
      sleepmode = False;
    }

    if (ev.type == KeyPress) {
      sleepmode = False;

      buf[0] = 0;
      num = XLookupString(&ev.xkey, buf, sizeof buf, &ksym, 0);
      if (IsKeypadKey(ksym)) {
        if (ksym == XK_KP_Enter)
          ksym = XK_Return;
        else if (ksym >= XK_KP_0 && ksym <= XK_KP_9)
          ksym = (ksym - XK_KP_0) + XK_0;
      }
      if (IsFunctionKey(ksym) || IsKeypadKey(ksym) || IsMiscFunctionKey(ksym) ||
          IsPFKey(ksym) || IsPrivateKeypadKey(ksym))
        continue;

      switch (ksym) {
      case XK_Return:
        hotpCalculate(&runtime, &data);
        int eq = strncmp(runtime.value, passwd, len);
        passwd[len] = 0;
        
        if (eq == 0) {
          running = 0;
        }

        if (running != 0)
          // change background on wrong password
          XSetWindowBackground(dpy, w, red.pixel);
        len = 0;
        break;
      case XK_Escape:
        len = 0;

        if (DPMSCapable(dpy)) {
          sleepmode = True;
        }

        break;
      case XK_BackSpace:
        if (len)
          --len;
        break;
      default:
        if (num && !iscntrl((int)buf[0]) && (len + num < sizeof passwd)) {
          memcpy(passwd + len, buf, num);
          len += num;
        }

        break;
      }

      update = True; // show changes
    }
  }

  XUngrabPointer(dpy, CurrentTime);
  XFreePixmap(dpy, pmap);
  XFreeFont(dpy, font);
  XFreeGC(dpy, gc);
  XDestroyWindow(dpy, w);
  XCloseDisplay(dpy);
  return 0;
}
