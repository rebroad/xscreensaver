/* xscreensaver-tray, Copyright (c) 2026
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 */

#include "utils.h"
#include "atoms.h"
#include "blurb.h"
#include "resources.h"
#include "minixpm.h"

#include <X11/Xatom.h>
#include <X11/extensions/shape.h>
#include <systemd/sd-bus.h>

#include <errno.h>
#include <poll.h>
#include <stdint.h>

#include "images/logo-50.xpm"

#define SYSTEM_TRAY_REQUEST_DOCK 0

#define XEMBED_EMBEDDED_NOTIFY 0

static Bool debug_p = False;
static Bool test_p = False;
static Bool force_xembed_p = False;

static sd_bus *sni_bus = 0;
static char sni_service[128];
static const char *sni_icon_name = "xscreensaver";
static const char *sni_icon_theme_path = "";
static unsigned int sni_icon_w = 0;
static unsigned int sni_icon_h = 0;
static unsigned char *sni_icon_argb = 0;
static size_t sni_icon_argb_len = 0;

static void
usage (const char *prog)
{
  fprintf (stderr,
           "usage: %s [--verbose] [--debug] [--test] [--xembed]\n",
           prog);
  exit (1);
}

static Bool
get_saver_status (Display *dpy, Bool *blanked_p, Bool *locked_p)
{
  Window root = RootWindow (dpy, 0);
  Atom type;
  unsigned char *data = 0;
  PROP32 *status = 0;
  int format;
  unsigned long nitems, bytesafter;
  Bool ok = False;

  if (XGetWindowProperty (dpy, root,
                          XA_SCREENSAVER_STATUS,
                          0, 3, False, XA_INTEGER,
                          &type, &format, &nitems, &bytesafter,
                          &data)
      == Success
      && type == XA_INTEGER
      && nitems >= 1
      && data)
    {
      status = (PROP32 *) data;
      if (blanked_p) *blanked_p = (status[0] & 0x01) ? True : False;
      if (locked_p)  *locked_p  = (status[0] & 0x02) ? True : False;
      ok = True;
    }

  if (data) XFree (data);
  return ok;
}

static Bool
tray_should_be_visible (Display *dpy)
{
  Bool blanked_p = False;
  Bool locked_p = False;

  if (! get_saver_status (dpy, &blanked_p, &locked_p))
    return False;

  return (locked_p && !blanked_p);
}

static Window
get_tray_owner (Display *dpy)
{
  int screen = DefaultScreen (dpy);
  Atom tray_sel;
  char name[64];

  sprintf (name, "_NET_SYSTEM_TRAY_S%d", screen);
  tray_sel = XInternAtom (dpy, name, False);
  return XGetSelectionOwner (dpy, tray_sel);
}

static Bool
dock_to_tray (Display *dpy, Window win)
{
  Window tray = get_tray_owner (dpy);
  XClientMessageEvent ev;
  Atom opcode = XInternAtom (dpy, "_NET_SYSTEM_TRAY_OPCODE", False);
  Atom xembed_info = XInternAtom (dpy, "_XEMBED_INFO", False);
  long info[2];

  if (! tray)
    return False;

  info[0] = 0;   /* XEMBED protocol version */
  info[1] = 0;   /* flags */
  XChangeProperty (dpy, win, xembed_info, xembed_info, 32,
                   PropModeReplace, (unsigned char *) info, 2);

  memset (&ev, 0, sizeof (ev));
  ev.type = ClientMessage;
  ev.window = tray;
  ev.message_type = opcode;
  ev.format = 32;
  ev.data.l[0] = CurrentTime;
  ev.data.l[1] = SYSTEM_TRAY_REQUEST_DOCK;
  ev.data.l[2] = win;

  XSendEvent (dpy, tray, False, NoEventMask, (XEvent *) &ev);
  XSync (dpy, False);
  return True;
}

static void
draw_icon (Display *dpy, Window win, Pixmap logo, GC gc)
{
  Window root;
  int x, y;
  unsigned int w, h, bw, depth;

  if (! logo) return;

  XGetGeometry (dpy, logo, &root, &x, &y, &w, &h, &bw, &depth);
  XCopyArea (dpy, logo, win, gc, 0, 0, w, h, 0, 0);
}

static int
sni_property_category (sd_bus *bus, const char *path, const char *interface,
                       const char *property, sd_bus_message *reply,
                       void *userdata, sd_bus_error *ret_error)
{
  return sd_bus_message_append (reply, "s", "SystemServices");
}

static int
sni_property_id (sd_bus *bus, const char *path, const char *interface,
                 const char *property, sd_bus_message *reply,
                 void *userdata, sd_bus_error *ret_error)
{
  return sd_bus_message_append (reply, "s", "xscreensaver");
}

