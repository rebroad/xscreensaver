/* xscreensaver, Copyright © 1992-2025 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 */

/* There are several different mechanisms here for fading the desktop to
   black, and then fading it back in again.

   - Colormaps: This only works on 8-bit displays, which basically haven't
     existed since the 90s.  It takes the current colormap, makes a writable
     copy of it, and then animates the color cells to fade and unfade.

   - XF86 Gamma or RANDR Gamma: These do the fade by altering the brightness
     settings of the screen.  This works on any system that has the "XF86
     Video-Mode" extension (which is every modern system) AND ALSO has gamma
     support in the video driver.  But it turns out that as of 2022, the
     Raspberry Pi HDMI video driver still does not support gamma.  And there's
     no way to determine that the video driver lacks gamma support even though
     the extension exists.  Since the Pi is probably the single most popular
     platform for running X11 on the desktop these days, that makes this
     method pretty much useless now.

   - SGI VC: Same as the above, but only works on SGI.

   - XSHM: This works by taking a screenshot and hacking the bits by hand.
     It's slow.  Also, in order to fade in from black to the desktop (possibly
     hours after it faded out) it has to retain that first screenshot of the
     desktop to fade back to.  But if the desktop had changed in the meantime,
     there will be a glitch at the end as it snaps from the screenshot to the
     new current reality.  And under Wayland, it has to get that screenshot
     by running "xscreensaver-getimage" which in turn runs "grim", if it is
     installed, which it might not be.

   - OpenGL: This is like XSHM but uses OpenGL for rendering.  Faster, but
     same screenshot- and Wayland-related downsides.

   In summary, everything is terrible because X11 doesn't support alpha.


   The fade process goes like this:

     Screen saver activates:

       - Desktop is visible
       - Save screenshot for later
       - Fade out:
         - Map invisible temp windows
         - Fade from desktop to black
         - Erase saver windows to black and raise them
         - Destroy temp windows

     Screen saver deactivates:

       - Saver graphics are visible
       - Fade out:
         - Get a screenshot of the current screenhack
         - Map invisible temp windows
         - Fade from screenhack screenshot to black
       - Fade in:
         - Fade from black to saved screenshot of desktop
         - Unmap obscured saver windows
         - Destroy temp windows
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <sys/time.h>

#ifdef HAVE_SYS_WAIT_H
# include <sys/wait.h>		/* for waitpid() and associated macros */
#endif

#ifdef HAVE_JWXYZ
# include "jwxyz.h"
#else /* real X11 */
# include <X11/Xlib.h>
# include <X11/Xatom.h>
# include <X11/Xproto.h>
# include <X11/Intrinsic.h>
#endif /* !HAVE_JWXYZ */

#ifdef USE_GL
# include <GL/gl.h>
# include <GL/glu.h>
# ifdef HAVE_EGL
#  include <EGL/egl.h>
#  include <EGL/eglext.h>
# else
#  include <GL/glx.h>
# endif
#endif /* USE_GL */

#include "blurb.h"
#include "visual.h"
#ifdef USE_GL
#include "visual-gl.h"
#endif
#include "usleep.h"
#include "fade.h"
#include "xshm.h"
#include "atoms.h"
#include "clientmsg.h"
#include "xmu.h"
#include "pow2.h"
#include "doubletime.h"
#include "screenshot.h"

#undef HAVE_XF86VMODE_GAMMA

#ifndef HAVE_XINPUT
# error The XInput2 extension is required
#endif

#include <X11/extensions/XInput2.h>
#include "xinput.h"

#if defined(__APPLE__) && !defined(HAVE_COCOA)
# define HAVE_MACOS_X11
#elif !defined(HAVE_JWXYZ) /* Real X11, possibly Wayland */
# define HAVE_REAL_X11
#endif


typedef struct {
  int nscreens;
  Pixmap *screenshots;
  double *interrupted_ratio;  /* Pointer to interrupted fade ratio (from saver_info) */
} fade_state;


#ifdef HAVE_SGI_VC_EXTENSION
static int sgi_gamma_fade (XtAppContext, Display *, Window *wins, int count,
                           double secs, Bool out_p);
#endif
#ifdef HAVE_XF86VMODE_GAMMA
static int xf86_gamma_fade (XtAppContext, Display *, Window *wins, int count,
                            double secs, Bool out_p);
#endif
#ifdef HAVE_RANDR_12
static int randr_gamma_fade (XtAppContext, Display *, Window *wins, int count,
                             double secs, Bool out_p, Bool from_desktop_p, double *interrupted_ratio);
#endif
static int colormap_fade (XtAppContext, Display *, Window *wins, int count,
                          double secs, Bool out_p, Bool from_desktop_p,
                          double *interrupted_ratio);
static int xshm_fade (XtAppContext, Display *,
                      Window *wins, int count, double secs,
                      Bool out_p, Bool from_desktop_p, fade_state *);


#ifdef HAVE_XINPUT
static int xi_opcode = -1;
#endif

/* Closure arg points to a Bool saying whether motion events count.
   Motion aborts fade-out, but only clicks and keys abort fade-in.
 */
static Bool
user_event_p (Display *dpy, XEvent *event, XPointer arg)
{
  Bool motion_p = *((Bool *) arg);

  switch (event->xany.type) {
  case KeyPress: case ButtonPress:
    return True;
    break;
  case MotionNotify:
    if (motion_p) return True;
    break;
# ifdef HAVE_XINPUT
  case GenericEvent:
    {
      XIRawEvent *re;
      if (event->xcookie.extension != xi_opcode)
        return False;
      if (! event->xcookie.data)
        XGetEventData (dpy, &event->xcookie);
      if (! event->xcookie.data)
        return False;

      re = event->xcookie.data;
      switch (re->evtype) {
      case XI_RawKeyPress:
      case XI_RawButtonPress:
      case XI_RawTouchBegin:
      case XI_KeyPress:
      case XI_ButtonPress:
      case XI_TouchBegin:
        return True;
        break;
      case XI_RawMotion:
      case XI_RawTouchUpdate:
      case XI_Motion:
      case XI_TouchUpdate:
        if (motion_p) return True;
        break;
      default:
        break;
      }

      /* Calling XFreeEventData here is bad news */
    }
    break;
# endif /* HAVE_XINPUT */
  default: break;
  }

  return False;
}


/* Forward declaration for randr_gamma_info (defined later in HAVE_RANDR_12 block) */
#ifdef HAVE_RANDR_12
# include <X11/extensions/Xrandr.h>
typedef struct {
  RRCrtc crtc;
  RROutput output;
  char *output_name;
  Bool enabled_p;
  XRRCrtcGamma *gamma;
  double original_brightness;  /* Brightness at start (default 1.0) */
  double current_brightness;   /* Current brightness (tracked since we can't query it) */
} randr_gamma_info;

/* Helper to query actual gamma ratio by comparing current gamma to original gamma */
static double
query_actual_gamma_ratio (Display *dpy, randr_gamma_info *ginfo)
{
  XRRCrtcGamma *current_gamma;
  double ratio = -1.0;

  if (!ginfo || !ginfo->gamma || !ginfo->crtc)
    return -1.0;

  current_gamma = XRRGetCrtcGamma (dpy, ginfo->crtc);
  if (!current_gamma || current_gamma->size != ginfo->gamma->size)
    return -1.0;

  /* Calculate ratio by comparing current gamma to original gamma.
     We'll use the middle value of the ramp as a representative sample. */
  int mid = current_gamma->size / 2;
  if (mid >= 0 && mid < current_gamma->size && ginfo->gamma->red[mid] > 0)
    {
      double red_ratio = (double)current_gamma->red[mid] / (double)ginfo->gamma->red[mid];
      double green_ratio = (double)current_gamma->green[mid] / (double)ginfo->gamma->green[mid];
      double blue_ratio = (double)current_gamma->blue[mid] / (double)ginfo->gamma->blue[mid];
      ratio = (red_ratio + green_ratio + blue_ratio) / 3.0;
    }

  XRRFreeGamma (current_gamma);
  return ratio;
}

/* Helper to set brightness using xrandr command (X11 BACKLIGHT property doesn't work visually) */
static int
set_brightness_via_xrandr (const char *output_name, double brightness)
{
  char cmd[256];
  snprintf (cmd, sizeof(cmd), "xrandr --output %s --brightness %.3f 2>/dev/null", output_name, brightness);
  int ret = system (cmd);
  return ret;
}

/* Helper to find the last non-clamped value in a gamma ramp (from xrandr.c) */
static int
find_last_non_clamped (const CARD16 *array, int size)
{
  for (int i = size - 1; i > 0; i--)
    {
      if (array[i] < 0xffff)
        return i;
    }
  return 0;
}

/* Helper to calculate brightness from gamma ramp (same algorithm as xrandr) */
static double
query_brightness (Display *dpy, RROutput output)
{
  double brightness = -1.0;

  if (!dpy || !output)
    return -1.0;

  /* Get CRTC for this output */
  XRRScreenResources *res = XRRGetScreenResources (dpy, DefaultRootWindow (dpy));
  if (!res)
    return -1.0;

  XRROutputInfo *rroi = XRRGetOutputInfo (dpy, res, output);
  if (!rroi || !rroi->crtc)
    {
      if (rroi) XRRFreeOutputInfo (rroi);
      XRRFreeScreenResources (res);
      return -1.0;
    }

  RRCrtc crtc = rroi->crtc;
  XRRFreeOutputInfo (rroi);

  /* Get gamma ramp */
  XRRCrtcGamma *crtc_gamma = XRRGetCrtcGamma (dpy, crtc);
  if (!crtc_gamma)
    {
      XRRFreeScreenResources (res);
      return -1.0;
    }

  int size = crtc_gamma->size;

  /* Calculate brightness using same algorithm as xrandr */
  int last_red = find_last_non_clamped (crtc_gamma->red, size);
  int last_green = find_last_non_clamped (crtc_gamma->green, size);
  int last_blue = find_last_non_clamped (crtc_gamma->blue, size);
  CARD16 *best_array = crtc_gamma->red;
  int last_best = last_red;

  if (last_green > last_best)
    {
      last_best = last_green;
      best_array = crtc_gamma->green;
    }
  if (last_blue > last_best)
    {
      last_best = last_blue;
      best_array = crtc_gamma->blue;
    }
  if (last_best == 0)
    last_best = 1;

  int middle = last_best / 2;
  double i1 = (double)(middle + 1) / size;
  double v1 = (double)(best_array[middle]) / 65535.0;
  double i2 = (double)(last_best + 1) / size;
  double v2 = (double)(best_array[last_best]) / 65535.0;

  if (v2 < 0.0001)
    {
      /* Screen is black */
      brightness = 0.0;
    }
  else
    {
      if ((last_best + 1) == size)
        brightness = v2;
      else
        brightness = exp ((log (v2) * log (i1) - log (v1) * log (i2)) / log (i1 / i2));
    }

  XRRFreeGamma (crtc_gamma);
  XRRFreeScreenResources (res);

  return brightness;
}
#else
/* Dummy type and function when RANDR not available */
typedef void randr_gamma_info;
static double query_actual_gamma_ratio (Display *dpy, randr_gamma_info *ginfo) { return -1.0; }
static double query_brightness (Display *dpy, RROutput output) { (void)dpy; (void)output; return -1.0; }
#endif

