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
  XImage *src_image = NULL, *dst_image = NULL;
  double scale_x, scale_y, scale;
  int scaled_width, scaled_height;
  int offset_x, offset_y;
  int x, y;

  XGetWindowAttributes (st->dpy, st->window, &xgwa);
  XGetWindowAttributes (st->dpy, st->root, &root_xgwa);

  st->width = xgwa.width;
  st->height = xgwa.height;
  st->root_width = root_xgwa.width;
  st->root_height = root_xgwa.height;

  /* Calculate scale factor to fit desktop into window while maintaining aspect ratio */
  scale_x = (double) st->width / st->root_width;
  scale_y = (double) st->height / st->root_height;
  scale = (scale_x < scale_y) ? scale_x : scale_y;

  scaled_width = (int) (st->root_width * scale);
  scaled_height = (int) (st->root_height * scale);
  offset_x = (st->width - scaled_width) / 2;
  offset_y = (st->height - scaled_height) / 2;

  /* Get the root window image */
  src_image = XGetImage (st->dpy, st->root, 0, 0,
                        st->root_width, st->root_height,
                        AllPlanes, ZPixmap);
  if (!src_image)
    {
      XSync (st->dpy, False);
      return;
    }

  /* Create destination image for scaled version */
  dst_image = XCreateImage (st->dpy, xgwa.visual, xgwa.depth,
                            ZPixmap, 0, NULL, scaled_width, scaled_height,
                            xgwa.depth == 1 ? 1 : 32, 0);
  if (!dst_image)
    {
      XDestroyImage (src_image);
      XSync (st->dpy, False);
      return;
    }

  dst_image->data = (char *) calloc (dst_image->bytes_per_line * scaled_height, 1);
  if (!dst_image->data)
    {
      XDestroyImage (src_image);
      XDestroyImage (dst_image);
      XSync (st->dpy, False);
      return;
    }

  /* Scale the image using nearest-neighbor interpolation */
  for (y = 0; y < scaled_height; y++)
    {
      int src_y = (int) (y / scale);
      if (src_y >= st->root_height) src_y = st->root_height - 1;

      for (x = 0; x < scaled_width; x++)
        {
          int src_x = (int) (x / scale);
          if (src_x >= st->root_width) src_x = st->root_width - 1;

          unsigned long pixel = XGetPixel (src_image, src_x, src_y);
          XPutPixel (dst_image, x, y, pixel);
        }
    }

  /* Put the scaled image into the window */
  XPutImage (st->dpy, st->window, st->gc, dst_image,
            0, 0, offset_x, offset_y, scaled_width, scaled_height);

  /* Clear border areas if image doesn't fill the window */
  if (offset_x > 0 || offset_y > 0 ||
      scaled_width < st->width || scaled_height < st->height)
    {
      XSetForeground (st->dpy, st->gc, BlackPixelOfScreen (st->screen));
      /* Top border */
      if (offset_y > 0)
        XFillRectangle (st->dpy, st->window, st->gc, 0, 0, st->width, offset_y);
      /* Bottom border */
      if (offset_y + scaled_height < st->height)
        XFillRectangle (st->dpy, st->window, st->gc, 0, offset_y + scaled_height,
                       st->width, st->height - (offset_y + scaled_height));
      /* Left border */
      if (offset_x > 0)
        XFillRectangle (st->dpy, st->window, st->gc, 0, offset_y, offset_x, scaled_height);
      /* Right border */
      if (offset_x + scaled_width < st->width)
        XFillRectangle (st->dpy, st->window, st->gc, offset_x + scaled_width, offset_y,
                       st->width - (offset_x + scaled_width), scaled_height);
    }

  /* Clean up */
  XDestroyImage (src_image);
  if (dst_image)
    {
      if (dst_image->data)
        {
          free (dst_image->data);
          dst_image->data = NULL;
        }
      XDestroyImage (dst_image);
    }

  XSync (st->dpy, False);
}

static unsigned long
desktop_preview_draw (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;

  /* Update screenshot on every draw call to match desktop frame rate */
  grab_and_display_screenshot (st);

  /* Return a small delay to allow other events to be processed */
  return 16666;  /* ~60 FPS */
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
  (void) closure;  /* unused */
  if (screenhack_event_helper (dpy, window, event))
    {
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
  0
};

static XrmOptionDescRec desktop_preview_options [] = {
  { 0, 0, 0, 0 }
};

XSCREENSAVER_MODULE ("Desktop Preview", desktop_preview)
