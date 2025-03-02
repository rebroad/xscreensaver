/* xscreensaver, Copyright © 1992-2025 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * And remember: X Windows is to graphics hacking as roman numerals are to
 * the square root of pi.
 */

/* Flow of control for the "screenhack" API:

      main
      xscreensaver_function_table->setup_cb => none
      XtAppInitialize
      pick_visual
      init_window
      run_screenhack_table (loops forever)
      !  xscreensaver_function_table.init_cb => HACK_init (once)
         usleep_and_process_events
      !    xscreensaver_function_table.event_cb => HACK_event
      !  xscreensaver_function_table.draw_cb => HACK_draw

   The "xlockmore" API (which is also the OpenGL API) acts like a screenhack
   but then adds another layer underneath that, before eventually running the
   hack's methods.  Flow of control for the "xlockmore" API:

      main
      + xscreensaver_function_table->setup_cb => xlockmore_setup (opt handling)
      XtAppInitialize
      pick_visual
      init_window
      run_screenhack_table (loops forever)
      !  xscreensaver_function_table.init_cb => xlockmore_init (once)
         usleep_and_process_events
      !    xscreensaver_function_table.event_cb => xlockmore_event
      +      xlockmore_function_table.hack_handle_events => HACK_handle_event
      !  xscreensaver_function_table.draw_cb => xlockmore_draw
      +    xlockmore_function_table.hack_init => init_HACK
      +      init_GL (eglCreateContext or glXCreateContext)
      +    xlockmore_function_table.hack_draw => draw_HACK
 */

#ifdef USE_SDL
#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <string.h>
#else // USE_SDL
#include <X11/Intrinsic.h>
#include <X11/IntrinsicP.h>
#include <X11/CoreP.h>
#include <X11/Shell.h>
#include <X11/StringDefs.h>
#include <X11/keysym.h>
#include <X11/extensions/Xinerama.h>
#endif // else USE_SDL

#include <stdio.h>
#include <ctype.h>

#ifdef __sgi
# include <X11/SGIScheme.h>	/* for SgiUseSchemes() */
#endif /* __sgi */

#include "screenhackI.h"
#include "version.h"
#ifdef USE_SDL
#include "xlockmoreI.h"
#else
#include "xmu.h"
#include "vroot.h"
#endif
#include "fps.h"

#ifdef HAVE_RECORD_ANIM
# include "recanim.h"
#endif

#ifndef USE_SDL
#ifndef _XSCREENSAVER_VROOT_H_
# error Error!  You have an old version of vroot.h!  Check -I args.
#endif /* _XSCREENSAVER_VROOT_H_ */
#endif

#ifndef isupper
# define isupper(c)  ((c) >= 'A' && (c) <= 'Z')
#endif
#ifndef _tolower
# define _tolower(c)  ((c) - 'A' + 'a')
#endif


/* This is defined by the SCREENHACK_MAIN() macro via screenhack.h.  */
#ifdef USE_SDL
// TODO - why do we declare this only for SDL?
extern struct xscreensaver_function_table *xscreensaver_function_table = NULL;
#else
extern struct xscreensaver_function_table *xscreensaver_function_table;
#endif

const char *progname;   /* used by hacks in error messages */
const char *progclass;  /* used by ../utils/resources.c */
Bool mono_p;		/* used by hacks */

#ifdef EXIT_AFTER
static time_t exit_after;	/* Exit gracefully after N seconds */
#endif

static XrmOptionDescRec default_options [] = {
  { "-root",	".root",		XrmoptionNoArg, "True" },
  { "-window",	".root",		XrmoptionNoArg, "False" },
  { "-mono",	".mono",		XrmoptionNoArg, "True" },
  { "-install",	".installColormap",	XrmoptionNoArg, "True" },
  { "-noinstall",".installColormap",XrmoptionNoArg, "False" },
  { "-visual",	".visualID",	XrmoptionSepArg, 0 },
  { "-window-id", ".windowID",	XrmoptionSepArg, 0 },
  { "-fps",	    ".doFPS",		XrmoptionNoArg, "True" },
  { "-no-fps",  ".doFPS",		XrmoptionNoArg, "False" },
  { "-fullscreen", ".fullscreen", XrmoptionSepArg, 0 },
  { "-pair",	".pair",		XrmoptionNoArg, "True" },
# ifdef HAVE_RECORD_ANIM
  { "-record-animation", ".recordAnim", XrmoptionSepArg, 0 },
# endif
# ifdef EXIT_AFTER
  { "-exit-after",	".exitAfter",	XrmoptionSepArg, 0 },
# endif

  { 0, 0, 0, 0 }
};

static char *default_defaults[] = {
  ".root:		false",
  "*geometry:		1280x720", /* this should be .geometry, but noooo... */
  "*mono:		false",
  "*installColormap:	false",
  "*doFPS:		false",
  "*multiSample:	false",
  "*visualID:		default",
  "*windowID:		",
  "*desktopGrabber:	xscreensaver-getimage %s",
  0
};

static XrmOptionDescRec *merged_options;
static int merged_options_size;
static char **merged_defaults;