static int
sni_property_title (sd_bus *bus, const char *path, const char *interface,
                    const char *property, sd_bus_message *reply,
                    void *userdata, sd_bus_error *ret_error)
{
  return sd_bus_message_append (reply, "s", "XScreenSaver");
}

static int
sni_property_status (sd_bus *bus, const char *path, const char *interface,
                     const char *property, sd_bus_message *reply,
                     void *userdata, sd_bus_error *ret_error)
{
  return sd_bus_message_append (reply, "s", "Active");
}

static int
sni_property_menu (sd_bus *bus, const char *path, const char *interface,
                   const char *property, sd_bus_message *reply,
                   void *userdata, sd_bus_error *ret_error)
{
  return sd_bus_message_append (reply, "o", "/StatusNotifierItem/Menu");
}

static int
sni_property_bool (sd_bus *bus, const char *path, const char *interface,
                   const char *property, sd_bus_message *reply,
                   void *userdata, sd_bus_error *ret_error)
{
  return sd_bus_message_append (reply, "b", 0);
}

static int
sni_method_nop (sd_bus_message *m, void *userdata, sd_bus_error *ret_error)
{
  return sd_bus_reply_method_return (m, "");
}

static int
sni_property_icon_name (sd_bus *bus, const char *path, const char *interface,
                        const char *property, sd_bus_message *reply,
                        void *userdata, sd_bus_error *ret_error)
{
  return sd_bus_message_append (reply, "s", sni_icon_name);
}

static int
sni_property_icon_theme_path (sd_bus *bus, const char *path, const char *interface,
                              const char *property, sd_bus_message *reply,
                              void *userdata, sd_bus_error *ret_error)
{
  return sd_bus_message_append (reply, "s", sni_icon_theme_path);
}

static int
sni_property_icon_pixmap (sd_bus *bus, const char *path, const char *interface,
                          const char *property, sd_bus_message *reply,
                          void *userdata, sd_bus_error *ret_error)
{
  int rc;

  if (! sni_icon_argb || sni_icon_w == 0 || sni_icon_h == 0)
    return sd_bus_message_append (reply, "a(iiay)");

  rc = sd_bus_message_open_container (reply, 'a', "(iiay)");
  if (rc < 0) return rc;
  rc = sd_bus_message_open_container (reply, 'r', "iiay");
  if (rc < 0) return rc;

  rc = sd_bus_message_append (reply, "ii", (int) sni_icon_w, (int) sni_icon_h);
  if (rc < 0) return rc;
  rc = sd_bus_message_append_array (reply, 'y', sni_icon_argb, sni_icon_argb_len);
  if (rc < 0) return rc;

  rc = sd_bus_message_close_container (reply);
  if (rc < 0) return rc;
  return sd_bus_message_close_container (reply);
}

static const sd_bus_vtable sni_vtable[] = {
  SD_BUS_VTABLE_START (0),
  SD_BUS_PROPERTY ("Category", "s", sni_property_category, 0, SD_BUS_VTABLE_PROPERTY_CONST),
  SD_BUS_PROPERTY ("Id", "s", sni_property_id, 0, SD_BUS_VTABLE_PROPERTY_CONST),
  SD_BUS_PROPERTY ("Title", "s", sni_property_title, 0, SD_BUS_VTABLE_PROPERTY_CONST),
  SD_BUS_PROPERTY ("Status", "s", sni_property_status, 0, SD_BUS_VTABLE_PROPERTY_CONST),
  SD_BUS_PROPERTY ("IconName", "s", sni_property_icon_name, 0, SD_BUS_VTABLE_PROPERTY_CONST),
  SD_BUS_PROPERTY ("IconThemePath", "s", sni_property_icon_theme_path, 0, SD_BUS_VTABLE_PROPERTY_CONST),
  SD_BUS_PROPERTY ("IconPixmap", "a(iiay)", sni_property_icon_pixmap, 0, SD_BUS_VTABLE_PROPERTY_CONST),
  SD_BUS_PROPERTY ("ItemIsMenu", "b", sni_property_bool, 0, SD_BUS_VTABLE_PROPERTY_CONST),
  SD_BUS_PROPERTY ("Menu", "o", sni_property_menu, 0, SD_BUS_VTABLE_PROPERTY_CONST),
  SD_BUS_METHOD ("Activate", "ii", "", sni_method_nop, SD_BUS_VTABLE_UNPRIVILEGED),
  SD_BUS_METHOD ("SecondaryActivate", "ii", "", sni_method_nop, SD_BUS_VTABLE_UNPRIVILEGED),
  SD_BUS_METHOD ("ContextMenu", "ii", "", sni_method_nop, SD_BUS_VTABLE_UNPRIVILEGED),
  SD_BUS_METHOD ("Scroll", "is", "", sni_method_nop, SD_BUS_VTABLE_UNPRIVILEGED),
  SD_BUS_VTABLE_END
};

