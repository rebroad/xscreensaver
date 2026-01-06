/* xscreensaver, Copyright © 2025 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 */

/* desktop-preview -- displays a live preview of the desktop
   in the given window.  Used by xscreensaver-settings for "Lock Only" mode
   preview.

   This program grabs a screenshot of the desktop and displays it in the
   given window, updating periodically to show the current desktop state.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "screenhack.h"

struct state {
  Display *dpy;
  Window window;
  Screen *screen;
  Window root;
  Pixmap screenshot;
  GC gc;
  int width, height;
  int root_width, root_height;
  time_t last_update;
  int update_interval;  /* seconds */
};

static void *
desktop_preview_init (Display *dpy, Window window)
{
  struct state *st = (struct state *) calloc (1, sizeof(*st));
  XGCValues gcv;
  XWindowAttributes xgwa;
  XWindowAttributes root_xgwa;

  st->dpy = dpy;
  st->window = window;

  XGetWindowAttributes (dpy, window, &xgwa);
  st->screen = xgwa.screen;
  st->root = XRootWindowOfScreen (st->screen);
  st->width = xgwa.width;
  st->height = xgwa.height;

  XGetWindowAttributes (dpy, st->root, &root_xgwa);
  st->root_width = root_xgwa.width;
  st->root_height = root_xgwa.height;

  st->update_interval = get_integer_resource (dpy, "updateInterval", "Integer");
  if (st->update_interval < 1) st->update_interval = 2;  /* Default 2 seconds */

  st->last_update = 0;
  st->screenshot = None;

  gcv.function = GXcopy;
  gcv.subwindow_mode = IncludeInferiors;
  st->gc = XCreateGC (dpy, window, GCFunction | GCSubwindowMode, &gcv);

  return st;
}

static void
grab_and_display_screenshot (struct state *st)
{
  XWindowAttributes xgwa, root_xgwa;
  XGCValues gcv;

  XGetWindowAttributes (st->dpy, st->window, &xgwa);
  XGetWindowAttributes (st->dpy, st->root, &root_xgwa);

  st->width = xgwa.width;
  st->height = xgwa.height;
  st->root_width = root_xgwa.width;
  st->root_height = root_xgwa.height;

  /* Free old screenshot */
  if (st->screenshot != None)
    {
      XFreePixmap (st->dpy, st->screenshot);
      st->screenshot = None;
    }

  /* Create pixmap for full desktop screenshot */
  st->screenshot = XCreatePixmap (st->dpy, st->root,
                                  st->root_width, st->root_height,
                                  root_xgwa.depth);

  if (st->screenshot != None)
    {
      gcv.function = GXcopy;
      gcv.subwindow_mode = IncludeInferiors;
      GC root_gc = XCreateGC (st->dpy, st->screenshot,
                             GCFunction | GCSubwindowMode, &gcv);

      /* Copy full root window to pixmap */
      XCopyArea (st->dpy, st->root, st->screenshot, root_gc,
                0, 0, st->root_width, st->root_height, 0, 0);
      XFreeGC (st->dpy, root_gc);

      /* Scale and copy to preview window */
      /* Note: XCopyArea doesn't scale - we copy what fits */
      {
        unsigned int copy_w = (st->root_width < st->width) ?
          st->root_width : st->width;
        unsigned int copy_h = (st->root_height < st->height) ?
          st->root_height : st->height;
        XCopyArea (st->dpy, st->screenshot, st->window, st->gc,
                  0, 0, copy_w, copy_h, 0, 0);
      }
      XSync (st->dpy, False);
    }
}

static unsigned long
desktop_preview_draw (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  time_t now = time ((time_t *) 0);

  /* Update screenshot if interval has passed */
  if (st->last_update == 0 || (now - st->last_update) >= st->update_interval)
    {
      grab_and_display_screenshot (st);
      st->last_update = now;
    }

  /* Return delay in microseconds until next draw call */
  return st->update_interval * 1000000;
}

static void
desktop_preview_reshape (Display *dpy, Window window, void *closure,
                         unsigned int w, unsigned int h)
{
  struct state *st = (struct state *) closure;
  /* Window size changed, update screenshot */
  grab_and_display_screenshot (st);
}

static Bool
desktop_preview_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  struct state *st = (struct state *) closure;
  if (screenhack_event_helper (dpy, window, event))
    {
      /* Force update on user interaction */
      st->last_update = 0;
      return True;
    }
  return False;
}

static void
desktop_preview_free (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  if (st->gc) XFreeGC (dpy, st->gc);
  if (st->screenshot != None) XFreePixmap (dpy, st->screenshot);
  free (st);
}

static const char *desktop_preview_defaults [] = {
  ".background:		black",
  ".foreground:		white",
  "*dontClearRoot:	True",
  "*fpsSolid:		true",
  "*updateInterval:	2",
  0
};

static XrmOptionDescRec desktop_preview_options [] = {
  { "-update-interval",	".updateInterval",	XrmoptionSepArg, 0 },
  { 0, 0, 0, 0 }
};

XSCREENSAVER_MODULE ("Desktop Preview", desktop_preview)