static void merge_options (void) {
  struct xscreensaver_function_table *ft = xscreensaver_function_table;
  const XrmOptionDescRec *options = ft->options;
  const char * const *defaults = ft->defaults;
  const char *progclass  = ft->progclass;

  int def_opts_size = 0, opts_size = 0, def_defaults_size = 0, defaults_size = 0;
  for (; default_options[def_opts_size].option; def_opts_size++);
  for (; options[opts_size].option; opts_size++);
  merged_options_size = def_opts_size + opts_size;
  merged_options = (XrmOptionDescRec *)
    malloc ((merged_options_size + 1) * sizeof(*default_options));
  memcpy (merged_options, default_options,
      (def_opts_size * sizeof(*default_options)));
  memcpy (merged_options + def_opts_size, options,
      ((opts_size + 1) * sizeof(*default_options)));

  for (; default_defaults[def_defaults_size]; def_defaults_size++);
  for (; defaults[defaults_size]; defaults_size++);
  merged_defaults = (char **)
      malloc ((def_defaults_size + defaults_size + 1) * sizeof (*defaults));
  memcpy(merged_defaults, default_defaults, def_defaults_size * sizeof(*defaults));
  memcpy(merged_defaults + def_defaults_size, defaults,
      (defaults_size + 1) * sizeof(*defaults));

  /* This totally sucks.  Xt should behave like this by default.
     If the string in `defaults' looks like ".foo", change that
     to "Progclass.foo".
   */
  {
    char **s;
    for (s = merged_defaults; *s; s++)
      if (**s == '.') {
        const char *oldr = *s;
        char *newr = (char *) malloc(strlen(oldr) + strlen(progclass) + 3);
        strcpy (newr, progclass);
        strcat (newr, oldr);
        *s = newr;
      } else *s = strdup (*s);
  }
  fprintf(stderr, "%s: Merged %d options:\n", progname, merged_options_size);
  for (int i = 0; i < merged_options_size; i++) {
    fprintf(stderr, "%s:   %s -> %s\n", progname, merged_options[i].option, merged_options[i].specifier);
  }
}

/* Make the X errors print out the name of this program, so we have some
   clue which one has a bug when they die under the screensaver.  */

#ifndef USE_SDL
static int screenhack_ehandler (Display *dpy, XErrorEvent *error) {
  fprintf (stderr, "\nX error in %s:\n", progname);
  if (XmuPrintDefaultErrorMessage (dpy, error, stderr))
    exit (-1);
  else
    fprintf (stderr, " (nonfatal.)\n");
  return 0;
}

static Bool MapNotify_event_p (Display *dpy, XEvent *event, XPointer window) {
  return (event->xany.type == MapNotify &&
      event->xvisibility.window == (Window) window);
}


static Atom XA_WM_PROTOCOLS, XA_WM_DELETE_WINDOW, XA_NET_WM_PID,
  XA_NET_WM_PING;

/* Dead-trivial event handling: exits if "q" or "ESC" are typed.
   Exit if the WM_PROTOCOLS WM_DELETE_WINDOW ClientMessage is received.
   Returns False if the screen saver should now terminate.  */
static Bool screenhack_handle_event_1 (Display *dpy, XEvent *event) {
  switch (event->xany.type) {
    case KeyPress:
      {
        KeySym keysym;
        char c = 0;
        XLookupString (&event->xkey, &c, 1, &keysym, 0);
        if (c == 'q' ||
            c == 'Q' ||
            c == 3 ||	/* ^C */
            c == 27)	/* ESC */
          return False;  /* exit */
        else if (! (keysym >= XK_Shift_L && keysym <= XK_Hyper_R))
          XBell (dpy, 0);  /* beep for non-chord keys */
      }
      break;
    case ButtonPress:
      XBell (dpy, 0);
      break;
    case ClientMessage:
      {
        if (event->xclient.message_type != XA_WM_PROTOCOLS)
          {
            char *s = XGetAtomName(dpy, event->xclient.message_type);
            if (!s) s = "(null)";
            fprintf (stderr, "%s: unknown ClientMessage %s received!\n",
                     progname, s);
          }
        else if (event->xclient.data.l[0] == XA_WM_DELETE_WINDOW)
          {
            return False;  /* exit */
          }
        else if (event->xclient.data.l[0] == XA_NET_WM_PING)
          {
            event->xclient.window = DefaultRootWindow (dpy);
            if (! XSendEvent (dpy, event->xclient.window, False,
                              PropertyChangeMask, event))
              fprintf (stderr, "%s: WM_PONG failed\n", progname);
          }
        else
          {
            char *s1 = XGetAtomName(dpy, event->xclient.message_type);
            char *s2 = XGetAtomName(dpy, event->xclient.data.l[0]);
            if (!s1) s1 = "(null)";
            if (!s2) s2 = "(null)";
            fprintf (stderr, "%s: unknown ClientMessage %s[%s] received!\n",
                     progname, s1, s2);
          }
      }
      break;
  }
  return True;
}


static Visual * pick_visual (Screen *screen) {
  struct xscreensaver_function_table *ft = xscreensaver_function_table;

  if (ft->pick_visual_hook) {
    Visual *v = ft->pick_visual_hook (screen);
    if (v) return v;
  }

  return get_visual_resource (screen, "visualID", "VisualID", False);
}


/* Notice when the user has requested a different visual or colormap
   on a pre-existing window (e.g., "--root --visual truecolor" or
   "--window-id 0x2c00001 --install") and complain, since when drawing
   on an existing window, we have no choice about these things.  */