/* Helper to log fade progress at 5% intervals */
static void
log_fade_progress (const char *fade_type, double ratio, Bool out_p, int *last_logged_percent, Display *dpy, void *gamma_info, int ngamma)
{
  int percent;
  if (out_p)
    percent = (int)((1.0 - ratio) * 100 + 0.5);  /* fade-out: 0% -> 100% */
  else
    percent = (int)(ratio * 100 + 0.5);           /* fade-in: 0% -> 100% */

  /* Round down to nearest 5% */
  int current_5_percent = (percent / 5) * 5;

  /* Only log when we cross into a new 5% bucket */
  if (current_5_percent > *last_logged_percent)
    {
      char actual_gamma_str[256] = "";

      /* Query actual gamma if we have gamma info (RANDR fade) */
# ifdef HAVE_RANDR_12
      if (gamma_info && ngamma > 0 && dpy)
        {
          randr_gamma_info *randr_info = (randr_gamma_info *)gamma_info;
          char gamma_values[512] = "";
          int gamma_count = 0;

          /* Query actual gamma for all enabled screens and collect all values */
          for (int i = 0; i < ngamma; i++)
            {
              if (randr_info[i].enabled_p)
                {
                  double screen_ratio = query_actual_gamma_ratio (dpy, &randr_info[i]);
                  if (screen_ratio >= 0.0)
                    {
                      if (gamma_count > 0)
                        strcat (gamma_values, ",");
                      char val_str[32];
                      snprintf (val_str, sizeof(val_str), "%.2f", screen_ratio);
                      strcat (gamma_values, val_str);
                      gamma_count++;
                    }
                }
            }

          if (gamma_count > 0)
            {
              snprintf (actual_gamma_str, sizeof(actual_gamma_str), " (actual gamma: %s)", gamma_values);
            }
        }
# endif /* HAVE_RANDR_12 */

      /* Query brightness for all enabled outputs and collect all values */
      char brightness_str[256] = "";
# ifdef HAVE_RANDR_12
      if (gamma_info && ngamma > 0 && dpy)
        {
          randr_gamma_info *randr_info = (randr_gamma_info *)gamma_info;
          char brightness_values[256] = "";
          int brightness_count = 0;

          for (int i = 0; i < ngamma; i++)
            {
              if (randr_info[i].enabled_p && randr_info[i].output)
                {
                  /* Query actual brightness */
                  double brightness = query_brightness (dpy, randr_info[i].output);
                  if (brightness < 0.0)
                    {
                      /* If query failed, use tracked current_brightness as fallback */
                      brightness = randr_info[i].current_brightness;
                    }
                  else
                    {
                      /* Update tracked brightness with queried value */
                      randr_info[i].current_brightness = brightness;
                    }

                  /* Only include valid brightness values (>= 0.0) */
                  if (brightness >= 0.0)
                    {
                      if (brightness_count > 0)
                        strcat (brightness_values, ",");
                      char val_str[32];
                      snprintf (val_str, sizeof(val_str), "%.2f", brightness);
                      strcat (brightness_values, val_str);
                      brightness_count++;
                    }
                }
            }

          if (brightness_count > 0)
            {
              snprintf (brightness_str, sizeof(brightness_str), " (brightness: %s)", brightness_values);
            }
          else
            {
              /* If we can't get brightness from any output, show unknown */
              snprintf (brightness_str, sizeof(brightness_str), " (brightness: unknown)");
            }
        }
# endif /* HAVE_RANDR_12 */

      if (current_5_percent == 0)
        debug_log ("[FADE] %s (%s): 0%% (start, target=%.2f)%s%s", fade_type, (out_p ? "fade-out" : "fade-in"), ratio, actual_gamma_str, brightness_str);
      else if (current_5_percent == 100)
        debug_log ("[FADE] %s (%s): 100%% (complete, target=%.2f)%s%s", fade_type, (out_p ? "fade-out" : "fade-in"), ratio, actual_gamma_str, brightness_str);
      else
        debug_log ("[FADE] %s (%s): %d%% (target=%.2f)%s%s", fade_type, (out_p ? "fade-out" : "fade-in"), current_5_percent, ratio, actual_gamma_str, brightness_str);
      *last_logged_percent = current_5_percent;
    }
}

static Bool
user_active_p (XtAppContext app, Display *dpy, Bool fade_out_p)
{
  XEvent event;
  XtInputMask m;
  Bool motion_p = fade_out_p;   /* Motion aborts fade-out, not fade-in. */
  /* Allow mouse motion to abort fade-out (user wants to cancel screensaver activation) */
  /* Note: motion_p is True during fade-out, False during fade-in */

# ifdef HAVE_XINPUT
  if (xi_opcode == -1)
    {
      Bool ov = verbose_p;
      xi_opcode = 0; /* only init once */
      verbose_p = False;  /* xscreensaver already printed this */
      init_xinput (dpy, &xi_opcode);
      verbose_p = ov;
    }
# endif

  m = XtAppPending (app);
  if (m & ~XtIMXEvent)
    {
      /* Process timers and signals only, don't block. */
      DL(1, "Xt pending %ld", m);
      XtAppProcessEvent (app, m);
    }

  /* If there is user activity, bug out.  (Bug out on keypresses or
     mouse presses, but not motion, and not release events.  Bugging
     out on motion made the unfade hack be totally useless, I think.)
   */
  if (XCheckIfEvent (dpy, &event, &user_event_p, (XPointer) &motion_p))
    {
      XIRawEvent *re = 0;
      if (event.xany.type == GenericEvent && !event.xcookie.data)
        {
          XGetEventData (dpy, &event.xcookie);
          re = event.xcookie.data;
        }
      debug_log ("[FADE] user activity detected during fade: type=%d evtype=%d, putting event back",
                 event.xany.type,
                 (re ? re->evtype : -1));
      XPutBackEvent (dpy, &event);
      XFlush (dpy);  /* Ensure event is available to main process immediately TODO needed? */
      debug_log ("[FADE] event put back and flushed");
      return True;
    }

  return False;
}


static void
flush_user_input (Display *dpy)
{
  XEvent event;
  Bool motion_p = True;
  while (XCheckIfEvent (dpy, &event, &user_event_p, (XPointer) &motion_p))
    if (verbose_p > 1)
      {
        XIRawEvent *re = 0;
        if (event.xany.type == GenericEvent && !event.xcookie.data)
          {
            XGetEventData (dpy, &event.xcookie);
            re = event.xcookie.data;
          }
        DL(1, "flushed user event %d %d",
           event.xany.type,
           (re ? re->evtype : -1));
      }
}


/* This bullshit is needed because the VidMode and SHM extensions don't work
   on remote displays. */
static Bool error_handler_hit_p = False;

static int
ignore_all_errors_ehandler (Display *dpy, XErrorEvent *error)
{
  if (verbose_p > 1)
    XmuPrintDefaultErrorMessage (dpy, error, stderr);
  error_handler_hit_p = True;
  return 0;
}


/* Like XDestroyWindow, but destroys the window later, on a timer.  This is
   necessary to work around a KDE 5 compositor bug.  Without this, destroying
   an old window causes the desktop to briefly become visible, even though a
   new window has already been mapped that is obscuring both of them!
 */
typedef struct {
  XtAppContext app;
  Display *dpy;
  Window window;
} defer_destroy_closure;

static void
defer_destroy_handler (XtPointer closure, XtIntervalId *id)
{
  defer_destroy_closure *c = (defer_destroy_closure *) closure;
  XErrorHandler old_handler;
  XSync (c->dpy, False);
  error_handler_hit_p = False;
  old_handler = XSetErrorHandler (ignore_all_errors_ehandler);
  XDestroyWindow (c->dpy, c->window);
  XSync (c->dpy, False);
  XSetErrorHandler (old_handler);
  if (!error_handler_hit_p)
    DL(2, "destroyed old window 0x%lx", (unsigned long) c->window);
  free (c);
}

/* Used here and in windows.c */
void
defer_XDestroyWindow (XtAppContext app, Display *dpy, Window w)
{
  defer_destroy_closure *c = (defer_destroy_closure *) malloc (sizeof (*c));
  c->app = app;
  c->dpy = dpy;
  c->window = w;
  XtAppAddTimeOut (app, 5 * 1000, defer_destroy_handler, (XtPointer) c);
}


/* Returns true if canceled by user activity.
   If interrupted_ratio is non-NULL:
     - For fade-out: stores the current fade ratio (0.0-1.0) in *interrupted_ratio when interrupted.
     - For fade-in: if *interrupted_ratio > 0.0, starts fade-in from that level instead of 0.0
       (used when resuming from an interrupted fade-out).
 */
Bool
fade_screens (XtAppContext app, Display *dpy,
              Window *saver_windows, int nwindows,
              double seconds, Bool out_p, Bool from_desktop_p,
              void **closureP, double *interrupted_ratio)
{
  int status = False;
  fade_state *state = 0;

  if (nwindows <= 0) return False;
  if (!saver_windows) return False;

  if (!closureP) abort();
  state = (fade_state *) *closureP;
  if (!state)
    {
      state = (fade_state *) calloc (1, sizeof (*state));
      *closureP = state;
    }
  /* Store interrupted_ratio pointer in state for access by fade functions */
  state->interrupted_ratio = interrupted_ratio;

  if (from_desktop_p && !out_p)
    abort();  /* Fading in from desktop makes no sense */

  /* Log the type of fade operation for debugging */
  if (out_p)
    {
      if (from_desktop_p)
        debug_log ("[FADE] fade-out FROM desktop (activating screensaver)");
      else
        debug_log ("[FADE] fade-out FROM screensaver (deactivating screensaver)");
    }
  else
    {
      debug_log ("[FADE] fade-in TO desktop (deactivating screensaver)");
    }

  if (out_p)
    flush_user_input (dpy);    /* Flush at start of cycle */

# ifdef HAVE_SGI_VC_EXTENSION
  /* First try to do it by fading the gamma in an SGI-specific way... */
  debug_log ("[FADE] trying SGI gamma fade method (%s)", (out_p ? "fade-out" : "fade-in"));
  status = sgi_gamma_fade (app, dpy, saver_windows, nwindows, seconds, out_p);
  if (status == 0 || status == 1)
    {
      debug_log ("[FADE] using SGI gamma fade method (%s)", (out_p ? "fade-out" : "fade-in"));
      return status;  /* faded, possibly canceled */
    }
  else
    debug_log ("[FADE] SGI gamma fade method failed (status=%d), trying next method", status);
# else
  debug_log ("[FADE] SGI gamma fade method not compiled in (HAVE_SGI_VC_EXTENSION not defined)");
# endif

# ifdef HAVE_RANDR_12
  /* Then try to do it by fading the gamma in an RANDR-specific way... */
  debug_log ("[FADE] trying RANDR gamma fade method (%s)", (out_p ? "fade-out" : "fade-in"));
  status = randr_gamma_fade (app, dpy, saver_windows, nwindows, seconds, out_p, from_desktop_p, interrupted_ratio);
  if (status == 0 || status == 1)
    {
      debug_log ("[FADE] using RANDR gamma fade method (%s)", (out_p ? "fade-out" : "fade-in"));
      return status;  /* faded, possibly canceled */
    }
  else
    debug_log ("[FADE] RANDR gamma fade method failed (status=%d), trying next method", status);
# else
  debug_log ("[FADE] RANDR gamma fade method not compiled in (HAVE_RANDR_12 not defined)");
# endif

# ifdef HAVE_XF86VMODE_GAMMA
  /* Then try to do it by fading the gamma in an XFree86-specific way... */
  debug_log ("[FADE] trying XF86 gamma fade method (%s)", (out_p ? "fade-out" : "fade-in"));
  status = xf86_gamma_fade(app, dpy, saver_windows, nwindows, seconds, out_p);
  if (status == 0 || status == 1)
    {
      debug_log ("[FADE] using XF86 gamma fade method (%s)", (out_p ? "fade-out" : "fade-in"));
      return status;  /* faded, possibly canceled */
    }
  else
    debug_log ("[FADE] XF86 gamma fade method failed (status=%d), trying next method", status);
# else
  debug_log ("[FADE] XF86 gamma fade method not compiled in (HAVE_XF86VMODE_GAMMA not defined)");
# endif

  if (has_writable_cells (DefaultScreenOfDisplay (dpy),
                          DefaultVisual (dpy, 0)))
    {
      /* Do it the old-fashioned way, which only really worked on
         8-bit displays. */
      debug_log ("[FADE] trying colormap fade method (%s)", (out_p ? "fade-out" : "fade-in"));
      status = colormap_fade (app, dpy, saver_windows, nwindows, seconds,
                              out_p, from_desktop_p, interrupted_ratio);
      if (status == 0 || status == 1)
        {
          debug_log ("[FADE] using colormap fade method (%s)", (out_p ? "fade-out" : "fade-in"));
          return status;  /* faded, possibly canceled */
        }
      else
        debug_log ("[FADE] colormap fade method failed (status=%d), trying next method", status);
    }
  else
    {
      debug_log ("[FADE] colormap fade method skipped (no writable cells available)");
    }

  /* Else do it the hard way, by hacking a screenshot. */
  debug_log ("[FADE] using XSHM/OpenGL fade method (%s)", (out_p ? "fade-out" : "fade-in"));
  status = xshm_fade (app, dpy, saver_windows, nwindows, seconds, out_p,
                      from_desktop_p, state);
  status = (status ? True : False);

  return status;
}

/****************************************************************************

    Colormap fading

 ****************************************************************************/


/* The business with `cmaps_per_screen' is to fake out the SGI 8-bit video
   hardware, which is capable of installing multiple (4) colormaps
   simultaneously.  We have to install multiple copies of the same set of
   colors in order to fill up all the available slots in the hardware color
   lookup table, so we install an extra N colormaps per screen to make sure
   that all screens really go black.

   I'm told that this trick also works with XInside's AcceleratedX when using
   the Matrox Millennium card (which also allows multiple PseudoColor and
   TrueColor visuals to co-exist and display properly at the same time.)

   This trick works ok on the 24-bit Indy video hardware, but doesn't work at
   all on the O2 24-bit hardware.  I guess the higher-end hardware is too
   "good" for this to work (dammit.)  So... I figured out the "right" way to
   do this on SGIs, which is to ramp the monitor's gamma down to 0.  That's
   what is implemented in sgi_gamma_fade(), so we use that if we can.

   Returns:
   0: faded normally
   1: canceled by user activity
  -1: unable to fade because the extension isn't supported.
 */