static int
sni_init (void)
{
  int rc;
  sd_bus_error error = SD_BUS_ERROR_NULL;

  rc = sd_bus_default_user (&sni_bus);
  if (rc < 0)
    return rc;

  snprintf (sni_service, sizeof (sni_service),
            "org.kde.StatusNotifierItem-%lu", (unsigned long) getpid());

  rc = sd_bus_add_object_vtable (sni_bus, NULL,
                                 "/StatusNotifierItem",
                                 "org.kde.StatusNotifierItem",
                                 sni_vtable, NULL);
  if (rc < 0)
    return rc;

  rc = sd_bus_request_name (sni_bus, sni_service, 0);
  if (rc < 0)
    return rc;

  rc = sd_bus_call_method (sni_bus,
                           "org.kde.StatusNotifierWatcher",
                           "/StatusNotifierWatcher",
                           "org.kde.StatusNotifierWatcher",
                           "RegisterStatusNotifierItem",
                           &error, NULL, "s", sni_service);
  if (rc < 0)
    {
      if (debug_p)
        fprintf (stderr, "xscreensaver-tray: SNI register failed: %s\n",
                 error.message ? error.message : strerror (-rc));
      sd_bus_error_free (&error);
      return rc;
    }

  if (debug_p)
    fprintf (stderr, "xscreensaver-tray: SNI registered as %s\n", sni_service);

  sd_bus_flush (sni_bus);
  return 0;
}

static int
sni_loop (Display *dpy)
{
  int xfd = ConnectionNumber (dpy);
  int bfd = sd_bus_get_fd (sni_bus);

  XSelectInput (dpy, RootWindow (dpy, 0), PropertyChangeMask);

  while (1)
    {
      struct pollfd fds[2];
      int rc;

      fds[0].fd = xfd;
      fds[0].events = POLLIN;
      fds[0].revents = 0;
      fds[1].fd = bfd;
      fds[1].events = POLLIN;
      fds[1].revents = 0;

      rc = poll (fds, 2, -1);
      if (rc < 0)
        {
          if (errno == EINTR) continue;
          return 1;
        }

      if (fds[1].revents)
        {
          for (;;)
            {
              rc = sd_bus_process (sni_bus, NULL);
              if (rc <= 0) break;
            }
        }

      if (fds[0].revents)
        {
          XEvent event;
          while (XPending (dpy))
            {
              XNextEvent (dpy, &event);
              if (event.type == PropertyNotify &&
                  event.xproperty.atom == XA_SCREENSAVER_STATUS)
                {
                  if (! test_p && ! tray_should_be_visible (dpy))
                    return 0;
                }
            }
        }
    }
}

static void
sni_build_icon (Display *dpy, Visual *visual, Colormap cmap)
{
  XImage *image = 0;
  unsigned char *mask = 0;
  unsigned int w = 0, h = 0;
  unsigned int x, y;
  unsigned int mask_stride;

  {
    unsigned long *pixels = 0;
    int npixels = 0;
    image = minixpm_to_ximage (dpy, visual, cmap,
                               DefaultDepth (dpy, 0),
                               BlackPixel (dpy, 0),
                               logo_50_xpm, &w, &h,
                               &pixels, &npixels,
                               &mask);
  }
  if (! image || w == 0 || h == 0)
    {
      if (image) XDestroyImage (image);
      if (mask) free (mask);
      return;
    }

  sni_icon_w = w;
  sni_icon_h = h;
  sni_icon_argb_len = w * h * 4;
  sni_icon_argb = (unsigned char *) malloc (sni_icon_argb_len);
  if (! sni_icon_argb)
    {
      XDestroyImage (image);
      if (mask) free (mask);
      sni_icon_w = sni_icon_h = 0;
      sni_icon_argb_len = 0;
      return;
    }

  mask_stride = (w + 7) / 8;

  for (y = 0; y < h; y++)
    for (x = 0; x < w; x++)
      {
        unsigned long pixel = XGetPixel (image, x, y);
        XColor c;
        unsigned char a = 255;
        size_t i = (y * w + x) * 4;

        c.pixel = pixel;
        XQueryColor (dpy, cmap, &c);

        if (mask)
          {
            unsigned int mi = y * mask_stride + (x >> 3);
            unsigned char mb = mask[mi];
            unsigned char bit = (mb >> (x & 7)) & 1;
            if (! bit) a = 0;
          }

        sni_icon_argb[i + 0] = a;
        sni_icon_argb[i + 1] = (unsigned char) (c.red   >> 8);
        sni_icon_argb[i + 2] = (unsigned char) (c.green >> 8);
        sni_icon_argb[i + 3] = (unsigned char) (c.blue  >> 8);
      }

  XDestroyImage (image);
  if (mask) free (mask);
}