static void
visual_warning (Screen *screen, Window window, Visual *visual, Colormap cmap,
                Bool window_p) {
  struct xscreensaver_function_table *ft = xscreensaver_function_table;

  char *visual_string = get_string_resource (DisplayOfScreen (screen),
                                             "visualID", "VisualID");
  Visual *desired_visual = pick_visual (screen);
  char win[100];
  char why[100];

  if (window == RootWindowOfScreen (screen))
    strcpy (win, "root window");
  else
    sprintf (win, "window 0x%lx", (unsigned long) window);

  if (window_p)
    sprintf (why, "--window-id 0x%lx", (unsigned long) window);
  else
    strcpy (why, "--root");

  if (visual_string && *visual_string) {
    char *s;
    for (s = visual_string; *s; s++)
      if (isupper (*s)) *s = _tolower (*s);

    if (!strcmp (visual_string, "default") ||
        !strcmp (visual_string, "default") ||
        !strcmp (visual_string, "best"))
      /* don't warn about these, just silently DWIM. */
        ;
    else if (visual != desired_visual) {
      fprintf (stderr, "%s: ignoring `-visual %s' because of `%s'.\n",
                   progname, visual_string, why);
      fprintf (stderr, "%s: using %s's visual 0x%lx.\n",
                   progname, win, XVisualIDFromVisual (visual));
    }
    free (visual_string);
  }

  if (visual == DefaultVisualOfScreen (screen) &&
      has_writable_cells (screen, visual) &&
      get_boolean_resource (DisplayOfScreen (screen),
                            "installColormap", "InstallColormap"))
    {
      fprintf (stderr, "%s: ignoring `-install' because of `%s'.\n",
               progname, why);
      fprintf (stderr, "%s: using %s's colormap 0x%lx.\n",
               progname, win, (unsigned long) cmap);
    }

  if (ft->validate_visual_hook) {
    if (! ft->validate_visual_hook (screen, win, visual)) exit (1);
  }
}


static void fix_fds (void) {
  /* Bad Things Happen if stdin, stdout, and stderr have been closed
     (as by the `sh incantation "attraction >&- 2>&-").  When you do
     that, the X connection gets allocated to one of these fds, and
     then some random library writes to stderr, and random bits get
     stuffed down the X pipe, causing "Xlib: sequence lost" errors.
     So, we cause the first three file descriptors to be open to
     /dev/null if they aren't open to something else already.  This
     must be done before any other files are opened (or the closing
     of that other file will again free up one of the "magic" first
     three FDs.)

     We do this by opening /dev/null three times, and then closing
     those fds, *unless* any of them got allocated as #0, #1, or #2,
     in which case we leave them open.  Gag.

     Really, this crap is technically required of *every* X program,
     if you want it to be robust in the face of "2>&-".  */
  int fd0 = open ("/dev/null", O_RDWR);
  int fd1 = open ("/dev/null", O_RDWR);
  int fd2 = open ("/dev/null", O_RDWR);
  if (fd0 > 2) close (fd0);
  if (fd1 > 2) close (fd1);
  if (fd2 > 2) close (fd2);
}

static Boolean screenhack_table_handle_events (Display *dpy,
                                const struct xscreensaver_function_table *ft,
                                Window *windows, void **closures, int num_windows) {
  XtAppContext app = XtDisplayToApplicationContext(dpy);
  XtInputMask m = XtAppPending(app);

  /* Process non-X11 Xt events (timers, files, signals) without blocking. */
  if (m & ~XtIMXEvent) XtAppProcessEvent (app, ~XtIMXEvent);

  /* Process all pending X11 events without blocking. */
  while (XPending (dpy)) {
    XEvent event;
    XNextEvent(dpy, &event);

    for (int i = 0; i < num_windows; i++) {
      if (event.xany.type == ConfigureNotify) {
        if (event.xany.window == windows[i])
          ft->reshape_cb(dpy, windows[i], closures[i],
                            event.xconfigure.width, event.xconfigure.height);
      } else if (event.xany.type == ClientMessage ||
                (!(event.xany.window == windows[i]
                   ? ft->event_cb(dpy, windows[i], closures[i], &event) : 0))) {
        if (!screenhack_handle_event_1 (dpy, &event)) return False;
      }
    } // for num_windows

    /* Last chance to process Xt timers before blocking. */
    m = XtAppPending(app);
    if (m & ~XtIMXEvent) XtAppProcessEvent(app, ~XtIMXEvent);
  } // while pending

# ifdef EXIT_AFTER
  if (exit_after != 0 && time ((time_t *) 0) >= exit_after) return False;
# endif

  return True;
}


static Boolean usleep_and_process_events(Display *dpy,
                           const struct xscreensaver_function_table *ft,
                           Window *windows, fps_state **fpsts, void **closures, unsigned long delay,
                           int num_windows
#ifdef HAVE_RECORD_ANIM
                         , record_anim_state *anim_state
#endif
                           ) {
  do {
    unsigned long quantum = 33333;  /* 30 fps */
    if (quantum > delay) quantum = delay;
    delay -= quantum;

    XSync (dpy, False);

#ifdef HAVE_RECORD_ANIM
    if (anim_state) screenhack_record_anim(anim_state);
#endif

    if (quantum > 0) {
      usleep(quantum);
      for (int i = 0; i < num_windows; i++)
        if (fpsts[i]) fps_slept(fpsts[i], quantum);
    }

    if (!screenhack_table_handle_events(dpy, ft, windows, closures, num_windows))
      return False;
  } while (delay > 0);

  return True;
}

static void
screenhack_do_fps (Display *dpy, Window w, fps_state *fpst, void *closure) {
  fps_compute (fpst, 0, -1);
  fps_draw (fpst);
}