static int
colormap_fade (XtAppContext app, Display *dpy,
               Window *saver_windows, int nwindows,
               double seconds, Bool out_p, Bool from_desktop_p,
               double *interrupted_ratio)
{
  int status = -1;
  Colormap *window_cmaps = 0;
  int i, j, k;
  unsigned int cmaps_per_screen = 5;
  unsigned int nscreens = ScreenCount(dpy);
  unsigned int ncmaps = nscreens * cmaps_per_screen;
  Colormap *fade_cmaps = 0;
  Bool installed = False;
  unsigned int total_ncolors;
  XColor *orig_colors, *current_colors, *screen_colors, *orig_screen_colors;
  unsigned int screen;

  window_cmaps = (Colormap *) calloc(sizeof(Colormap), nwindows);
  if (!window_cmaps) abort();
  for (screen = 0; screen < nwindows; screen++)
    {
      XWindowAttributes xgwa;
      XGetWindowAttributes (dpy, saver_windows[screen], &xgwa);
      window_cmaps[screen] = xgwa.colormap;
    }

  error_handler_hit_p = False;

  DL(2, "colormap fade %s", (out_p ? "out" : "in"));

  total_ncolors = 0;
  for (i = 0; i < nscreens; i++)
    total_ncolors += CellsOfScreen (ScreenOfDisplay(dpy, i));

  orig_colors    = (XColor *) calloc(sizeof(XColor), total_ncolors);
  current_colors = (XColor *) calloc(sizeof(XColor), total_ncolors);

  /* Get the contents of the colormap we are fading from or to. */
  screen_colors = orig_colors;
  for (i = 0; i < nscreens; i++)
    {
      Screen *sc = ScreenOfDisplay (dpy, i);
      int ncolors = CellsOfScreen (sc);
      Colormap cmap = (from_desktop_p || !out_p
                       ? DefaultColormapOfScreen(sc)
                       : window_cmaps[i]);
      for (j = 0; j < ncolors; j++)
        screen_colors[j].pixel = j;
      XQueryColors (dpy, cmap, screen_colors, ncolors);

      screen_colors += ncolors;
    }

  memcpy (current_colors, orig_colors, total_ncolors * sizeof (XColor));


  /* Make the writable colormaps (we keep these around and reuse them.) */
  if (!fade_cmaps)
    {
      fade_cmaps = (Colormap *) calloc(sizeof(Colormap), ncmaps);
      for (i = 0; i < nscreens; i++)
        {
          Visual *v = DefaultVisual(dpy, i);
          Screen *s = ScreenOfDisplay(dpy, i);
          if (has_writable_cells (s, v))
            for (j = 0; j < cmaps_per_screen; j++)
              fade_cmaps[(i * cmaps_per_screen) + j] =
            XCreateColormap (dpy, RootWindowOfScreen (s), v, AllocAll);
        }
    }

  /* Run the animation at the maximum frame rate in the time allotted. */
  {
    double start_time = double_time();
    double end_time = start_time + seconds;
    double prev = 0;
    double now;
    int frames = 0;
    int last_logged_percent = -1;
    double max = 1/60.0;  /* max FPS */
    if (!out_p && interrupted_ratio && *interrupted_ratio > 0.0)
      debug_log ("[FADE] fade-in starting from captured level: %.2f", *interrupted_ratio);
    while ((now = double_time()) < end_time)
      {
        double ratio = (end_time - now) / seconds;
        if (!out_p)
          {
            /* For fade-in, adjust to start from interrupted_ratio if provided */
            ratio = 1-ratio;
            if (interrupted_ratio && *interrupted_ratio > 0.0)
              ratio = *interrupted_ratio + (1.0 - *interrupted_ratio) * ratio;
          }

        /* Continuously update interrupted_ratio during fade-out so it's current if interrupted */
        if (out_p && from_desktop_p && interrupted_ratio)
          *interrupted_ratio = ratio;

        log_fade_progress ("colormap", ratio, out_p, &last_logged_percent, dpy, NULL, 0);

        /* For each screen, compute the current value of each color...
         */
        orig_screen_colors = orig_colors;
        screen_colors = current_colors;
        for (j = 0; j < nscreens; j++)
          {
            int ncolors = CellsOfScreen (ScreenOfDisplay (dpy, j));
            for (k = 0; k < ncolors; k++)
              {
                /* This doesn't take into account the relative luminance of the
                   RGB components (0.299, 0.587, and 0.114 at gamma 2.2) but
                   the difference is imperceptible for this application... */
                screen_colors[k].red   = orig_screen_colors[k].red   * ratio;
                screen_colors[k].green = orig_screen_colors[k].green * ratio;
                screen_colors[k].blue  = orig_screen_colors[k].blue  * ratio;
              }
            screen_colors      += ncolors;
            orig_screen_colors += ncolors;
          }

        /* Put the colors into the maps...
         */
        screen_colors = current_colors;
        for (j = 0; j < nscreens; j++)
          {
            int ncolors = CellsOfScreen (ScreenOfDisplay (dpy, j));
            for (k = 0; k < cmaps_per_screen; k++)
              {
                Colormap c = fade_cmaps[j * cmaps_per_screen + k];
                if (c)
                  XStoreColors (dpy, c, screen_colors, ncolors);
              }
            screen_colors += ncolors;
          }

        /* Put the maps on the screens, and then take the windows off the
           screen.  (only need to do this the first time through the loop.)
         */
        if (!installed)
          {
            for (j = 0; j < ncmaps; j++)
              if (fade_cmaps[j])
                XInstallColormap (dpy, fade_cmaps[j]);
            installed = True;

            if (!out_p)
              for (j = 0; j < nwindows; j++)
                {
                  debug_log ("[FADE] colormap fade-in: unmapping saver window 0x%lx", (unsigned long) saver_windows[j]);
                  XUnmapWindow (dpy, saver_windows[j]);
                  XClearWindow (dpy, saver_windows[j]);
                }
          }

        XSync (dpy, False);

        if (error_handler_hit_p)
          goto DONE;
        /* Only check for user activity during fade-out INTO screensaver (from_desktop_p).
           Fade-out FROM screensaver and fade-in should NOT be interruptable. */
        if (out_p && from_desktop_p && user_active_p (app, dpy, True))
          {
            status = 1;   /* user activity status code */
            /* Fade-out into screensaver was interrupted, capture the current ratio */
            if (interrupted_ratio)
              {
                *interrupted_ratio = ratio;
                debug_log ("[FADE] fade-out interrupted, capturing fade level: %.2f", ratio);
              }
            goto DONE;
          }
        frames++;

        if (now < prev + max)
          usleep (1000000 * (prev + max - now));
        prev = now;
      }

    DL(2, "%.0f FPS", frames / (now - start_time));
  }

  status = 0;   /* completed fade with no user activity */

 DONE:

  if (orig_colors)    free (orig_colors);
  if (current_colors) free (current_colors);

  /* If we've been given windows to raise after blackout, raise them before
     releasing the colormaps.
     However, if fade-out was interrupted, don't raise windows - preserve the interrupted fade level.
   */
  if (out_p && status != 1)
    {
      for (i = 0; i < nwindows; i++)
        {
          debug_log ("[FADE] colormap fade-out: clearing and map-raising saver window 0x%lx", (unsigned long) saver_windows[i]);
          XClearWindow (dpy, saver_windows[i]);
          XMapRaised (dpy, saver_windows[i]);
        }
      XSync(dpy, False);
    }

  /* Now put the target maps back.
     If we're fading out, use the given cmap (or the default cmap, if none.)
     If we're fading in, always use the default cmap.
   */
  for (i = 0; i < nscreens; i++)
    {
      Colormap cmap = window_cmaps[i];
      if (!cmap || !out_p)
        cmap = DefaultColormap(dpy, i);
      XInstallColormap (dpy, cmap);
    }

  /* The fade (in or out) is complete, so we don't need the black maps on
     stage any more.
   */
  for (i = 0; i < ncmaps; i++)
    if (fade_cmaps[i])
      {
        XUninstallColormap(dpy, fade_cmaps[i]);
        XFreeColormap(dpy, fade_cmaps[i]);
        fade_cmaps[i] = 0;
      }
  free (window_cmaps);
  free(fade_cmaps);
  fade_cmaps = 0;

  if (error_handler_hit_p) status = -1;
  return status;
}


/****************************************************************************

    SGI gamma fading

 ****************************************************************************/

#ifdef HAVE_SGI_VC_EXTENSION

# include <X11/extensions/XSGIvc.h>

struct screen_sgi_gamma_info {
  int gamma_map;  /* ??? always using 0 */
  int nred, ngreen, nblue;
  unsigned short *red1, *green1, *blue1;
  unsigned short *red2, *green2, *blue2;
  int gamma_size;
  int gamma_precision;
  Bool alpha_p;
};


static void sgi_whack_gamma(Display *dpy, int screen,
                            struct screen_sgi_gamma_info *info, float ratio);

/* Returns:
   0: faded normally
   1: canceled by user activity
  -1: unable to fade because the extension isn't supported.
 */
static int
sgi_gamma_fade (XtAppContext app, Display *dpy,
                Window *saver_windows, int nwindows,
                double seconds, Bool out_p)
{
  int nscreens = ScreenCount(dpy);
  struct timeval then, now;
  int i, screen;
  int status = -1;
  struct screen_sgi_gamma_info *info = (struct screen_sgi_gamma_info *)
    calloc(nscreens, sizeof(*info));

  DL(2, "sgi fade %s", (out_p ? "out" : "in"));

  /* Get the current gamma maps for all screens.
     Bug out and return -1 if we can't get them for some screen.
   */
  for (screen = 0; screen < nscreens; screen++)
    {
      if (!XSGIvcQueryGammaMap(dpy, screen, info[screen].gamma_map,
                               &info[screen].gamma_size,
                               &info[screen].gamma_precision,
                               &info[screen].alpha_p))
        goto FAIL;

      if (!XSGIvcQueryGammaColors(dpy, screen, info[screen].gamma_map,
                              XSGIVC_COMPONENT_RED,
                              &info[screen].nred, &info[screen].red1))
        goto FAIL;
      if (! XSGIvcQueryGammaColors(dpy, screen, info[screen].gamma_map,
                                   XSGIVC_COMPONENT_GREEN,
                                   &info[screen].ngreen, &info[screen].green1))
        goto FAIL;
      if (!XSGIvcQueryGammaColors(dpy, screen, info[screen].gamma_map,
                                  XSGIVC_COMPONENT_BLUE,
                                  &info[screen].nblue, &info[screen].blue1))
        goto FAIL;

      if (info[screen].gamma_precision == 8)    /* Scale it up to 16 bits. */
        {
          int j;
          for(j = 0; j < info[screen].nred; j++)
            info[screen].red1[j]   =
              ((info[screen].red1[j]   << 8) | info[screen].red1[j]);
          for(j = 0; j < info[screen].ngreen; j++)
            info[screen].green1[j] =
              ((info[screen].green1[j] << 8) | info[screen].green1[j]);
          for(j = 0; j < info[screen].nblue; j++)
            info[screen].blue1[j]  =
              ((info[screen].blue1[j]  << 8) | info[screen].blue1[j]);
        }

      info[screen].red2   = (unsigned short *)
        malloc(sizeof(*info[screen].red2)   * (info[screen].nred+1));
      info[screen].green2 = (unsigned short *)
        malloc(sizeof(*info[screen].green2) * (info[screen].ngreen+1));
      info[screen].blue2  = (unsigned short *)
        malloc(sizeof(*info[screen].blue2)  * (info[screen].nblue+1));
    }

#ifdef GETTIMEOFDAY_TWO_ARGS
  gettimeofday(&then, &tzp);
#else
  gettimeofday(&then);
#endif

  /* If we're fading in (from black), then first crank the gamma all the
     way down to 0, then take the windows off the screen.
   */
  if (!out_p)
    {
      for (screen = 0; screen < nscreens; screen++)
        sgi_whack_gamma(dpy, screen, &info[screen], 0.0);

      for (screen = 0; screen < nwindows; screen++)
        {
          debug_log ("[FADE] SGI fade-in: unmapping saver window 0x%lx", (unsigned long) saver_windows[screen]);
          XUnmapWindow (dpy, saver_windows[screen]);
          XClearWindow (dpy, saver_windows[screen]);
          XSync(dpy, False);
        }
    }

  /* Run the animation at the maximum frame rate in the time allotted. */
  {
    double start_time = double_time();
    double end_time = start_time + seconds;
    double prev = 0;
    double now;
    int frames = 0;
    int last_logged_percent = -1;
    double max = 1/60.0;  /* max FPS */
    while ((now = double_time()) < end_time)
      {
        double ratio = (end_time - now) / seconds;
        if (!out_p) ratio = 1-ratio;

        log_fade_progress ("SGI gamma", ratio, out_p, &last_logged_percent, dpy, NULL, 0);

        for (screen = 0; screen < nwindows; screen++)
          sgi_whack_gamma (dpy, screen, &info[screen], ratio);

        if (error_handler_hit_p)
          goto FAIL;
        if (user_active_p (app, dpy, out_p))
          {
            status = 1;   /* user activity status code */
            goto DONE;
          }
        frames++;

        if (now < prev + max)
          usleep (1000000 * (prev + max - now));
        prev = now;
      }

    DL(2, "%.0f FPS", frames / (now - start_time));
  }

  status = 0;   /* completed fade with no user activity */

 DONE:

  if (out_p)
    {
      for (screen = 0; screen < nwindows; screen++)
        {
          debug_log ("[FADE] SGI fade-out: clearing and map-raising saver window 0x%lx", (unsigned long) saver_windows[screen]);
          XClearWindow (dpy, saver_windows[screen]);
          XMapRaised (dpy, saver_windows[screen]);
        }
      XSync(dpy, False);
    }

  /* I can't explain this; without this delay, we get a flicker.
     I suppose there's some lossage with stale bits being in the
     hardware frame buffer or something, and this delay gives it
     time to flush out.  This sucks! */
  usleep(100000);  /* 1/10th second */

  for (screen = 0; screen < nscreens; screen++)
    sgi_whack_gamma(dpy, screen, &info[screen], 1.0);
  XSync(dpy, False);

 FAIL:
  for (screen = 0; screen < nscreens; screen++)
    {
      if (info[screen].red1)   free (info[screen].red1);
      if (info[screen].green1) free (info[screen].green1);
      if (info[screen].blue1)  free (info[screen].blue1);
      if (info[screen].red2)   free (info[screen].red2);
      if (info[screen].green2) free (info[screen].green2);
      if (info[screen].blue2)  free (info[screen].blue2);
    }
  free(info);

  if (status)
    DL(2, "SGI fade %s failed", (out_p ? "out" : "in"));

  if (error_handler_hit_p) status = -1;
  return status;
}