int
main (int argc, char **argv)
{
  Display *dpy;
  Window root, win;
  Visual *visual;
  Colormap cmap;
  Pixmap logo = 0, mask = 0;
  GC gc;
  XGCValues gcv;
  XEvent event;
  int i;
  Atom xembed;
  Bool use_xembed_p = True;

  progname = argv[0];
  {
    const char *s = strrchr (progname, '/');
    if (s) progname = s + 1;
  }

  for (i = 1; i < argc; i++)
    {
      const char *arg = argv[i];
      if (!strcmp (arg, "--verbose"))
        verbose_p++;
      else if (!strcmp (arg, "--debug"))
        debug_p = True;
      else if (!strcmp (arg, "--test"))
        test_p = True;
      else if (!strcmp (arg, "--xembed"))
        force_xembed_p = True;
      else
        usage (argv[0]);
    }

  dpy = XOpenDisplay (0);
  if (! dpy)
    {
      fprintf (stderr, "%s: unable to open display\n", argv[0]);
      return 1;
    }

  init_xscreensaver_atoms (dpy);
  xembed = XInternAtom (dpy, "_XEMBED", False);

  if (! test_p && ! tray_should_be_visible (dpy))
    {
      if (debug_p)
        fprintf (stderr, "xscreensaver-tray: not locked (or already blanked)\n");
      return 0;
    }

  if (test_p)
    {
      sni_icon_name = "logo-50";
      sni_icon_theme_path = "../hacks/images";
    }

  if (! force_xembed_p)
    {
      sni_build_icon (dpy, DefaultVisual (dpy, 0), DefaultColormap (dpy, 0));
      int rc = sni_init ();
      if (rc == 0)
        use_xembed_p = False;
      else if (debug_p)
        fprintf (stderr, "xscreensaver-tray: SNI unavailable (%s), using XEmbed\n",
                 strerror (-rc));
    }

  if (! use_xembed_p)
    return sni_loop (dpy);

  root = RootWindow (dpy, 0);
  visual = DefaultVisual (dpy, 0);
  cmap = DefaultColormap (dpy, 0);

  {
    unsigned long *pixels = 0;
    int npixels = 0;
    logo = xscreensaver_logo (ScreenOfDisplay (dpy, 0), visual, root, cmap,
                              BlackPixel (dpy, 0), &pixels, &npixels, &mask, 0);
  }
  if (! logo)
    return 1;

  {
    Window root2;
    int x, y;
    unsigned int w, h, bw, depth;
    XGetGeometry (dpy, logo, &root2, &x, &y, &w, &h, &bw, &depth);
    win = XCreateSimpleWindow (dpy, root, 0, 0, w, h, 0,
                               BlackPixel (dpy, 0), BlackPixel (dpy, 0));
  }

  XSelectInput (dpy, root, PropertyChangeMask);
  XSelectInput (dpy, win, ExposureMask | StructureNotifyMask);

  if (mask)
    XShapeCombineMask (dpy, win, ShapeBounding, 0, 0, mask, ShapeSet);

  gc = XCreateGC (dpy, win, 0, &gcv);

  if (! dock_to_tray (dpy, win))
    {
      if (debug_p)
        fprintf (stderr, "xscreensaver-tray: no system tray owner\n");
      return 0;
    }

  XMapRaised (dpy, win);
  XSync (dpy, False);
  draw_icon (dpy, win, logo, gc);

  while (1)
    {
      XNextEvent (dpy, &event);
      switch (event.type)
        {
        case Expose:
          if (event.xexpose.count == 0)
            draw_icon (dpy, win, logo, gc);
          break;
        case PropertyNotify:
          if (event.xproperty.atom == XA_SCREENSAVER_STATUS)
            {
              if (! test_p && ! tray_should_be_visible (dpy))
                return 0;
            }
          break;
        case ClientMessage:
          if (event.xclient.message_type == xembed &&
              event.xclient.data.l[1] == XEMBED_EMBEDDED_NOTIFY)
            {
              if (verbose_p)
                fprintf (stderr, "embedded in system tray\n");
            }
          break;
        case DestroyNotify:
          return 0;
        default:
          break;
        }
    }

  return 0;
}