// Constants
#define FRAME_INTERVAL 1666666  // 60 FPS (1000000 / 60)
#define DEBUG_INTERVAL 1000000  // 1 second

// Global variables for debugging
unsigned long *total_delays;  // Will be allocated in run_screenhack_table
unsigned long frame_count = 0, sleep_times = 0;
int number_of_windows;  // Will be set in run_screenhack_table
struct timespec last_debug_time = {0, 0};

void debug_output(void);
void debug_output(void) {
    struct timespec current_time;
    clock_gettime(CLOCK_MONOTONIC, &current_time);

    if (last_debug_time.tv_sec == 0 && last_debug_time.tv_nsec == 0) {
        last_debug_time = current_time;
    }

    long elapsed_time = (current_time.tv_sec - last_debug_time.tv_sec) * 1000000 +
                        (current_time.tv_nsec - last_debug_time.tv_nsec) / 1000;

    if (elapsed_time >= DEBUG_INTERVAL) {
        for (int i = 0; i < number_of_windows; i++) {
            printf("Avg delay[%d]: %.2f ms, ", i, (double)total_delays[i] / frame_count);
            total_delays[i] = 0;
        }
        printf("Avg sleep: %.2f ms\n", (double)sleep_times / frame_count);
        frame_count = 0; sleep_times = 0;

        if (current_time.tv_sec > last_debug_time.tv_sec + DEBUG_INTERVAL/1000000)
            last_debug_time = current_time;
        else
            last_debug_time.tv_sec += DEBUG_INTERVAL/1000000;
    }
}

static void run_screenhack_table (
#ifdef USE_SDL // TODO what type is dpy?
        void *dpy, SDL_Window **windows, SDL_GLContext **contexts,
#else
        Display *dpy, Window *windows,
#endif
        int window_count,
#ifdef HAVE_RECORD_ANIM
                      record_anim_state *anim_state,
#endif
                      const struct xscreensaver_function_table *ft)
{
#ifdef USE_SDL
  void *(*init_cb)(void *, void *) = ft->init_cb;
  void (*fps_cb)(void *, fps_state *, void *) = ft->fps_cb;
#else
  /* Kludge: even though the init_cb functions are declared to take 2 args,
     actually call them with 3, for the benefit of xlockmore_init() and
     xlockmore_setup().  */
  void *(*init_cb)(Display *, Window, void *) =
    (void *(*)(Display *, Window, void *))ft->init_cb;
  void (*fps_cb)(Display *, Window, fps_state *, void *) = ft->fps_cb;
#endif

  void **closures = (void **)calloc(window_count, sizeof(void *));
  fps_state **fpsts = (fps_state **)calloc(window_count, sizeof(fps_state *));
  /* Entries below for debugging */
  total_delays = (unsigned long *)calloc(window_count, sizeof(unsigned long));
  number_of_windows = window_count;  // Set global for debug_output
  unsigned long *delays = (unsigned long *)calloc(window_count, sizeof(unsigned long));
  unsigned long delay = 0;

  for (int i = 0; i < window_count; i++) {
#ifdef USE_SDL
    closures[i] = init_cb(windows[i], contexts[i]);
#else // USE_SDL
    closures[i] = init_cb(dpy, windows[i], ft->setup_arg);
#endif // else USE_SDL
    if (!closures[i]) abort();
    fpsts[i] = fps_init(dpy, windows[i]);
  }

  if (!fps_cb) fps_cb = screenhack_do_fps;

  while(1) {
    static struct timespec start_time, end_time;
    if (!end_time.tv_sec)
        clock_gettime(CLOCK_MONOTONIC, &start_time);
    else
        start_time = end_time;

#ifdef USE_SDL
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      for (int i = 0; i < window_count; i++) {
        if (!ft->event_cb(windows[i], closures[i], &event) break;
        if (event.type == SDL_EVENT_WINDOW_RESIZED)
          ft->reshape_cb(windows[i], closures[i], event.window,data1, event.window,data2);
      }
    }

    for (int i = 0; i < window_count; i++) {
      SDL_GL_SwapWindow(windows[i]);
      if (delays[i] > 0) SDL_Delay(delays[i] / 1000);
    }
#else // USE_SDL
    if (!usleep_and_process_events(dpy, ft, windows, fpsts, closures, delay, window_count
#ifdef HAVE_RECORD_ANIM
                                       , anim_state
#endif // HAVE_RECORD_ANIM
                                       ))
      break;
#endif // else USE_SDL

#ifdef USE_SDL
#else
    // TODO - no need for glXSwapBuffers(dpy, window); here?
#endif // USE_SDL
    clock_gettime(CLOCK_MONOTONIC, &end_time);

    long elapsed_time = (end_time.tv_sec - start_time.tv_sec) * 1000000 +
                        (end_time.tv_nsec - start_time.tv_nsec) / 1000;

    long sleep_time = FRAME_INTERVAL - elapsed_time;
    sleep_times += sleep_time; frame_count++;
    debug_output();

    for (int i = 0; i < window_count; i++) {
      delays[i] = ft->draw_cb(dpy, windows[i], closures[i]);
      if (!i) delay = delays[0];
      total_delays[i] += delays[i];
      if (fpsts[i]) fps_cb(dpy, windows[i], fpsts[i], closures[i]);
    }

  } // while running

#ifdef HAVE_RECORD_ANIM
  /* Exiting before target frames hit: write the video anyway. */
  if (anim_state) screenhack_record_anim_free (anim_state);
#endif // HAVE_RECORD_ANIM

  for (int i = 0; i < window_count; i++) {
    if (fpsts[i]) ft->fps_free(fpsts[i]);
    ft->free_cb(dpy, windows[i], closures[i]);
  }
  free(closures); free(fpsts); free(total_delays); free(delays);
}