static void
sgi_whack_gamma (Display *dpy, int screen, struct screen_sgi_gamma_info *info,
                 float ratio)
{
  int k;

  if (ratio < 0) ratio = 0;
  if (ratio > 1) ratio = 1;
  for (k = 0; k < info->gamma_size; k++)
    {
      info->red2[k]   = info->red1[k]   * ratio;
      info->green2[k] = info->green1[k] * ratio;
      info->blue2[k]  = info->blue1[k]  * ratio;
    }

  XSGIvcStoreGammaColors16(dpy, screen, info->gamma_map, info->nred,
                           XSGIVC_MComponentRed, info->red2);
  XSGIvcStoreGammaColors16(dpy, screen, info->gamma_map, info->ngreen,
                           XSGIVC_MComponentGreen, info->green2);
  XSGIvcStoreGammaColors16(dpy, screen, info->gamma_map, info->nblue,
                           XSGIVC_MComponentBlue, info->blue2);
  XSync(dpy, False);
}

#endif /* HAVE_SGI_VC_EXTENSION */


/****************************************************************************

    XFree86 gamma fading

 ****************************************************************************/

#ifdef HAVE_XF86VMODE_GAMMA

#include <X11/extensions/xf86vmode.h>

typedef struct {
  XF86VidModeGamma vmg;
  int size;
  unsigned short *r, *g, *b;
} xf86_gamma_info;

static int xf86_check_gamma_extension (Display *dpy);
static Bool xf86_whack_gamma (Display *dpy, int screen,
                              xf86_gamma_info *ginfo, float ratio);

/* Returns:
   0: faded normally
   1: canceled by user activity
  -1: unable to fade because the extension isn't supported.
 */
static int
xf86_gamma_fade (XtAppContext app, Display *dpy,
                 Window *saver_windows, int nwindows,
                 double seconds, Bool out_p)
{
  int nscreens = ScreenCount(dpy);
  int screen;
  int status = -1;
  xf86_gamma_info *info = 0;

  static int ext_ok = -1;

  DL(2, "xf86 fade %s", (out_p ? "out" : "in"));

  /* Only probe the extension once: the answer isn't going to change. */
  if (ext_ok == -1)
    ext_ok = xf86_check_gamma_extension (dpy);

  /* If this server doesn't have the gamma extension, bug out. */
  if (ext_ok == 0)
    goto FAIL;

# ifndef HAVE_XF86VMODE_GAMMA_RAMP
  if (ext_ok == 2) ext_ok = 1;  /* server is newer than client! */
# endif

  info = (xf86_gamma_info *) calloc(nscreens, sizeof(*info));

  /* Get the current gamma maps for all screens.
     Bug out and return -1 if we can't get them for some screen.
   */
  for (screen = 0; screen < nscreens; screen++)
    {
      if (ext_ok == 1)  /* only have gamma parameter, not ramps. */
        {
          if (!XF86VidModeGetGamma(dpy, screen, &info[screen].vmg))
            goto FAIL;
        }
# ifdef HAVE_XF86VMODE_GAMMA_RAMP
      else if (ext_ok == 2)  /* have ramps */
        {
          if (!XF86VidModeGetGammaRampSize(dpy, screen, &info[screen].size))
            goto FAIL;
          if (info[screen].size <= 0)
            goto FAIL;

          info[screen].r = (unsigned short *)
            calloc(info[screen].size, sizeof(unsigned short));
          info[screen].g = (unsigned short *)
            calloc(info[screen].size, sizeof(unsigned short));
          info[screen].b = (unsigned short *)
            calloc(info[screen].size, sizeof(unsigned short));

          if (!(info[screen].r && info[screen].g && info[screen].b))
            goto FAIL;

# if 0
          if (verbose_p > 1 && out_p)
            {
              int i;
              fprintf (stderr, "%s: initial gamma ramps, size %d:\n",
                       blurb(), info[screen].size);
              fprintf (stderr, "%s:   R:", blurb());
              for (i = 0; i < info[screen].size; i++)
                fprintf (stderr, " %d", info[screen].r[i]);
              fprintf (stderr, "\n%s:   G:", blurb());
              for (i = 0; i < info[screen].size; i++)
                fprintf (stderr, " %d", info[screen].g[i]);
              fprintf (stderr, "\n%s:   B:", blurb());
              for (i = 0; i < info[screen].size; i++)
                fprintf (stderr, " %d", info[screen].b[i]);
              fprintf (stderr, "\n");
            }
# endif /* 0 */

          if (!XF86VidModeGetGammaRamp(dpy, screen, info[screen].size,
                                       info[screen].r,
                                       info[screen].g,
                                       info[screen].b))
            goto FAIL;
        }
# endif /* HAVE_XF86VMODE_GAMMA_RAMP */
      else
        abort();
    }

  /* If we're fading in (from black), then first crank the gamma all the
     way down to 0, then take the windows off the screen.
   */
  if (!out_p)
    {
      for (screen = 0; screen < nscreens; screen++)
        xf86_whack_gamma(dpy, screen, &info[screen], 0.0);
      for (screen = 0; screen < nwindows; screen++)
        {
          debug_log ("[FADE] XF86 fade-in: unmapping saver window 0x%lx", (unsigned long) saver_windows[screen]);
          XUnmapWindow (dpy, saver_windows[screen]);
          XClearWindow (dpy, saver_windows[screen]);
          XSync(dpy, False);
        }
    }

  /* Run the animation at the maximum frame rate in the time allotted. */
  {
    double start_time = double_time();
    double end_time = start_time + seconds;
    double prev = 0;
    double now;
    int frames = 0;
    int last_logged_percent = -1;
    double max = 1/60.0;  /* max FPS */
    while ((now = double_time()) < end_time)
      {
        double ratio = (end_time - now) / seconds;
        if (!out_p) ratio = 1-ratio;

        log_fade_progress ("XF86 gamma", ratio, out_p, &last_logged_percent, dpy, NULL, 0);

        for (screen = 0; screen < nscreens; screen++)
          xf86_whack_gamma (dpy, screen, &info[screen], ratio);

        if (error_handler_hit_p)
          goto FAIL;
        if (user_active_p (app, dpy, out_p))
          {
            status = 1;   /* user activity status code */
            goto DONE;
          }
        frames++;

        if (now < prev + max)
          usleep (1000000 * (prev + max - now));
        prev = now;
      }

    DL(2, "%.0f FPS", frames / (now - start_time));
  }

  status = 0;   /* completed fade with no user activity */

 DONE:

  if (out_p)
    {
      for (screen = 0; screen < nwindows; screen++)
        {
          debug_log ("[FADE] XF86 fade-out: clearing and map-raising saver window 0x%lx", (unsigned long) saver_windows[screen]);
          XClearWindow (dpy, saver_windows[screen]);
          XMapRaised (dpy, saver_windows[screen]);
        }
      XSync(dpy, False);
    }

  /* I can't explain this; without this delay, we get a flicker.
     I suppose there's some lossage with stale bits being in the
     hardware frame buffer or something, and this delay gives it
     time to flush out.  This sucks! */
  usleep(100000);  /* 1/10th second */

  /* If fade-out was interrupted, don't restore gamma - preserve the interrupted fade level */
  if (status != 1)
    {
      for (screen = 0; screen < nscreens; screen++)
        xf86_whack_gamma(dpy, screen, &info[screen], 1.0);
      XSync(dpy, False);
    }

 FAIL:
  if (info)
    {
      for (screen = 0; screen < nscreens; screen++)
        {
          if (info[screen].r) free(info[screen].r);
          if (info[screen].g) free(info[screen].g);
          if (info[screen].b) free(info[screen].b);
        }
      free(info);
    }

  if (status)
    DL(2, "xf86 fade %s failed", (out_p ? "out" : "in"));

  if (error_handler_hit_p) status = -1;
  return status;
}


static Bool
safe_XF86VidModeQueryVersion (Display *dpy, int *majP, int *minP)
{
  Bool result;
  XErrorHandler old_handler;
  XSync (dpy, False);
  error_handler_hit_p = False;
  old_handler = XSetErrorHandler (ignore_all_errors_ehandler);

  result = XF86VidModeQueryVersion (dpy, majP, minP);

  XSync (dpy, False);
  XSetErrorHandler (old_handler);
  XSync (dpy, False);

  return (error_handler_hit_p
          ? False
          : result);
}



/* VidModeExtension version 2.0 or better is needed to do gamma.
   2.0 added gamma values; 2.1 added gamma ramps.
 */
# define XF86_VIDMODE_GAMMA_MIN_MAJOR 2
# define XF86_VIDMODE_GAMMA_MIN_MINOR 0
# define XF86_VIDMODE_GAMMA_RAMP_MIN_MAJOR 2
# define XF86_VIDMODE_GAMMA_RAMP_MIN_MINOR 1



/* Returns 0 if gamma fading not available; 1 if only gamma value setting
   is available; 2 if gamma ramps are available.
 */
static int
xf86_check_gamma_extension (Display *dpy)
{
  int event, error, major, minor;

  if (!XF86VidModeQueryExtension (dpy, &event, &error))
    return 0;  /* display doesn't have the extension. */

  if (!safe_XF86VidModeQueryVersion (dpy, &major, &minor))
    return 0;  /* unable to get version number? */

  if (major < XF86_VIDMODE_GAMMA_MIN_MAJOR ||
      (major == XF86_VIDMODE_GAMMA_MIN_MAJOR &&
       minor < XF86_VIDMODE_GAMMA_MIN_MINOR))
    return 0;  /* extension is too old for gamma. */

  if (major < XF86_VIDMODE_GAMMA_RAMP_MIN_MAJOR ||
      (major == XF86_VIDMODE_GAMMA_RAMP_MIN_MAJOR &&
       minor < XF86_VIDMODE_GAMMA_RAMP_MIN_MINOR))
    return 1;  /* extension is too old for gamma ramps. */

  /* Copacetic */
  return 2;
}


/* XFree doesn't let you set gamma to a value smaller than this.
   Apparently they didn't anticipate the trick I'm doing here...
 */
#define XF86_MIN_GAMMA  0.1