static Widget make_shell (Screen *screen, Widget toplevel, int width, int height) {
  printf("%s: %dx%d\n", __func__, width, height);
  Display *dpy = DisplayOfScreen (screen);
  Visual *visual = pick_visual(screen);
  Boolean def_visual_p = (toplevel && visual == DefaultVisualOfScreen(screen));

  if (width  <= 0) width  = 600;
  if (height <= 0) height = 480;

  Widget new;
  if (def_visual_p) {
    XtVaSetValues(toplevel, XtNmappedWhenManaged, False, XtNwidth, width,
                  XtNheight, height, XtNinput, True,  /* for WM_HINTS */
                  NULL);
    printf("%s: After XtVaSetValues: %sx%s\n", __func__, XtNwidth, XtNheight);
    XtRealizeWidget (toplevel);
    new = toplevel;

    if (get_boolean_resource(dpy, "installColormap", "InstallColormap")) {
      Colormap cmap = XCreateColormap(dpy, RootWindowOfScreen(screen), visual, AllocNone);
      XSetWindowColormap(dpy, XtWindow(new), cmap);
    }
  } else {
    Colormap cmap = XCreateColormap(dpy, VirtualRootWindowOfScreen(screen),
                                       visual, AllocNone);
    unsigned int bg = get_pixel_resource(dpy, cmap, "background", "Background");
    unsigned int bd = get_pixel_resource(dpy, cmap, "borderColor", "Foreground");

    new = XtVaAppCreateShell(progname, progclass, topLevelShellWidgetClass, dpy,
                             XtNmappedWhenManaged, False, XtNvisual, visual,
                             XtNdepth, visual_depth(screen, visual),
                             XtNwidth, width, XtNheight, height, XtNcolormap, cmap,
                             XtNbackground, bg, XtNborderColor, bd,
                             XtNinput, True /* for WM_HINTS */, NULL);
    printf("%s: After XtVaAppCreateShell: %dx%d %sx%s\n", __func__, width, height, XtNwidth, XtNheight);
    if (!toplevel) { /* kludge for the second window in -pair mode... */
      XtVaSetValues (new, XtNx, 0, XtNy, 0, NULL);
      printf("%s: After 2nd XtVaSetValues: %sx%s\n", __func__, XtNx, XtNy);
    }
    XtRealizeWidget(new);
  }

  return new;
}

static void init_window (Display *dpy, Widget toplevel, const char *title) {
  long pid = getpid();
  Window window;
  XWindowAttributes xgwa;
  XtPopup (toplevel, XtGrabNone);
  XtVaSetValues (toplevel, XtNtitle, title, NULL);

  /* Select KeyPress, and announce that we accept WM_DELETE_WINDOW.  */
  window = XtWindow (toplevel);
  XGetWindowAttributes (dpy, window, &xgwa);
  XSelectInput (dpy, window,
                (xgwa.your_event_mask | KeyPressMask | KeyReleaseMask |
                 ButtonPressMask | ButtonReleaseMask));
  XChangeProperty (dpy, window, XA_WM_PROTOCOLS, XA_ATOM, 32,
                   PropModeReplace,
                   (unsigned char *) &XA_WM_DELETE_WINDOW, 1);
  XChangeProperty (dpy, window, XA_NET_WM_PID, XA_CARDINAL, 32,
                   PropModeReplace, (unsigned char *)&pid, 1);
}
#endif // !USE_SDL

int main (int argc, char **argv) {
  printf("%s: %s\n", __FILE__, __func__);
#ifdef USE_SDL
  struct xscreensaver_function_table *ft;
#else
  struct xscreensaver_function_table *ft = xscreensaver_function_table;
#endif

#ifndef _WIN32
  fix_fds();
#endif

  progname = argv[0];   /* reset later */
  progclass = ft->progclass;

#ifdef USE_SDL
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    fprintf(stderr, "%s: SDL initialization failed: %s\n", progname, SDL_GetError());
    return 1;
  }

  if (TTF_Init() < 0) {
    fprintf(stderr, "%s: TTF_Init failed: %s\n", progname, SDL_GetError());
    SDL_Quit();
    return 1;
  }
#endif // USE_SDL

  if (ft->setup_cb) ft->setup_cb (ft, ft->setup_arg);
  merge_options();

  /* Xt and xscreensaver predate the "--arg" convention, so convert
     double dashes to single. */

  for (int i = 1; i < argc; i++)
    if (argv[i][0] == '-' && argv[i][1] == '-') argv[i]++;

#ifdef USE_SDL
  sdl_init_resources(argc, argv, merged_options, merged_defaults);
#else // USE_SDL
  XtAppContext app;
  Widget toplevel = XtAppInitialize(&app, progclass, merged_options,
                 merged_options_size, &argc, argv, merged_defaults, 0, 0);
  Display *dpy = XtDisplay(toplevel);