static Bool
xf86_whack_gamma(Display *dpy, int screen, xf86_gamma_info *info,
                 float ratio)
{
  XErrorHandler old_handler;
  XSync (dpy, False);
  error_handler_hit_p = False;
  old_handler = XSetErrorHandler (ignore_all_errors_ehandler);

  if (ratio < 0) ratio = 0;
  if (ratio > 1) ratio = 1;

  if (info->size == 0)    /* we only have a gamma number, not a ramp. */
    {
      XF86VidModeGamma g2;

      g2.red   = info->vmg.red   * ratio;
      g2.green = info->vmg.green * ratio;
      g2.blue  = info->vmg.blue  * ratio;

# ifdef XF86_MIN_GAMMA
      if (g2.red   < XF86_MIN_GAMMA) g2.red   = XF86_MIN_GAMMA;
      if (g2.green < XF86_MIN_GAMMA) g2.green = XF86_MIN_GAMMA;
      if (g2.blue  < XF86_MIN_GAMMA) g2.blue  = XF86_MIN_GAMMA;
# endif

      if (! XF86VidModeSetGamma (dpy, screen, &g2))
        return -1;
    }
  else
    {
# ifdef HAVE_XF86VMODE_GAMMA_RAMP

      unsigned short *r, *g, *b;
      int i;
      r = (unsigned short *) malloc(info->size * sizeof(unsigned short));
      g = (unsigned short *) malloc(info->size * sizeof(unsigned short));
      b = (unsigned short *) malloc(info->size * sizeof(unsigned short));

      for (i = 0; i < info->size; i++)
        {
          r[i] = info->r[i] * ratio;
          g[i] = info->g[i] * ratio;
          b[i] = info->b[i] * ratio;
        }

      if (! XF86VidModeSetGammaRamp(dpy, screen, info->size, r, g, b))
        return -1;

      free (r);
      free (g);
      free (b);

# else  /* !HAVE_XF86VMODE_GAMMA_RAMP */
      abort();
# endif /* !HAVE_XF86VMODE_GAMMA_RAMP */
    }

  XSync (dpy, False);
  XSetErrorHandler (old_handler);
  XSync (dpy, False);

  return status;
}

#endif /* HAVE_XF86VMODE_GAMMA */


/****************************************************************************

    RANDR gamma fading

 ****************************************************************************


   Dec 2020: I noticed that gamma fading was not working on a Raspberry Pi
   with Raspbian 10.6, and wrote this under the hypothesis that the XF86
   gamma fade code was failing and maybe the RANDR version would work better.
   Then I discovered that gamma simply isn't supported by the Raspberry Pi
   HDMI driver:

       https://github.com/raspberrypi/firmware/issues/1274

   I should have tried this first and seen it not work:

       xrandr --output HDMI-1 --brightness .1

   Since I still don't have an answer to the question of whether the XF86
   gamma fading method works on modern Linux systems that also have RANDR,
   I'm leaving this new code turned off for now, as it is largely untested.
   The new code would be useful if:

     A) The XF86 way doesn't work but the RANDR way does, or
     B) There exist systems that have RANDR but do not have XF86.

   But until Raspberry Pi supports gamma, both gamma methods fail to work
   for far too many users for them to be used in XScreenSaver.
 */
#ifdef HAVE_RANDR_12

# include <X11/extensions/Xrandr.h>


/* Check if we're running on a Raspberry Pi (which doesn't support gamma).
   Simplest detection: check /proc/cpuinfo for "Hardware" line containing "BCM"
   (Broadcom chip, which is used by all Raspberry Pi models).
 */
static Bool
is_raspberry_pi (void)
{
  static int cached = -1;  /* -1 = unknown, 0 = no, 1 = yes */
  FILE *f;
  char line[256];

  if (cached != -1)
    return (cached == 1);

  cached = 0;  /* Default to not Raspberry Pi */

  /* Check /proc/cpuinfo for Hardware line with BCM (Broadcom chip) */
  f = fopen ("/proc/cpuinfo", "r");
  if (f)
    {
      while (fgets (line, sizeof(line), f))
        {
          /* Look for "Hardware" line containing "BCM" (Broadcom) */
          if (strstr (line, "Hardware") && strstr (line, "BCM"))
            {
              cached = 1;
              fclose (f);
              return True;
            }
        }
      fclose (f);
    }

  return False;
}


static int
randr_check_gamma_extension (Display *dpy)
{
  int event, error, major, minor;
  if (! XRRQueryExtension (dpy, &event, &error))
    return 0;

  if (! XRRQueryVersion (dpy, &major, &minor)) {
    DL(2, "no randr ext");
    return 0;
  }

  /* Reject if < 1.5. It's possible that 1.2 - 1.4 work, but untested. */
  if (major < 1 || (major == 1 && minor < 5)) {
    DL(2, "randr ext only version %d.%d", major, minor);
    return 0;
  }

  return 1;
}


static void randr_whack_gamma (Display *dpy, int screen,
                               randr_gamma_info *ginfo, float ratio);

/* Returns:
   0: faded normally
   1: canceled by user activity
  -1: unable to fade because the extension isn't supported.
 */