#endif // else USE_SDL

  char version[255];
  {
    char *v = (char *) strdup(strchr(screensaver_id, ' '));
    char *s1, *s2, *s3, *s4;
    const char *ot = get_string_resource (dpy, "title", "Title");
    s1 = (char *) strchr(v,  ' '); s1++;
    s2 = (char *) strchr(s1, ' ');
    s3 = (char *) strchr(v,  '('); s3++;
    s4 = (char *) strchr(s3, ')');
    *s2 = 0;
    *s4 = 0;
    if (ot && !*ot) ot = 0;
    sprintf (version, "%.50s%s%s: from the XScreenSaver %s distribution (%s)",
             (ot ? ot : ""),
             (ot ? ": " : ""),
         progclass, s1, s3);
    free(v);
  }

  if (argc > 1) {
    int x = 18, end = 78;
    Bool help_p = (!strcmp(argv[1], "-help") || !strcmp(argv[1], "--help"));
    fprintf (stderr, "%s\n\n\thttps://www.jwz.org/xscreensaver/\n\n", version);

    if (!help_p) fprintf(stderr, "Unrecognised option: %s\n", argv[1]);
    fprintf(stderr, "Options include: ");
    for (int i = 0; i < merged_options_size; i++) {
      char *sw = merged_options [i].option;
      Bool argp = (merged_options [i].argKind == XrmoptionSepArg);
      int size = strlen (sw) + (argp ? 6 : 0) + 2;
      if (x + size >= end) {
        fprintf(stderr, "\n\t\t ");
        x = 18;
      }
      x += size;
      fprintf(stderr, "-%s", sw);  /* two dashes */
      if (argp) fprintf(stderr, " <arg>");
      if (i != merged_options_size - 1) fprintf(stderr, ", ");
    }
    fprintf (stderr, ".\n");
    exit(help_p ? 0 : 1);
  }

#ifdef USE_SDL
  // Call the hack's setup function if it exists
  if (xscreensaver_function_table && xscreensaver_function_table->setup_cb) {
    xscreensaver_function_table->setup_cb(xscreensaver_function_table,
            xscreensaver_function_table->setup_arg);
  }

  init_function_table();
  ft = xscreensaver_function_table;

  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

  Bool fullscreen = get_boolean_option("fullscreen");
  int width = 1280, height = 720;
  int num_displays = 0;
  SDL_DisplayID *displays = SDL_GetDisplays(&num_displays);
  if (!displays || num_displays <= 0) {
    fprintf(stderr, "%s:No displays detected: %s\n", progname, SDL_GetError());
    SDL_Quit();
    return 1;
  }

  int window_count = fullscreen ? num_displays : 1;

  SDL_Window **windows = calloc(window_count, sizeof(SDL_Window *));
  SDL_GLContext *contexts = calloc(window_count, sizeof(SDL_GLContext));
  void **closures = calloc(window_count, sizeof(void *));

  for (int i = 0; i < window_count; i++) {
    SDL_Rect bounds;
    Uint32 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;
    if (fullscreen) {
      SDL_GetDisplayBounds(displays[i % num_displays], &bounds);
      flags |= SDL_WINDOW_FULLSCREEN;
      width = bounds.w; height = bounds.y;
    }

    windows[i] = SDL_CreateWindow(progclass, width, height, flags);
    if (!windows[i]) {
      fprintf(stderr, "%s: Window %d creation failed: %s\n", progname, i, SDL_GetError());
      continue;
    }

    contexts[i] = SDL_GL_CreateContext(windows[i]);
    if (!contexts[i]) {
      fprintf(stderr, "%s:GL context %d creation failed: %s\n", progname, i, SDL_GetError());
      SDL_DestroyWindow(windows[i]);
      windows[i] = NULL;
    }
  }

  run_screenhack_table(NULL, window, context, ft);

  for (int i = 0; i < num_displays; i++) {
    if (closures[i]) ft->free_cb(windows[i], closures[i]);
    if (contexts[i]) SDL_GL_DestroyContext(contexts[i]);
    if (windows[i]) SDL_DestroyWindow(windows[i]);
  }

  free(windows); free(contexts); free(closures);
  SDL_free(displays);
  SDL_Quit();

  for (int i = 0; i < options_count; i++) {
    free(options_store[i].name);
    free(options_store[i].value);
  }
  free(options_store);
#else // USE_SDL
# ifdef HAVE_RECORD_ANIM
  record_anim_state *anim_state = 0;
# endif
  XEvent event;
  Boolean dont_clear;

#ifdef __sgi
  /* We have to do this on SGI to prevent the background color from being
     overridden by the current desktop color scheme (we'd like our backgrounds
     to be black, thanks.)  This should be the same as setting the
     "*useSchemes: none" resource, but it's not -- if that resource is
     present in the `default_defaults' above, it doesn't work, though it
     does work when passed as an -xrm arg on the command line.  So screw it,
     turn them off from C instead.  */
  SgiUseSchemes ("none");