static int
randr_gamma_fade (XtAppContext app, Display *dpy,
                   Window *saver_windows, int nwindows,
                   double seconds, Bool out_p, Bool from_desktop_p, double *interrupted_ratio)
{
  int xsc = ScreenCount (dpy);
  int nscreens = 0;
  int j, screen;
  int status = -1;
  randr_gamma_info *info = 0;

  static int ext_ok = -1;

  DL(2, "randr fade %s", (out_p ? "out" : "in"));

  /* Skip RANDR gamma fade on Raspberry Pi (gamma not supported) */
  if (is_raspberry_pi ())
    {
      debug_log ("[FADE] RANDR gamma fade skipped (Raspberry Pi detected - gamma not supported)");
      goto FAIL;
    }

  /* Only probe the extension once: the answer isn't going to change. */
  if (ext_ok == -1)
    ext_ok = randr_check_gamma_extension (dpy);

  /* If this server doesn't have the RANDR extension, bug out. */
  if (ext_ok == 0)
    goto FAIL;

  /* Add up the virtual screens on each X screen. */
  for (screen = 0; screen < xsc; screen++)
    {
      XRRScreenResources *res =
        XRRGetScreenResources (dpy, RootWindow (dpy, screen));
      nscreens += res->noutput;
      XRRFreeScreenResources (res);
    }

  if (nscreens <= 0)
    goto FAIL;

  info = (randr_gamma_info *) calloc(nscreens, sizeof(*info));

  /* Get the current gamma maps for all screens.
     Bug out and return -1 if we can't get them for some screen.
  */
  for (screen = 0, j = 0; screen < xsc; screen++)
    {
      XRRScreenResources *res =
        XRRGetScreenResources (dpy, RootWindow (dpy, screen));
      int k;
      for (k = 0; k < res->noutput; k++, j++)
        {
          XRROutputInfo *rroi = XRRGetOutputInfo (dpy, res, res->outputs[k]);
          RRCrtc crtc = (rroi->crtc  ? rroi->crtc :
                         rroi->ncrtc ? rroi->crtcs[0] : 0);

          info[j].crtc = crtc;
          info[j].output = res->outputs[k];
          info[j].output_name = strdup (rroi->name);

          /* Get the current gamma ramp.

             IMPORTANT: For fade-out, this is the original gamma ramp (at gamma=1.0).
             For fade-in after interruption, this is the interrupted gamma ramp (at e.g., gamma=0.76).
             For fade-in from black, this is the black gamma ramp (at gamma=0.0).

             We need to preserve the "original" gamma ramp (at gamma=1.0) for brightness calculations.
             For fade-in after interruption, we'll need to "restore" the original by scaling the
             interrupted gamma ramp back to 1.0, or we can calculate brightness assuming the original
             was at 1.0.
             */
          XRRCrtcGamma *current_gamma = XRRGetCrtcGamma (dpy, crtc);

          /* If we can't get gamma for this CRTC, gamma isn't supported */
          if (!current_gamma || !crtc)
            {
              debug_log ("[FADE] RANDR gamma not supported (XRRGetCrtcGamma returned NULL for crtc %lu)",
                        (unsigned long) crtc);
              free (info[j].output_name);
              XRRFreeOutputInfo (rroi);
              XRRFreeScreenResources (res);
              goto FAIL;
            }

          /* For fade-in after interruption, we need to restore the original gamma ramp (at 1.0).
             The current gamma ramp is at the interrupted level. We can estimate the original by
             scaling the current ramp up by 1.0/start_ratio. However, a simpler approach is to
             calculate brightness from the current ramp and then scale it, or assume the original
             brightness was 1.0.

             For now, we'll store the current gamma ramp and handle the brightness calculation
             specially for fade-in after interruption.
             */
          if (!out_p && interrupted_ratio && *interrupted_ratio > 0.0)
            {
              /* Fade-in after interruption: current gamma is at interrupted level.
                 We need to "restore" it to the original (gamma=1.0) by scaling up.
                 Create a new gamma ramp by dividing the current ramp by interrupted_ratio. */
              double start_ratio = *interrupted_ratio;
              info[j].gamma = XRRAllocGamma (current_gamma->size);
              for (int i = 0; i < current_gamma->size; i++)
                {
                  info[j].gamma->red[i] = (unsigned short)(current_gamma->red[i] / start_ratio);
                  info[j].gamma->green[i] = (unsigned short)(current_gamma->green[i] / start_ratio);
                  info[j].gamma->blue[i] = (unsigned short)(current_gamma->blue[i] / start_ratio);
                  /* Clamp to max value */
                  if (info[j].gamma->red[i] > 65535) info[j].gamma->red[i] = 65535;
                  if (info[j].gamma->green[i] > 65535) info[j].gamma->green[i] = 65535;
                  if (info[j].gamma->blue[i] > 65535) info[j].gamma->blue[i] = 65535;
                }
              info[j].gamma->size = current_gamma->size;
              XRRFreeGamma (current_gamma);
              debug_log ("[FADE] fade-in after interruption: restored original gamma ramp for %s (scaled from interrupted level %.2f)",
                        info[j].output_name, start_ratio);
            }
          else
            {
              /* Fade-out or fade-in from black: use the current gamma ramp as-is */
              info[j].gamma = current_gamma;
            }

          /* Query and capture original brightness.

             At fade-out start: gamma is at 1.0 (full brightness), so we query the current brightness
             which represents the original brightness before fade-out began.

             At fade-in start after interrupted fade-out: gamma is at the interrupted level (e.g., 0.76),
             but we need the original brightness (at gamma=1.0). Since brightness is calculated from
             the gamma ramp, and we have the original gamma ramp stored in info[j].gamma (captured at
             fade-out start, representing gamma=1.0), we calculate brightness from that original ramp.

             At fade-in start from black (fade-out completed): gamma is at 0.0, so we query current
             brightness (which should be 0.0 or very low).
             */
          info[j].original_brightness = -1.0;  /* invalid until queried */
          info[j].current_brightness = -1.0;   /* invalid until queried */

          if (!out_p && interrupted_ratio && *interrupted_ratio > 0.0)
            {
              /* Fade-in after interrupted fade-out:
                 - original_brightness = brightness at gamma=1.0 (START value, before fade-out began)
                 - current_brightness = brightness at interrupted gamma level (END value, at interruption)

                 The original gamma ramp (info[j].gamma) was captured at fade-out start and represents
                 gamma=1.0. We calculate brightness from that original ramp to get the original brightness.
                 We also query the current brightness to track the interrupted level.
                 */
              double start_ratio = *interrupted_ratio;

              /* Calculate original brightness from the original gamma ramp (gamma=1.0) */
              XRRCrtcGamma *original_gamma = info[j].gamma;
              if (original_gamma && original_gamma->size > 0)
                {
                  /* Use the same algorithm as query_brightness, but on the original gamma ramp */
                  int last_red = find_last_non_clamped (original_gamma->red, original_gamma->size);
                  int last_green = find_last_non_clamped (original_gamma->green, original_gamma->size);
                  int last_blue = find_last_non_clamped (original_gamma->blue, original_gamma->size);
                  CARD16 *best_array = original_gamma->red;
                  int last_best = last_red;

                  if (last_green > last_best)
                    {
                      last_best = last_green;
                      best_array = original_gamma->green;
                    }
                  if (last_blue > last_best)
                    {
                      last_best = last_blue;
                      best_array = original_gamma->blue;
                    }
                  if (last_best == 0)
                    last_best = 1;

                  int middle = last_best / 2;
                  double i1 = (double)(middle + 1) / original_gamma->size;
                  double v1 = (double)(best_array[middle]) / 65535.0;
                  double i2 = (double)(last_best + 1) / original_gamma->size;
                  double v2 = (double)(best_array[last_best]) / 65535.0;

                  if (v2 < 0.0001)
                    {
                      info[j].original_brightness = 0.0;
                    }
                  else
                    {
                      double log_v1 = log (v1);
                      double log_v2 = log (v2);
                      double log_i1 = log (i1);
                      double log_i2 = log (i2);
                      double gamma_val = (log_v1 - log_v2) / (log_i1 - log_i2);
                      info[j].original_brightness = exp (log_v2 - gamma_val * log_i2);
                    }

                  debug_log ("[FADE] fade-in after interruption: calculated original brightness from gamma ramp: %.3f for %s",
                            info[j].original_brightness, info[j].output_name);
                }
              else
                {
                  /* Fallback: assume original brightness is 1.0 (full brightness at gamma=1.0) */
                  info[j].original_brightness = 1.0;
                  debug_log ("[FADE] fade-in after interruption: using default brightness 1.0 for %s (could not calculate from gamma ramp)",
                            info[j].output_name);
                }

              /* Query current brightness (the interrupted level) */
              double current_queried = query_brightness (dpy, info[j].output);
              if (current_queried >= 0.0)
                {
                  info[j].current_brightness = current_queried;
                  debug_log ("[FADE] fade-in after interruption: current (interrupted) brightness for %s: %.3f",
                            info[j].output_name, info[j].current_brightness);
                }
              else
                {
                  /* If we can't query, use interrupted_ratio as approximation */
                  info[j].current_brightness = start_ratio;
                  debug_log ("[FADE] fade-in after interruption: using interrupted_ratio %.3f as current brightness for %s (query failed)",
                            start_ratio, info[j].output_name);
                }
            }
          else
            {
              /* Fade-out (always starts from non-black) or fade-in from black (fade-out completed):
                 Query the current brightness, which represents the brightness at the current gamma level.

                 For fade-out: current gamma is 1.0, so this is the original brightness.
                 For fade-in from black: current gamma is 0.0, so this should be 0.0 or very low.
                 */
              double queried_brightness = query_brightness (dpy, info[j].output);
              if (queried_brightness >= 0.0)
                {
                  info[j].original_brightness = queried_brightness;
                  info[j].current_brightness = queried_brightness;
                }
            }

          /* Only log "captured original gamma" during fade-out.
             During fade-in, we're using the gamma ramp (either restored from interrupted fade-out
             or the current black gamma ramp), but we didn't "capture" it - it was already captured
             during fade-out or is the current state. */
          if (out_p)
            {
              if (info[j].original_brightness >= 0.0)
                debug_log ("[FADE] captured original gamma for screen %d (crtc=%lu, output=%s, brightness=%.2f, size=%d, first_red=%d, first_green=%d, first_blue=%d)",
                          j, (unsigned long) crtc, info[j].output_name, info[j].original_brightness,
                          info[j].gamma->size,
                          info[j].gamma->red[0], info[j].gamma->green[0], info[j].gamma->blue[0]);
              else
                debug_log ("[FADE] captured original gamma for screen %d (crtc=%lu, output=%s, brightness=invalid, size=%d, first_red=%d, first_green=%d, first_blue=%d)",
                          j, (unsigned long) crtc, info[j].output_name,
                          info[j].gamma->size,
                          info[j].gamma->red[0], info[j].gamma->green[0], info[j].gamma->blue[0]);
            }

          /* #### is this test sufficient? */
          info[j].enabled_p = (rroi->connection != RR_Disconnected);

# if 0
          if (verbose_p > 1 && out_p)
            {
              int m;
              fprintf (stderr, "%s: initial gamma ramps, size %d:\n",
                       blurb(), info[j].gamma->size);
              fprintf (stderr, "%s:   R:", blurb());
              for (m = 0; m < info[j].gamma->size; m++)
                fprintf (stderr, " %d", info[j].gamma->red[m]);
              fprintf (stderr, "\n%s:   G:", blurb());
              for (m = 0; m < info[j].gamma->size; m++)
                fprintf (stderr, " %d", info[j].gamma->green[m]);
              fprintf (stderr, "\n%s:   B:", blurb());
              for (m = 0; m < info[j].gamma->size; m++)
                fprintf (stderr, " %d", info[j].gamma->blue[m]);
              fprintf (stderr, "\n");
            }
# endif /* 0 */

          XRRFreeOutputInfo (rroi);
        }
      XRRFreeScreenResources (res);
    }

  /* If we're fading in (from black), then first set gamma to the start level.
     If interrupted_ratio > 0.0 (fade-out was interrupted), start from that level.
     Otherwise, start from 0.0 (fully black).
  */
  if (!out_p)
    {
      double initial_ratio = (interrupted_ratio && *interrupted_ratio > 0.0) ? *interrupted_ratio : 0.0;
      if (interrupted_ratio && *interrupted_ratio > 0.0)
        debug_log ("[FADE] fade-in: setting initial gamma to interrupted level %.2f (not 0.0)", *interrupted_ratio);
      else
        debug_log ("[FADE] fade-in: setting initial gamma to 0.0 (fade-out completed)");
      for (screen = 0; screen < nscreens; screen++)
        randr_whack_gamma(dpy, screen, &info[screen], initial_ratio);
      for (screen = 0; screen < nwindows; screen++)
        {
          debug_log ("[FADE] RANDR fade-in: unmapping saver window 0x%lx", (unsigned long) saver_windows[screen]);
          XUnmapWindow (dpy, saver_windows[screen]);
          XClearWindow (dpy, saver_windows[screen]);
          XSync(dpy, False);
        }
    }

  /* Run the animation at the maximum frame rate in the time allotted. */
  Bool reversed_p = False;  /* Track if we reversed direction due to user activity */
  {
    double start_time = double_time();
    double end_time = start_time + seconds;
    double prev = 0;
    double now;
    int frames = 0;
    int last_logged_percent = -1;
    double max = 1/60.0;  /* max FPS */
    double reversal_time = 0.0;  /* Time when we reversed direction */
    double reversal_ratio = 0.0;  /* Ratio when we reversed direction */

    if (!out_p && interrupted_ratio && *interrupted_ratio > 0.0)
      debug_log ("[FADE] fade-in starting from captured level: %.2f", *interrupted_ratio);

    while ((now = double_time()) < end_time)
      {
        double ratio;

        if (reversed_p)
          {
            /* We reversed direction: fade-in from the reversal point back to 1.0 */
            double elapsed_since_reversal = now - reversal_time;
            double remaining_time = end_time - reversal_time;
            if (remaining_time > 0.0)
              {
                /* Fade from reversal_ratio back to 1.0 over remaining time */
                double fade_progress = elapsed_since_reversal / remaining_time;
                if (fade_progress > 1.0) fade_progress = 1.0;
                ratio = reversal_ratio + (1.0 - reversal_ratio) * fade_progress;
              }
            else
              {
                ratio = 1.0;  /* Already at end time, go to full brightness */
              }
          }
        else if (!out_p)
          {
            /* Normal fade-in */
            ratio = (end_time - now) / seconds;
            ratio = 1-ratio;
            if (interrupted_ratio && *interrupted_ratio > 0.0)
              ratio = *interrupted_ratio + (1.0 - *interrupted_ratio) * ratio;
          }
        else
          {
            /* Normal fade-out */
            ratio = (end_time - now) / seconds;
          }

        /* Continuously update interrupted_ratio during fade-out so it's current if interrupted */
        if (out_p && from_desktop_p && !reversed_p && interrupted_ratio)
          *interrupted_ratio = ratio;

        log_fade_progress ("RANDR gamma", ratio, reversed_p ? False : out_p, &last_logged_percent, dpy, info, nscreens);

        for (screen = 0; screen < nscreens; screen++)
          {
            if (!info[screen].enabled_p)
              continue;

            randr_whack_gamma (dpy, screen, &info[screen], ratio);

            /* Update tracked brightness after setting gamma (brightness is calculated from gamma) */
            if (info[screen].output)
              {
                double calculated_brightness = query_brightness (dpy, info[screen].output);
                if (calculated_brightness >= 0.0)
                  info[screen].current_brightness = calculated_brightness;
              }
          }

        if (error_handler_hit_p)
          goto FAIL;

        /* Check for user activity during fade-out INTO screensaver (from_desktop_p).
           If detected, reverse direction and fade back to full brightness. */
        if (out_p && from_desktop_p && !reversed_p && user_active_p (app, dpy, True))
          {
            /* Reverse direction: switch from fade-out to fade-in */
            reversed_p = True;
            reversal_time = now;
            reversal_ratio = ratio;
            debug_log ("[FADE] fade-out interrupted at ratio %.2f, reversing direction to fade-in", ratio);
            /* Extend end_time to give us time to fade back in */
            end_time = now + seconds;
            /* Capture the interrupted ratio for reporting (if needed) */
            if (interrupted_ratio)
              *interrupted_ratio = ratio;
            /* Continue the loop to fade back in */
          }

        frames++;

        if (now < prev + max)
          usleep (1000000 * (prev + max - now));
        prev = now;
      }

    DL(2, "%.0f FPS", frames / (now - start_time));
  }

  status = 0;   /* completed fade (may have reversed direction) */

  /* If we reversed direction, we're now at fade-in completion, so treat it as fade-in */
  Bool actually_faded_in = (!out_p) || reversed_p;

  if (out_p && !reversed_p && status != 1)
    {
      for (screen = 0; screen < nwindows; screen++)
        {
          debug_log ("[FADE] RANDR fade-out: clearing and map-raising saver window 0x%lx", (unsigned long) saver_windows[screen]);
          XClearWindow (dpy, saver_windows[screen]);
          XMapRaised (dpy, saver_windows[screen]);
        }
      XSync(dpy, False);
    }

  /* I can't explain this; without this delay, we get a flicker.
     I suppose there's some lossage with stale bits being in the
     hardware frame buffer or something, and this delay gives it
     time to flush out.  This sucks! */
  /* #### That comment was about XF86, not verified with randr. */
  usleep(100000);  /* 1/10th second */

  /* For fade-in (or reversed fade-out), always restore gamma to 1.0 (full brightness).
     For fade-out that wasn't reversed, only restore if not interrupted (preserve interrupted fade level). */
  if (actually_faded_in)
    {
      /* Fade-in: always restore to full brightness */
      debug_log ("[FADE] fade-in completed: restoring gamma to 1.0 (full brightness)");
      for (screen = 0; screen < nscreens; screen++)
        {
          if (info[screen].enabled_p)
            {
              debug_log ("[FADE] fade-in: setting gamma to 1.0 for %s (original brightness was %.3f)",
                        info[screen].output_name ? info[screen].output_name : "unknown",
                        info[screen].original_brightness >= 0.0 ? info[screen].original_brightness : -1.0);
              randr_whack_gamma (dpy, screen, &info[screen], 1.0);
            }
        }
      XSync(dpy, False);

      /* Query gamma after setting to verify it's actually 1.0 */
      for (screen = 0; screen < nscreens; screen++)
        {
          if (info[screen].enabled_p)
            {
              double verify_gamma = query_actual_gamma_ratio (dpy, &info[screen]);
              debug_log ("[FADE] fade-in: after setting gamma to 1.0, queried gamma for %s is %.3f",
                        info[screen].output_name ? info[screen].output_name : "unknown", verify_gamma);
            }
        }

      /* Restore brightness to original value (not necessarily 1.0) for all outputs using xrandr command */
      for (screen = 0; screen < nscreens; screen++)
        {
          if (info[screen].enabled_p && info[screen].output_name)
            {
              /* Restore to original brightness if we captured it, otherwise use 1.0 */
              double target_brightness = (info[screen].original_brightness >= 0.0)
                                        ? info[screen].original_brightness
                                        : 1.0;
              int ret = set_brightness_via_xrandr (info[screen].output_name, target_brightness);
              if (ret == 0)
                {
                  info[screen].current_brightness = target_brightness;
                  if (info[screen].original_brightness >= 0.0)
                    debug_log ("[FADE] fade-in: restored brightness to %.3f (original) for output %s",
                              target_brightness, info[screen].output_name);
                  else
                    debug_log ("[FADE] fade-in: restored brightness to 1.0 (default, original was invalid) for output %s",
                              info[screen].output_name);
                }
              else
                {
                  debug_log ("[FADE] fade-in: failed to restore brightness for output %s (xrandr returned %d)", info[screen].output_name, ret);
                }
            }
        }
      /* Verify gamma was restored by querying actual value and compare brightness */
      for (screen = 0; screen < nscreens; screen++)
        {
          if (info[screen].enabled_p)
            {
              double actual_gamma = query_actual_gamma_ratio (dpy, &info[screen]);
              double current_brightness = query_brightness (dpy, info[screen].output);
              if (current_brightness < 0.0)
                current_brightness = info[screen].current_brightness;

              const char *output_name = info[screen].output_name ? info[screen].output_name : "unknown";

              if (actual_gamma >= 0.0)
                {
                  if (current_brightness >= 0.0)
                    {
                      /* Compare with original brightness */
                      if (fabs (current_brightness - info[screen].original_brightness) < 0.01)
                        debug_log ("[FADE] fade-in: verified gamma=%.2f brightness=%.2f for %s (MATCHES original brightness %.2f)",
                                  actual_gamma, current_brightness, output_name, info[screen].original_brightness);
                      else
                        debug_log ("[FADE] fade-in: verified gamma=%.2f brightness=%.2f for %s (DIFFERS from original brightness %.2f)",
                                  actual_gamma, current_brightness, output_name, info[screen].original_brightness);
                    }
                  else
                    {
                      debug_log ("[FADE] fade-in: verified gamma=%.2f for %s (brightness query failed)",
                                actual_gamma, output_name);
                    }
                }
            }
        }
    }
  else if (status != 1)
    {
      /* Fade-out completed: restore to full brightness */
      for (screen = 0; screen < nscreens; screen++)
        randr_whack_gamma (dpy, screen, &info[screen], 1.0);
      XSync(dpy, False);
    }
  /* else: fade-out was interrupted, preserve the interrupted fade level */

 FAIL:
  if (info)
    {
      for (screen = 0; screen < nscreens; screen++)
        {
          if (info[screen].gamma) XRRFreeGamma (info[screen].gamma);
          if (info[screen].output_name) free (info[screen].output_name);
        }
      free(info);
    }

  if (status)
    DL(2, "randr fade %s failed", (out_p ? "out" : "in"));

  return status;
}