#endif /* __sgi */

  XtGetApplicationNameAndClass (dpy, (char **) &progname, (char **) &progclass);

  /* half-assed way of avoiding buffer-overrun attacks. */
  if (strlen (progname) >= 100) ((char *) progname)[100] = 0;

  XSetErrorHandler (screenhack_ehandler);

  XA_WM_PROTOCOLS = XInternAtom (dpy, "WM_PROTOCOLS", False);
  XA_WM_DELETE_WINDOW = XInternAtom (dpy, "WM_DELETE_WINDOW", False);
  XA_NET_WM_PID = XInternAtom (dpy, "_NET_WM_PID", False);
  XA_NET_WM_PING = XInternAtom (dpy, "_NET_WM_PING", False);

  for (char **s = merged_defaults; *s; s++) free(*s);

  free (merged_options); free (merged_defaults);
  merged_options = 0, merged_defaults = 0;

  dont_clear = get_boolean_resource (dpy, "dontClearRoot", "Boolean");
  mono_p = get_boolean_resource (dpy, "mono", "Boolean");
  if (CellsOfScreen (DefaultScreenOfDisplay (dpy)) <= 2) mono_p = True;

  fprintf(stderr, "%s: Parsing options...\n", progname);
  Bool root_p = get_boolean_resource(dpy, "root", "Boolean");
  Bool fullscreen_p = False;
  int fullscreen_display = -1;
  char *fs_val = get_string_resource(dpy, "fullscreen", "Fullscreen");
  fprintf(stderr, "%s: root_p = %d, fs_val = %s\n", progname, root_p, fs_val);
  if (fs_val) {
    if (!strcasecmp(fs_val, "true") || !strcasecmp(fs_val, "on") ||
        !strcasecmp(fs_val, "yes")) {
      fullscreen_p = True;
    } else if (!strcasecmp(fs_val, "all")) {
      fullscreen_p = True;
    } else if (isdigit(fs_val[0])) {
      fullscreen_p = True;
      fullscreen_display = atoi(fs_val);
    }
    free(fs_val);
  }
  printf("%s: fullscreen_p = %d, fullscreen_display = %d\n", progname, fullscreen_p, fullscreen_display);

#ifdef EXIT_AFTER
  int secs = get_integer_resource (dpy, "exitAfter", "Integer");
  exit_after = (secs > 0 ? time((time_t *) 0) + secs : 0);
#endif

  char *s = get_string_resource (dpy, "windowID", "WindowID");
  Window on_window = 0;
  if (s && *s) on_window = get_integer_resource (dpy, "windowID", "WindowID");
  if (s) free (s);

  /* Determine display configuration */
  XWindowAttributes xgwa;
  Window *windows = (Window *)calloc(1, sizeof(Window));
  Widget *toplevels = (Widget *)calloc(1, sizeof(Widget));
  toplevels[0] = toplevel;
  int window_count = 1; // Default to 1
  if (fullscreen_p && !root_p) {
    fprintf(stderr, "%s: Entering fullscreen branch\n", progname);
    int num_screens = 0;
    XineramaScreenInfo *xsi = NULL;
    if (XineramaIsActive(dpy)) {
      xsi = XineramaQueryScreens(dpy, &num_screens);
      fprintf(stderr, "%s: Xinerama active, found %d screens\n", progname, num_screens);
    } else {
      num_screens = ScreenCount(dpy);
      fprintf(stderr, "%s: Xinerama inactive, using %d screens from ScreenCount\n", progname, num_screens);
    }

    if (fullscreen_display >= 0) {
      /* Single display mode */
      if (fullscreen_display >= num_screens) {
        fprintf(stderr, "%s: Display %d not found; only %d displays available, defaulting to 0\n",
                progname, fullscreen_display, num_screens);
        fullscreen_display = 0;  /* TODO - default to primary */
      }
      fprintf(stderr, "%s: Single display mode, window_count = %d, display %d\n",
              progname, window_count, fullscreen_display);
    } else {
      /* All displays mode */
      window_count = num_screens;
      fprintf(stderr, "%s: All displays mode, window count = %d\n", progname, window_count);
    }

    windows = (Window *)realloc(windows, window_count * sizeof(Window));
    toplevels = (Widget *)realloc(toplevels, window_count * sizeof(Widget));

    int def_width = DisplayWidth(dpy, DefaultScreen(dpy));
    int def_height = DisplayHeight(dpy, DefaultScreen(dpy));

    for (int i = 0; i < window_count; i++) {
      int screen_idx = (fullscreen_display >= 0 ? fullscreen_display : i);
      int scr_num = xsi ? xsi[screen_idx].screen_number : screen_idx;
      Screen *screen = ScreenOfDisplay(dpy, scr_num);
      int width = xsi ? xsi[screen_idx].width : def_width;
      int height = xsi ? xsi[screen_idx].height : def_height;
      int x = xsi ? xsi[screen_idx].x_org : (i * 50);  /* Offset for non-Xinerama */
      int y = xsi ? xsi[screen_idx].y_org : (i * 50);

      fprintf(stderr, "%s: Creating window %d: screen_idx=%d, %dx%d at (%d,%d)\n",
              progname, i, screen_idx, width, height, x, y);

      toplevels[i] = make_shell(screen, NULL, width, height);
      init_window(dpy, toplevels[i], version);
      windows[i] = XtWindow(toplevels[i]);

      XMoveResizeWindow(dpy, windows[i], x, y, width, height);
      XSetWindowBorderWidth(dpy, windows[i], 0);
      XMapWindow(dpy, windows[i]);  /* Ensure it's mapped */
      fprintf(stderr, "%s: Window %d created and positioned\n", progname, i);
    }
    if (xsi) XFree(xsi);
  } else if (on_window) {
    fprintf(stderr, "%s: Using existing window ID\n", progname);
    windows[0] = (Window)on_window;
    XtDestroyWidget(toplevel);
    XGetWindowAttributes(dpy, windows[0], &xgwa);
    visual_warning(xgwa.screen, windows[0], xgwa.visual, xgwa.colormap, True);

    /* Select KeyPress and resize events on the external window.  */
    xgwa.your_event_mask |= KeyPressMask | StructureNotifyMask;
    XSelectInput(dpy, windows[0], xgwa.your_event_mask);

    /* Select ButtonPress and ButtonRelease events on the external window,
       if no other app has already selected them (only one app can select
       ButtonPress at a time: BadAccess results.) */
    if (! (xgwa.all_event_masks & (ButtonPressMask | ButtonReleaseMask)))
      XSelectInput(dpy, windows[0], (xgwa.your_event_mask |
                       ButtonPressMask | ButtonReleaseMask));
  } else if (root_p) {
    fprintf(stderr, "%s: Running on root window\n", progname);
    windows[0] = VirtualRootWindowOfScreen(XtScreen(toplevel));
    XtDestroyWidget(toplevel);
    XGetWindowAttributes(dpy, windows[0], &xgwa);
    /* With RANDR, the root window can resize! */
    XSelectInput(dpy, windows[0], xgwa.your_event_mask | StructureNotifyMask);
    visual_warning(xgwa.screen, windows[0], xgwa.visual, xgwa.colormap, False);
  } else {
    fprintf(stderr, "%s: Running in default windowed mode\n", progname);
    if (get_boolean_resource(dpy, "pair", "Boolean")) window_count = 2;
    windows = (Window *)realloc(windows, window_count * sizeof(Window));
    toplevels = (Widget *)realloc(toplevels, window_count * sizeof(Widget));
    toplevels[0] = make_shell(XtScreen(toplevel), toplevel,
            toplevel->core.width, toplevel->core.height);
    printf("%s: new=%p toplevel=%p\n", __func__, (void *)toplevels[0], (void *)toplevel);
	if (toplevels[0] != toplevel)
      XtDestroyWidget(toplevel);
    init_window(dpy, toplevels[0], version);
    windows[0] = XtWindow(toplevels[0]);
    XGetWindowAttributes(dpy, windows[0], &xgwa);
	for (int i = 1; i < window_count; i++) {
      toplevels[i] = make_shell(xgwa.screen, 0, toplevels[0]->core.width,
                                toplevels[0]->core.height);
      init_window(dpy, toplevels[i], version);
      windows[i] = XtWindow(toplevels[i]);
    }
  }

  if (!dont_clear) {
    unsigned int bg = get_pixel_resource (dpy, xgwa.colormap,
                                            "background", "Background");
	for (int i = 0; i < window_count; i++) {
      XSetWindowBackground (dpy, windows[i], bg);
      XClearWindow (dpy, windows[i]);
	}
  }

  if (!root_p && !on_window)
    /* wait for it to be mapped */ // TODO - only need to check the first window?
    XIfEvent (dpy, &event, MapNotify_event_p, (XPointer)windows[0]);

  XSync (dpy, False);

  /* This is the one and only place that the random-number generator is
     seeded in any screenhack.  You do not need to seed the RNG again,
     it is done for you before your code is invoked. */
# undef ya_rand_init
  ya_rand_init (0);

#ifdef HAVE_RECORD_ANIM
  {
    char *str = get_string_resource (dpy, "recordAnim", "Time");
    int fps = 30;
    int h = 0, m = 0, s = 0;
    int frames = 0;
    char c, suf[20];
    *suf = 0;

    if (!str || !*str)
      ;
    else if (3 == sscanf (str, " %d:%d:%d %c", &h, &m, &s, &c))  /* H:MM:SS */
      frames = fps * (h*60*60 + m*60 + s);
    else if (2 == sscanf (str,    " %d:%d %c",     &m, &s, &c))  /*    M:SS */
      frames = fps * (m*60 + s);
    else if (1 == sscanf (str,       " %d %c",         &s, &c))  /* frames  */
      frames = s;
    else if (2 == sscanf (str, "%d %10s %c", &h, suf, &c))
      {
        if (!strcasecmp (suf, "h") ||				 /* 1 H     */
            !strcasecmp (suf, "hour") ||
            !strcasecmp (suf, "hours"))
          frames = fps * h*60*60;
        else if (!strcasecmp (suf, "m") ||			 /* 2 min   */
                 !strcasecmp (suf, "min") ||
                 !strcasecmp (suf, "mins") ||
                 !strcasecmp (suf, "minute") ||
                 !strcasecmp (suf, "minutes"))
          frames = fps * h*60;
        else if (!strcasecmp (suf, "s") ||			 /* 30 sec  */
                 !strcasecmp (suf, "sec") ||
                 !strcasecmp (suf, "secs") ||
                 !strcasecmp (suf, "second") ||
                 !strcasecmp (suf, "seconds"))
          frames = fps * h;
        else
          goto FAIL;
      }
    else
      {
      FAIL:
        fprintf (stderr, "%s: unparsable duration: %s\n", progname, str);
        exit (1);
      }

    if (str) free (str);
    if (frames > 0)
      anim_state = screenhack_record_anim_init(xgwa.screen, windows[0], frames);
  }
#endif

  run_screenhack_table (dpy, windows, window_count,
# ifdef HAVE_RECORD_ANIM
                        anim_state,
# endif
                        ft);

  // Cleanup
#ifdef HAVE_RECORD_ANIM
  if (anim_state) screenhack_record_anim_free(anim_state);
#endif
  for (int i = 0; i < window_count; i++)
    if (toplevels && toplevels[i]) XtDestroyWidget(toplevels[i]);
  if (windows) free(windows);
  if (toplevels) free(toplevels);
  XtDestroyApplicationContext (app);
#endif // else USE_SDL

  return 0;
}