static void
randr_whack_gamma (Display *dpy, int screen, randr_gamma_info *info,
                   float ratio)
{
  XErrorHandler old_handler;
  XRRCrtcGamma *g2;
  int i;

  XSync (dpy, False);
  error_handler_hit_p = False;
  old_handler = XSetErrorHandler (ignore_all_errors_ehandler);

  if (ratio < 0) ratio = 0;
  if (ratio > 1) ratio = 1;

  g2 = XRRAllocGamma (info->gamma->size);
  for (i = 0; i < info->gamma->size; i++)
    {
      g2->red[i]   = ratio * info->gamma->red[i];
      g2->green[i] = ratio * info->gamma->green[i];
      g2->blue[i]  = ratio * info->gamma->blue[i];
    }

  XRRSetCrtcGamma (dpy, info->crtc, g2);
  XRRFreeGamma (g2);

  XSync (dpy, False);
  XSetErrorHandler (old_handler);
  XSync (dpy, False);

  const char *output_name = info->output_name ? info->output_name : "unknown";

  if (error_handler_hit_p)
    {
      debug_log ("[FADE] XRRSetCrtcGamma FAILED for %s (screen=%d, crtc=%lu, ratio=%.2f) - X error occurred",
                 output_name, screen, (unsigned long) info->crtc, ratio);
    }
  else if (ratio == 1.0)
    {
      debug_log ("[FADE] XRRSetCrtcGamma succeeded for %s (screen=%d, crtc=%lu, ratio=1.0) - gamma restored to original",
                 output_name, screen, (unsigned long) info->crtc);
    }
  else if (verbose_p > 1)
    {
      debug_log ("[FADE] XRRSetCrtcGamma succeeded for %s (screen=%d, crtc=%lu, ratio=%.2f)",
                 output_name, screen, (unsigned long) info->crtc, ratio);
    }
}

#endif /* HAVE_RANDR_12 */


/****************************************************************************

    XSHM or OpenGL screenshot fading
    This module does one or the other, depending on USE_GL

 ****************************************************************************/

typedef struct {
  Window window;
  XImage *src;
  Pixmap screenshot;
# ifndef USE_GL
  XImage *intermediate;
  GC gc;
# else /* USE_GL */
  GLuint texid;
  GLfloat texw, texh;
#  ifdef HAVE_EGL
  egl_data   *glx_context;
#  else
  GLXContext glx_context;
#  endif
# endif /* USE_GL */
} xshm_fade_info;


#ifdef USE_GL
static int opengl_whack (Display *, xshm_fade_info *, float ratio);
#else
static int xshm_whack (Display *, XShmSegmentInfo *,
                       xshm_fade_info *, float ratio);
#endif

/* Grab a screenshot and return it.
   It will be the size and extent of the given window.
   Or None if we failed.
   Somewhat duplicated in screenshot.c:screenshot_grab().
 */
static Pixmap
xshm_screenshot_grab (Display *dpy, Window window,
                      Bool from_desktop_p, Bool verbose_p)
{
  XWindowAttributes xgwa;
  Pixmap pixmap = 0;
  Bool external_p = False;
  double start = double_time();

# ifdef HAVE_MACOS_X11
  external_p = True;
# else  /* X11, possibly Wayland */
  external_p = getenv ("WAYLAND_DISPLAY") || getenv ("WAYLAND_SOCKET");
# endif

  if (from_desktop_p)
    {
      Pixmap p = screenshot_load (dpy, window, verbose_p);
      if (p) return p;
    }

  XGetWindowAttributes (dpy, window, &xgwa);
  pixmap = XCreatePixmap (dpy, window, xgwa.width, xgwa.height, xgwa.depth);
  if (!pixmap) return None;

  XSync (dpy, False);  /* So that the pixmap exists before we exec. */

  /* Run xscreensaver-getimage to get a screenshot.  It will, in turn,
     run "screencapture" or "grim" to write the screenshot to a PNG file,
     then will load that file onto our Pixmap.
   */
  if (external_p)
    {
      pid_t forked;
      char *av[20];
      int ac = 0;
      char buf[1024];
      char rootstr[20], pixstr[20];

      sprintf (rootstr, "0x%0lx", (unsigned long) window);
      sprintf (pixstr,  "0x%0lx", (unsigned long) pixmap);
      av[ac++] = "xscreensaver-getimage";
      if (verbose_p)
        av[ac++] = "--verbose";
      av[ac++] = "--desktop";
      if (!from_desktop_p)
        av[ac++] = "--no-cache";
      av[ac++] = "--no-images";
      av[ac++] = "--no-video";
      av[ac++] = rootstr;
      av[ac++] = pixstr;
      av[ac] = 0;

      if (verbose_p)
        {
          int i;
          char cmd_buf[512] = "";
          for (i = 0; i < ac; i++)
            {
              if (i > 0) strcat (cmd_buf, " ");
              strcat (cmd_buf, av[i]);
            }
          DL(0, "fade: executing: %s", cmd_buf);
        }

      switch ((int) (forked = fork ())) {
      case -1:
        {
          sprintf (buf, "%s: couldn't fork", blurb());
          perror (buf);
          return 0;
        }
      case 0:
        {
          close (ConnectionNumber (dpy));	/* close display fd */
          execvp (av[0], av);			/* shouldn't return. */
          exit (-1);				/* exits fork */
          break;
        }
      default:
        {
          int wait_status = 0, exit_status = 0;
          /* Wait for the child to die. */
          waitpid (forked, &wait_status, 0);
          exit_status = WEXITSTATUS (wait_status);
          /* Treat exit code as a signed 8-bit quantity. */
          if (exit_status & 0x80) exit_status |= ~0xFF;
          if (exit_status != 0)
            {
              DL(0, "fade: %s exited with %d", av[0], exit_status);
              if (pixmap) XFreePixmap (dpy, pixmap);
              return None;
            }
        }
      }
    }
# ifndef HAVE_MACOS_X11
  else
    {
      /* Grab a screenshot using XCopyArea. */

      XGCValues gcv;
      GC gc;
      gcv.function = GXcopy;
      gcv.subwindow_mode = IncludeInferiors;
      gc = XCreateGC (dpy, pixmap, GCFunction | GCSubwindowMode, &gcv);
      XCopyArea (dpy, xgwa.root, pixmap, gc,
                 xgwa.x, xgwa.y, xgwa.width, xgwa.height, 0, 0);
      XFreeGC (dpy, gc);
    }
# endif /* HAVE_MACOS_X11 */

  {
    double elapsed = double_time() - start;
    char late[100];
    if (elapsed >= 0.75)
      sprintf (late, " -- took %.1f seconds", elapsed);
    else
      *late = 0;

    if (verbose_p || !pixmap || *late)
      DL(0, "%s screenshot 0x%lx %dx%d for window 0x%lx%s",
         (pixmap ? "saved" : "failed to save"),
         (unsigned long) pixmap, xgwa.width, xgwa.height,
         (unsigned long) window,
         late);
  }

  return pixmap;
}


#ifdef USE_GL
static void
opengl_make_current (Display *dpy, xshm_fade_info *info)
{
# ifdef HAVE_EGL
  egl_data *d = info->glx_context;
  if (! eglMakeCurrent (d->egl_display, d->egl_surface, d->egl_surface,
                        d->egl_context))
    abort();
# else /* !HAVE_EGL */
  glXMakeCurrent (dpy, info->window, info->glx_context);
# endif /* !HAVE_EGL */

}

/* report a GL error. */
static Bool
check_gl_error (const char *type)
{
  char buf[100];
  GLenum i;
  const char *e;
  switch ((i = glGetError())) {
  case GL_NO_ERROR: return False;
  case GL_INVALID_ENUM:          e = "invalid enum";      break;
  case GL_INVALID_VALUE:         e = "invalid value";     break;
  case GL_INVALID_OPERATION:     e = "invalid operation"; break;
  case GL_STACK_OVERFLOW:        e = "stack overflow";    break;
  case GL_STACK_UNDERFLOW:       e = "stack underflow";   break;
  case GL_OUT_OF_MEMORY:         e = "out of memory";     break;
#ifdef GL_INVALID_FRAMEBUFFER_OPERATION
  case GL_INVALID_FRAMEBUFFER_OPERATION:
    e = "invalid framebuffer operation";
    break;
#endif
#ifdef GL_TABLE_TOO_LARGE_EXT
  case GL_TABLE_TOO_LARGE_EXT:   e = "table too large";   break;
#endif
#ifdef GL_TEXTURE_TOO_LARGE_EXT
  case GL_TEXTURE_TOO_LARGE_EXT: e = "texture too large"; break;
#endif
  default:
    e = buf; sprintf (buf, "unknown error %d", (int) i); break;
  }
  DL(0, "%s error: %s", type, e);
  return True;
}
#endif /* USE_GL */


/* Returns:
   0: faded normally
   1: canceled by user activity
  -1: unknown error
 */
static int
xshm_fade (XtAppContext app, Display *dpy,
           Window *saver_windows, int nwindows, double seconds,
           Bool out_p, Bool from_desktop_p, fade_state *state)
{
  int screen;
  int status = -1;
  xshm_fade_info *info = 0;
# ifndef USE_GL
  XShmSegmentInfo shm_info;
# endif
  Window saver_window = 0;
  XErrorHandler old_handler = 0;

  XSync (dpy, False);
  old_handler = XSetErrorHandler (ignore_all_errors_ehandler);
  error_handler_hit_p = False;

# ifdef USE_GL
  DL(2, "GL fade %s", (out_p ? "out" : "in"));
# else /* !USE_GL */
  DL(2, "SHM fade %s", (out_p ? "out" : "in"));
# endif /* !USE_GL */

  info = (xshm_fade_info *) calloc(nwindows, sizeof(*info));
  if (!info) goto FAIL;

  saver_window = find_screensaver_window (dpy, 0);
  if (!saver_window) goto FAIL;

  /* Retrieve a screenshot of the area covered by each window.
     Windows might not be mapped.
     Bug out and return -1 if we can't get one for some screen.
  */

  for (screen = 0; screen < nwindows; screen++)
    {
      XWindowAttributes xgwa;
      Window root;
      Visual *visual;
      XSetWindowAttributes attrs;
      unsigned long attrmask = 0;
# ifndef USE_GL
      XGCValues gcv;
# endif /* USE_GL */

      XGetWindowAttributes (dpy, saver_windows[screen], &xgwa);
      root = RootWindowOfScreen (xgwa.screen);
# ifdef USE_GL
      visual = get_gl_visual (xgwa.screen);
# else /* !USE_GL */
      visual = xgwa.visual;
# endif /* !USE_GL */

# ifdef USE_GL
      info[screen].src =
        XCreateImage (dpy, visual, 32, ZPixmap, 0, NULL,
                      xgwa.width, xgwa.height, 32, 0);
      if (!info[screen].src) goto FAIL;
      info[screen].src->data = (char *)
        malloc (info[screen].src->height * info[screen].src->bytes_per_line);
      info[screen].src->bitmap_bit_order =
        info[screen].src->byte_order = MSBFirst;

      while (glGetError() != GL_NO_ERROR)
        ;  /* Flush */

# else /* !USE_GL */
      info[screen].src =
        create_xshm_image (dpy, visual, xgwa.depth,
                           ZPixmap, &shm_info, xgwa.width, xgwa.height);
      if (!info[screen].src) goto FAIL;

      info[screen].intermediate =
        create_xshm_image (dpy, visual, xgwa.depth,
                           ZPixmap, &shm_info, xgwa.width, xgwa.height);
      if (!info[screen].intermediate) goto FAIL;
# endif /* !USE_GL */

      if (!out_p)
        {
          /* If we are fading in, retrieve the saved screenshot from
             before we faded out. */
          if (state->nscreens <= screen) goto FAIL;
          info[screen].screenshot = state->screenshots[screen];
        }
      else
        {
          info[screen].screenshot =
            xshm_screenshot_grab (dpy, saver_windows[screen], from_desktop_p,
                                  verbose_p);
          if (!info[screen].screenshot) goto FAIL;
        }

      /* Create the fader window for the animation. */
      attrmask = CWOverrideRedirect;
      attrs.override_redirect = True;
      info[screen].window =
        XCreateWindow (dpy, root, xgwa.x, xgwa.y,
                       xgwa.width, xgwa.height, xgwa.border_width, xgwa.depth,
                       InputOutput, visual,
                       attrmask, &attrs);
      if (!info[screen].window) goto FAIL;
      /* XSelectInput (dpy, info[screen].window,
                       KeyPressMask | ButtonPressMask); */

# ifdef USE_GL
      /* Copy the screenshot pixmap to the texture XImage */
      XGetSubImage (dpy, info[screen].screenshot,
                    0, 0, xgwa.width, xgwa.height,
                    ~0L, ZPixmap, info[screen].src, 0, 0);

      /* Convert 0RGB to RGBA */
      {
        XImage *ximage = info[screen].src;
        int x, y;
        for (y = 0; y < ximage->height; y++)
          for (x = 0; x < ximage->width; x++)
            {
              unsigned long p = XGetPixel (ximage, x, y);
              unsigned long a = 0xFF;
           /* unsigned long a = (p >> 24) & 0xFF; */
              unsigned long r = (p >> 16) & 0xFF;
              unsigned long g = (p >>  8) & 0xFF;
              unsigned long b = (p >>  0) & 0xFF;
              p = (r << 24) | (g << 16) | (b << 8) | (a << 0);
              XPutPixel (ximage, x, y, p);
            }
      }

      /* Connect the window to an OpenGL context */
      info[screen].glx_context =
        openGL_context_for_window (xgwa.screen, info[screen].window);
      opengl_make_current (dpy, &info[screen]);
      if (check_gl_error ("connect")) goto FAIL;

      glEnable (GL_TEXTURE_2D);
      glEnable (GL_NORMALIZE);
      glGenTextures (1, &info[screen].texid);
      glBindTexture (GL_TEXTURE_2D, info[screen].texid);
      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
      glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
      glPixelStorei (GL_UNPACK_ALIGNMENT, 1);
      if (check_gl_error ("bind texture")) goto FAIL;

      {
        int tex_width  = (GLsizei) to_pow2 (info[screen].src->width);
        int tex_height = (GLsizei) to_pow2 (info[screen].src->height);
        /* Create power of 2 empty texture */
        glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, tex_width, tex_height, 0,
                      GL_RGBA, GL_UNSIGNED_BYTE, 0);
        if (check_gl_error ("glTexImage2D")) goto FAIL;
        /* Load our non-power-of-2 image data into it. */
        glTexSubImage2D (GL_TEXTURE_2D, 0, 0, 0,
                         info[screen].src->width, info[screen].src->height,
                           GL_RGBA, GL_UNSIGNED_BYTE, info[screen].src->data);
        if (check_gl_error ("glTexSubImage2D")) goto FAIL;
        info[screen].texw = info[screen].src->width  / (GLfloat) tex_width;
        info[screen].texh = info[screen].src->height / (GLfloat) tex_height;
      }

      glViewport (0, 0, xgwa.width, xgwa.height);
      glMatrixMode(GL_PROJECTION);
      glLoadIdentity();
      glOrtho (0, 1, 1, 0, -1, 1);
      glMatrixMode (GL_MODELVIEW);
      glLoadIdentity();
      glClearColor (0, 0, 0, 1);
      glClear (GL_COLOR_BUFFER_BIT);
      glFrontFace (GL_CCW);
      if (check_gl_error ("GL setup")) goto FAIL;

# else /* !USE_GL */
      /* Copy the screenshot pixmap to the source image */
      if (! get_xshm_image (dpy, info[screen].screenshot, info[screen].src,
                            0, 0, ~0L, &shm_info))
        goto FAIL;

      gcv.function = GXcopy;
      info[screen].gc = XCreateGC (dpy, info[screen].window, GCFunction, &gcv);
# endif /* !USE_GL */
    }

  /* If we're fading out from the desktop, save our screen shots for later use.
     But not if we're fading out from the savers to black.  In that case we
     don't want to overwrite the desktop screenshot with the current screenshot
     which is of the final frames of the just-killed graphics hacks. */
  if (from_desktop_p)
    {
      if (!out_p) abort();
      for (screen = 0; screen < state->nscreens; screen++)
        if (state->screenshots[screen])
          XFreePixmap (dpy, state->screenshots[screen]);
      if (state->screenshots)
        free (state->screenshots);
      state->nscreens = nwindows;
      state->screenshots = calloc (nwindows, sizeof(*state->screenshots));
      if (!state->screenshots)
        state->nscreens = 0;
      for (screen = 0; screen < state->nscreens; screen++)
        state->screenshots[screen] = info[screen].screenshot;
    }

  for (screen = 0; screen < nwindows; screen++)
    {
# ifndef USE_GL
      if (out_p)
        /* Copy the screenshot to the fader window */
        XSetWindowBackgroundPixmap (dpy, info[screen].window,
                                    info[screen].screenshot);
      else
        {
          XSetWindowBackgroundPixmap (dpy, info[screen].window, None);
          XSetWindowBackground (dpy, info[screen].window, BlackPixel (dpy, 0));
        }
# endif /* USE_GL */

      debug_log ("[FADE] XSHM/OpenGL: map-raising fader window 0x%lx (screen %d)", (unsigned long) info[screen].window, screen);
      XMapRaised (dpy, info[screen].window);

      /* Now that we have mapped the screenshot on the fader windows,
         take the saver windows off the screen. */
# if 0
      /* Under macOS X11 and Wayland, this causes a single frame of flicker.
         I don't see how that is possible, since we just did XMapRaised,
         above. */
      if (out_p)
        {
          XUnmapWindow (dpy, saver_windows[screen]);
          XClearWindow (dpy, saver_windows[screen]);
        }
# endif
    }

  /* Run the animation at the maximum frame rate in the time allotted. */
  {
    double start_time = double_time();
    double end_time = start_time + seconds;
    double prev = 0;
    double now;
    int frames = 0;
    int last_logged_percent = -1;
    double max = 1/60.0;  /* max FPS */
    double *interrupted_ratio = state->interrupted_ratio;
    if (!out_p && interrupted_ratio && *interrupted_ratio > 0.0)
      debug_log ("[FADE] fade-in starting from captured level: %.2f", *interrupted_ratio);
    while ((now = double_time()) < end_time)
      {
        double ratio = (end_time - now) / seconds;
        if (!out_p)
          {
            /* For fade-in, adjust to start from interrupted_ratio if provided */
            ratio = 1-ratio;
            if (interrupted_ratio && *interrupted_ratio > 0.0)
              ratio = *interrupted_ratio + (1.0 - *interrupted_ratio) * ratio;
          }

        /* Continuously update interrupted_ratio during fade-out so it's current if interrupted */
        if (out_p && from_desktop_p && interrupted_ratio)
          *interrupted_ratio = ratio;

# ifdef USE_GL
        log_fade_progress ("OpenGL", ratio, out_p, &last_logged_percent, dpy, NULL, 0);
# else /* !USE_GL */
        log_fade_progress ("XSHM", ratio, out_p, &last_logged_percent, dpy, NULL, 0);
# endif /* !USE_GL */

        for (screen = 0; screen < nwindows; screen++)
# ifdef USE_GL
          if (opengl_whack (dpy, &info[screen], ratio))
            goto FAIL;
# else /* !USE_GL */
          if (xshm_whack (dpy, &shm_info, &info[screen], ratio))
            goto FAIL;
# endif /* !USE_GL */

        if (error_handler_hit_p)
          goto FAIL;
        /* Only check for user activity during fade-out INTO screensaver (from_desktop_p).
           Fade-out FROM screensaver and fade-in should NOT be interruptable. */
        if (out_p && from_desktop_p && user_active_p (app, dpy, True))
          {
            status = 1;   /* user activity status code */
            /* Fade-out into screensaver was interrupted, capture the current ratio */
            if (interrupted_ratio)
              {
                *interrupted_ratio = ratio;
                debug_log ("[FADE] fade-out interrupted, capturing fade level: %.2f", ratio);
              }
            goto DONE;
          }
        frames++;

        if (now < prev + max)
          usleep (1000000 * (prev + max - now));
        prev = now;
      }

    DL(2, "%.0f FPS", frames / (now - start_time));
  }

  status = 0;   /* completed fade with no user activity */

 DONE:

  /* If we're fading out, we have completed the transition from what was
     on the screen to black, using our fader windows.  Now raise the saver
     windows and take the fader windows off the screen.  Since they're both
     black, that will be imperceptible.
     However, if fade-out was interrupted, don't raise windows - preserve the interrupted fade level.
   */
  if (out_p && status != 1)
    {
      for (screen = 0; screen < nwindows; screen++)
        {
          debug_log ("[FADE] XSHM/OpenGL fade-out: clearing and map-raising saver window 0x%lx", (unsigned long) saver_windows[screen]);
          XClearWindow (dpy, saver_windows[screen]);
          XMapRaised (dpy, saver_windows[screen]);
          /* Doing this here triggers the same KDE 5 compositor bug that
             defer_XDestroyWindow is to work around. */
          /* if (info[screen].window)
            XUnmapWindow (dpy, info[screen].window); */
        }
    }

  XSync (dpy, False);

 FAIL:

  /* After fading in, take the saver windows off the screen before
     destroying the occluding screenshot windows. */
  if (!out_p)
    {
      for (screen = 0; screen < nwindows; screen++)
        {
          debug_log ("[FADE] XSHM/OpenGL fade-in: unmapping saver window 0x%lx", (unsigned long) saver_windows[screen]);
          XUnmapWindow (dpy, saver_windows[screen]);
          XClearWindow (dpy, saver_windows[screen]);
        }
    }

  if (info)
    {
      for (screen = 0; screen < nwindows; screen++)
        {
# ifdef USE_GL
          if (info[screen].src)
            XDestroyImage (info[screen].src);
          if (info[screen].texid)
            glDeleteTextures (1, &info[screen].texid);
          openGL_destroy_context (dpy, info[screen].glx_context);
# else /* !USE_GL */
          if (info[screen].src)
            destroy_xshm_image (dpy, info[screen].src, &shm_info);
          if (info[screen].intermediate)
            destroy_xshm_image (dpy, info[screen].intermediate, &shm_info);
          if (info[screen].gc)
            XFreeGC (dpy, info[screen].gc);
# endif /* !USE_GL */

          if (info[screen].window)
            defer_XDestroyWindow (app, dpy, info[screen].window);
        }
      free (info);
    }

  /* If fading in, delete the screenshot pixmaps, and the list of them. */
  if (!out_p && saver_window)
    {
      for (screen = 0; screen < state->nscreens; screen++)
        if (state->screenshots[screen])
          XFreePixmap (dpy, state->screenshots[screen]);
      if (state->screenshots)
        free (state->screenshots);
      state->nscreens = 0;
      state->screenshots = 0;
    }

  XSync (dpy, False);
  XSetErrorHandler (old_handler);

  if (error_handler_hit_p) status = -1;
  if (status)
# ifdef HAVE_GL
    DL(2, "GL fade %s failed", (out_p ? "out" : "in"));
# else /* !HAVE_GL */
    DL(2, "SHM fade %s failed", (out_p ? "out" : "in"));
# endif /* !HAVE_GL */

  return status;
}


#ifdef USE_GL

static int
opengl_whack (Display *dpy, xshm_fade_info *info, float ratio)
{
  GLfloat w = info->texw;
  GLfloat h = info->texh;

  opengl_make_current (dpy, info);
  glBindTexture (GL_TEXTURE_2D, info->texid);
  glClear (GL_COLOR_BUFFER_BIT);

  if (ratio < 0) ratio = 0;
  if (ratio > 1) ratio = 1;

  glColor3f (ratio, ratio, ratio);

  glBegin (GL_QUADS);
  glTexCoord2f (0, 0); glVertex3f (0, 0, 0);
  glTexCoord2f (0, h); glVertex3f (0, 1, 0);
  glTexCoord2f (w, h); glVertex3f (1, 1, 0);
  glTexCoord2f (w, 0); glVertex3f (1, 0, 0);
  glEnd();
  glFinish();

# ifdef HAVE_EGL
  if (! eglSwapBuffers (info->glx_context->egl_display,
                        info->glx_context->egl_surface))
    return True;
# else
  glXSwapBuffers (dpy, info->window);
# endif

  if (check_gl_error ("gl whack")) return True;
  return False;
}


#else /* !USE_GL */

static int
xshm_whack (Display *dpy, XShmSegmentInfo *shm_info,
            xshm_fade_info *info, float ratio)
{
  unsigned char *inbits  = (unsigned char *) info->src->data;
  unsigned char *outbits = (unsigned char *) info->intermediate->data;
  unsigned char *end = (outbits +
                        info->intermediate->bytes_per_line *
                        info->intermediate->height);
  unsigned char ramp[256];
  int i;

  XSync (dpy, False);

  if (ratio < 0) ratio = 0;
  if (ratio > 1) ratio = 1;

  for (i = 0; i < sizeof(ramp); i++)
    ramp[i] = i * ratio;
  while (outbits < end)
    *outbits++ = ramp[*inbits++];

  put_xshm_image (dpy, info->window, info->gc, info->intermediate, 0, 0, 0, 0,
                  info->intermediate->width, info->intermediate->height,
                  shm_info);
  XSync (dpy, False);
  return 0;
}

#endif /* !USE_GL */

