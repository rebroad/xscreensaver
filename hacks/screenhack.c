/* xscreensaver, Copyright © 1992-2022 Jamie Zawinski <jwz@jwz.org>
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

#define DEBUG_PAIR

#ifdef USE_SDL
#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#else
#include <X11/Intrinsic.h>
#include <X11/IntrinsicP.h>
#include <X11/CoreP.h>
#include <X11/Shell.h>
#include <X11/StringDefs.h>
#include <X11/keysym.h>
#endif /* USE_SDL */

#include <stdio.h>

#ifdef __sgi
# include <X11/SGIScheme.h>	/* for SgiUseSchemes() */
#endif /* __sgi */

#include "screenhackI.h"
#include "xmu.h"
#include "version.h"
#include "vroot.h"
#include "fps.h"

#ifdef HAVE_RECORD_ANIM
# include "recanim.h"
#endif

#ifndef _XSCREENSAVER_VROOT_H_
# error Error!  You have an old version of vroot.h!  Check -I args.
#endif /* _XSCREENSAVER_VROOT_H_ */

#ifndef isupper
# define isupper(c)  ((c) >= 'A' && (c) <= 'Z')
#endif
#ifndef _tolower
# define _tolower(c)  ((c) - 'A' + 'a')
#endif


/* This is defined by the SCREENHACK_MAIN() macro via screenhack.h.
 */
extern struct xscreensaver_function_table *xscreensaver_function_table;

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
  { "-noinstall",".installColormap",	XrmoptionNoArg, "False" },
  { "-visual",	".visualID",		XrmoptionSepArg, 0 },
  { "-window-id", ".windowID",		XrmoptionSepArg, 0 },
  { "-fps",	".doFPS",		XrmoptionNoArg, "True" },
  { "-no-fps",  ".doFPS",		XrmoptionNoArg, "False" },

# ifdef DEBUG_PAIR
  { "-pair",	".pair",		XrmoptionNoArg, "True" },
# endif
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
  const char * const *defaults    = ft->defaults;
  const char *progclass           = ft->progclass;

  int def_opts_size, opts_size;
  int def_defaults_size, defaults_size;

  for (def_opts_size = 0; default_options[def_opts_size].option;
       def_opts_size++)
    ;
  for (opts_size = 0; options[opts_size].option; opts_size++)
    ;

  merged_options_size = def_opts_size + opts_size;
  merged_options = (XrmOptionDescRec *)
    malloc ((merged_options_size + 1) * sizeof(*default_options));
  memcpy (merged_options, default_options,
	  (def_opts_size * sizeof(*default_options)));
  memcpy (merged_options + def_opts_size, options,
	  ((opts_size + 1) * sizeof(*default_options)));

  for (def_defaults_size = 0; default_defaults[def_defaults_size];
       def_defaults_size++)
    ;
  for (defaults_size = 0; defaults[defaults_size]; defaults_size++)
    ;
  merged_defaults = (char **)
    malloc ((def_defaults_size + defaults_size + 1) * sizeof (*defaults));;
  memcpy (merged_defaults, default_defaults,
	  def_defaults_size * sizeof(*defaults));
  memcpy (merged_defaults + def_defaults_size, defaults,
	  (defaults_size + 1) * sizeof(*defaults));

  /* This totally sucks.  Xt should behave like this by default.
     If the string in `defaults' looks like ".foo", change that
     to "Progclass.foo".
   */
  {
    char **s;
    for (s = merged_defaults; *s; s++)
      if (**s == '.')
	{
	  const char *oldr = *s;
	  char *newr = (char *) malloc(strlen(oldr) + strlen(progclass) + 3);
	  strcpy (newr, progclass);
	  strcat (newr, oldr);
	  *s = newr;
	}
      else
        *s = strdup (*s);
  }
}


/* Make the X errors print out the name of this program, so we have some
   clue which one has a bug when they die under the screensaver.
 */

#ifndef USE_SDL
static int
screenhack_ehandler (Display *dpy, XErrorEvent *error)
{
  fprintf (stderr, "\nX error in %s:\n", progname);
  if (XmuPrintDefaultErrorMessage (dpy, error, stderr))
    exit (-1);
  else
    fprintf (stderr, " (nonfatal.)\n");
  return 0;
}

static Bool
MapNotify_event_p (Display *dpy, XEvent *event, XPointer window)
{
  return (event->xany.type == MapNotify &&
	  event->xvisibility.window == (Window) window);
}

static Atom XA_WM_PROTOCOLS, XA_WM_DELETE_WINDOW, XA_NET_WM_PID;

/* Dead-trivial event handling: exits if "q" or "ESC" are typed.
   Exit if the WM_PROTOCOLS WM_DELETE_WINDOW ClientMessage is received.
   Returns False if the screen saver should now terminate.
 */
static Bool
screenhack_handle_event_1 (Display *dpy, XEvent *event)
{
  switch (event->xany.type)
    {
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
        else if (event->xclient.data.l[0] != XA_WM_DELETE_WINDOW)
          {
            char *s1 = XGetAtomName(dpy, event->xclient.message_type);
            char *s2 = XGetAtomName(dpy, event->xclient.data.l[0]);
            if (!s1) s1 = "(null)";
            if (!s2) s2 = "(null)";
            fprintf (stderr, "%s: unknown ClientMessage %s[%s] received!\n",
                     progname, s1, s2);
          }
        else
          {
            return False;  /* exit */
          }
      }
      break;
    }
  return True;
}


static Visual * pick_visual (Screen *screen) {
  struct xscreensaver_function_table *ft = xscreensaver_function_table;

  if (ft->pick_visual_hook)
    {
      Visual *v = ft->pick_visual_hook (screen);
      if (v) return v;
    }

  return get_visual_resource (screen, "visualID", "VisualID", False);
}


/* Notice when the user has requested a different visual or colormap
   on a pre-existing window (e.g., "--root --visual truecolor" or
   "--window-id 0x2c00001 --install") and complain, since when drawing
   on an existing window, we have no choice about these things.
 */
static void
visual_warning (Screen *screen, Window window, Visual *visual, Colormap cmap,
                Bool window_p)
{
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

  if (visual_string && *visual_string)
    {
      char *s;
      for (s = visual_string; *s; s++)
        if (isupper (*s)) *s = _tolower (*s);

      if (!strcmp (visual_string, "default") ||
          !strcmp (visual_string, "default") ||
          !strcmp (visual_string, "best"))
        /* don't warn about these, just silently DWIM. */
        ;
      else if (visual != desired_visual)
        {
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

  if (ft->validate_visual_hook)
    {
      if (! ft->validate_visual_hook (screen, win, visual))
        exit (1);
    }
}
#endif


static void
fix_fds (void)
{
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
     if you want it to be robust in the face of "2>&-".
   */
  int fd0 = open ("/dev/null", O_RDWR);
  int fd1 = open ("/dev/null", O_RDWR);
  int fd2 = open ("/dev/null", O_RDWR);
  if (fd0 > 2) close (fd0);
  if (fd1 > 2) close (fd1);
  if (fd2 > 2) close (fd2);
}

#ifndef USE_SDL
static Boolean
screenhack_table_handle_events (Display *dpy,
                                const struct xscreensaver_function_table *ft,
                                Window window, void *closure
#ifdef DEBUG_PAIR
                                , Window window2, void *closure2
#endif
                                )
{
  XtAppContext app = XtDisplayToApplicationContext (dpy);
  XtInputMask m = XtAppPending (app);

  /* Process non-X11 Xt events (timers, files, signals) without blocking. */
  if (m & ~XtIMXEvent)
    XtAppProcessEvent (app, ~XtIMXEvent);

  /* Process all pending X11 events without blocking. */
  while (XPending (dpy))
    {
      XEvent event;
      XNextEvent (dpy, &event);

      if (event.xany.type == ConfigureNotify)
        {
          if (event.xany.window == window)
            ft->reshape_cb (dpy, window, closure,
                            event.xconfigure.width, event.xconfigure.height);
#ifdef DEBUG_PAIR
          if (window2 && event.xany.window == window2)
            ft->reshape_cb (dpy, window2, closure2,
                            event.xconfigure.width, event.xconfigure.height);
#endif
        }
      else if (event.xany.type == ClientMessage ||
               (! (event.xany.window == window
                   ? ft->event_cb (dpy, window, closure, &event)
#ifdef DEBUG_PAIR
                   : (window2 && event.xany.window == window2)
                   ? ft->event_cb (dpy, window2, closure2, &event)
#endif
                   : 0)))
        if (! screenhack_handle_event_1 (dpy, &event))
          return False;

      /* Last chance to process Xt timers before blocking. */
      m = XtAppPending (app);
      if (m & ~XtIMXEvent)
        XtAppProcessEvent (app, ~XtIMXEvent);
    }

# ifdef EXIT_AFTER
  if (exit_after != 0 && time ((time_t *) 0) >= exit_after)
    return False;
# endif

  return True;
}


static Boolean
usleep_and_process_events (Display *dpy,
                           const struct xscreensaver_function_table *ft,
                           Window window, fps_state *fpst, void *closure,
                           unsigned long delay
#ifdef DEBUG_PAIR
                         , Window window2, fps_state *fpst2, void *closure2,
                           unsigned long delay2
#endif
# ifdef HAVE_RECORD_ANIM
                         , record_anim_state *anim_state
# endif
                           )
{
  do {
    unsigned long quantum = 33333;  /* 30 fps */
    if (quantum > delay) 
      quantum = delay;
    delay -= quantum;

    XSync (dpy, False);

#ifdef HAVE_RECORD_ANIM
    if (anim_state) screenhack_record_anim (anim_state);
#endif

    if (quantum > 0)
      {
        usleep (quantum);
        if (fpst) fps_slept (fpst, quantum);
#ifdef DEBUG_PAIR
        if (fpst2) fps_slept (fpst2, quantum);
#endif
      }

    if (! screenhack_table_handle_events (dpy, ft, window, closure
#ifdef DEBUG_PAIR
                                          , window2, closure2
#endif
                                          ))
      return False;
  } while (delay > 0);

  return True;
}

static void
screenhack_do_fps (Display *dpy, Window w, fps_state *fpst, void *closure)
{
  fps_compute (fpst, 0, -1);
  fps_draw (fpst);
}
#endif


#ifdef USE_SDL
static void run_screenhack_table_sdl(SDL_Window *window, SDL_GLContext gl_context,
                                    const struct xscreensaver_function_table *ft) {
  struct sdl_state {
	SDL_Window *window;
	SDL_GLContext gl_context;
  } state = {window, gl_context};

  void *(*init_cb)(Display *, Window, void *) =
    (void *(*)(Display *, Window, void *)) ft->init_cb;

  void *closure = init_cb(NULL, (Window)SDL_GetWindowID(window), &state);
  fps_state *fpst = fps_init(NULL, (Window)SDL_GetWindowID(window));
  unsigned long delay = 0;

  SDL_Event event;
  Bool running = True;

  while (running) {
    while (SDL_PollEvent(&event)) {
      if (!ft->event_cb(NULL, (Window)SDL_GetWindowID(window), closure, &event))
        running = False;
    }

    if (!running) break;

    delay = ft->draw_cb(NULL, (Window)SDL_GetWindowID(window), closure);

    if (fpst) {
      ft->fps_cb(NULL, (Window)SDL_GetWindowID(window), fpst, closure);
    }

    SDL_GL_SwapWindow(window);
    SDL_Delay(delay/1000); // Convert microseconds to milliseconds
  }

  if (fpst) ft->fps_free(fpst);
  ft->free_cb(NULL, (Window)SDL_GetWindowID(window), closure);
}
#else
static void
run_screenhack_table (Display *dpy, 
                      Window window,
# ifdef DEBUG_PAIR
                      Window window2,
# endif
# ifdef HAVE_RECORD_ANIM
                      record_anim_state *anim_state,
# endif
                      const struct xscreensaver_function_table *ft)
{

  /* Kludge: even though the init_cb functions are declared to take 2 args,
     actually call them with 3, for the benefit of xlockmore_init() and
     xlockmore_setup().
   */
  void *(*init_cb) (Display *, Window, void *) = 
    (void *(*) (Display *, Window, void *)) ft->init_cb;

  void (*fps_cb) (Display *, Window, fps_state *, void *) = ft->fps_cb;

  void *closure = init_cb (dpy, window, ft->setup_arg);
  fps_state *fpst = fps_init (dpy, window);
  unsigned long delay = 0;

#ifdef DEBUG_PAIR
  void *closure2 = 0;
  fps_state *fpst2 = 0;
  unsigned long delay2 = 0;
  if (window2) closure2 = init_cb (dpy, window2, ft->setup_arg);
  if (window2) fpst2 = fps_init (dpy, window2);
#endif

  if (! closure)  /* if it returns nothing, it can't possibly be re-entrant. */
    abort();

  if (! fps_cb) fps_cb = screenhack_do_fps;

  while (1)
    {
      if (! usleep_and_process_events (dpy, ft,
                                       window, fpst, closure, delay
#ifdef DEBUG_PAIR
                                       , window2, fpst2, closure2, delay2
#endif
#ifdef HAVE_RECORD_ANIM
                                       , anim_state
#endif
                                       ))
        break;

      delay = ft->draw_cb (dpy, window, closure);
#ifdef DEBUG_PAIR
      delay2 = 0;
      if (window2) delay2 = ft->draw_cb (dpy, window2, closure2);
#endif

      if (fpst) fps_cb (dpy, window, fpst, closure);
#ifdef DEBUG_PAIR
      if (fpst2) fps_cb (dpy, window2, fpst2, closure2);
#endif
    }

#ifdef HAVE_RECORD_ANIM
  /* Exiting before target frames hit: write the video anyway. */
  if (anim_state) screenhack_record_anim_free (anim_state);
#endif

  if (fpst) ft->fps_free (fpst);
#ifdef DEBUG_PAIR
  if (fpst2) ft->fps_free (fpst2);
#endif

  ft->free_cb (dpy, window, closure);
#ifdef DEBUG_PAIR
  if (window2) ft->free_cb (dpy, window2, closure2);
#endif
}
#endif

#ifndef USE_SDL
static Widget make_shell (Screen *screen, Widget toplevel, int width, int height) {
  Display *dpy = DisplayOfScreen (screen);
  Visual *visual = pick_visual (screen);
  Boolean def_visual_p = (toplevel && 
                          visual == DefaultVisualOfScreen (screen));

  if (width  <= 0) width  = 600;
  if (height <= 0) height = 480;

  if (def_visual_p)
    {
      Window window;
      XtVaSetValues (toplevel,
                     XtNmappedWhenManaged, False,
                     XtNwidth, width,
                     XtNheight, height,
                     XtNinput, True,  /* for WM_HINTS */
                     NULL);
      XtRealizeWidget (toplevel);
      window = XtWindow (toplevel);

      if (get_boolean_resource (dpy, "installColormap", "InstallColormap"))
        {
          Colormap cmap = 
            XCreateColormap (dpy, window, DefaultVisualOfScreen (screen),
                             AllocNone);
          XSetWindowColormap (dpy, window, cmap);
        }
    }
  else
    {
      unsigned int bg, bd;
      Widget new;
      Colormap cmap = XCreateColormap (dpy, VirtualRootWindowOfScreen(screen),
                                       visual, AllocNone);
      bg = get_pixel_resource (dpy, cmap, "background", "Background");
      bd = get_pixel_resource (dpy, cmap, "borderColor", "Foreground");

      new = XtVaAppCreateShell (progname, progclass,
                                topLevelShellWidgetClass, dpy,
                                XtNmappedWhenManaged, False,
                                XtNvisual, visual,
                                XtNdepth, visual_depth (screen, visual),
                                XtNwidth, width,
                                XtNheight, height,
                                XtNcolormap, cmap,
                                XtNbackground, (Pixel) bg,
                                XtNborderColor, (Pixel) bd,
                                XtNinput, True,  /* for WM_HINTS */
                                NULL);

      if (!toplevel)  /* kludge for the second window in -pair mode... */
        XtVaSetValues (new, XtNx, 0, XtNy, 550, NULL);

      XtRealizeWidget (new);
      toplevel = new;
    }

  return toplevel;
}
#endif

#ifdef USE_SDL
static void init_window (SDL_Window *window, const char *title) {
  SDL_SetWindowTitle(window, title);
#else
static void init_window (Display *dpy, Widget toplevel, const char *title) {
  long pid = getpid();
  Window window;
  XWindowAttributes xgwa;
  XtPopup (toplevel, XtGrabNone);
  XtVaSetValues (toplevel, XtNtitle, title, NULL);

  /* Select KeyPress, and announce that we accept WM_DELETE_WINDOW.
   */
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
#endif
}

#ifdef USE_SDL
SDL_Window *window = NULL;
SDL_GLContext glContext = NULL;
#endif

#ifdef _WIN32
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
				LPSTR lpCmdLine, int nCmdShow) {
  return SDL_main(__argc, __argv);
}
#endif

int main (int argc, char **argv) {
  printf("%s: %s\n", __FILE__, __func__);
  struct xscreensaver_function_table *ft = xscreensaver_function_table;

  fix_fds();

  progname = argv[0];   /* reset later */
  progclass = ft->progclass;

  if (ft->setup_cb)
    ft->setup_cb (ft, ft->setup_arg);

  merge_options ();

  /* Xt and xscreensaver predate the "--arg" convention, so convert
     double dashes to single. */
  {
    int i;
    for (i = 1; i < argc; i++)
      if (argv[i][0] == '-' && argv[i][1] == '-')
        argv[i]++;
  }

#ifndef USE_SDL
  Display *dpy;
  Widget toplevel;
  XtAppContext app;
  toplevel = XtAppInitialize (&app, progclass, merged_options,
			      merged_options_size, &argc, argv,
			      merged_defaults, 0, 0);
  dpy = XtDisplay (toplevel);
#endif
  char version[255];
  {
    char *v = (char *) strdup(strchr(screensaver_id, ' '));
    char *s1, *s2, *s3, *s4;
#ifdef USE_SDL
	const char *ot = NULL;
#else
    const char *ot = get_string_resource (dpy, "title", "Title");
#endif
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
    int i;
    int x = 18;
    int end = 78;
    Bool help_p = (!strcmp(argv[1], "-help") || !strcmp(argv[1], "--help"));
    fprintf (stderr, "%s\n", version);
    fprintf (stderr, "\n\thttps://www.jwz.org/xscreensaver/\n\n");

    if (!help_p)
      fprintf(stderr, "Unrecognised option: %s\n", argv[1]);
    fprintf (stderr, "Options include: ");
    for (i = 0; i < merged_options_size; i++) {
	  char *sw = merged_options [i].option;
	  Bool argp = (merged_options [i].argKind == XrmoptionSepArg);
	  int size = strlen (sw) + (argp ? 6 : 0) + 2;
	  if (x + size >= end) {
	    fprintf (stderr, "\n\t\t ");
	    x = 18;
	  }
	  x += size;
	  fprintf (stderr, "-%s", sw);  /* two dashes */
	  if (argp) fprintf (stderr, " <arg>");
	  if (i != merged_options_size - 1) fprintf (stderr, ", ");
    }
    fprintf (stderr, ".\n");

    exit (help_p ? 0 : 1);
  }

#ifdef USE_SDL
  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
	fprintf(stderr, "SDL initialization failed: %s\n", SDL_GetError());
	return 1;
  }

  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

  SDL_Window *window = SDL_CreateWindow(
    ft->progclass, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
	1280, 720, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);

  if (!window) {
	fprintf(stderr, "Window creation failed: %s\n", SDL_GetError());
	SDL_Quit();
	return 1;
  }

  SDL_GLContext gl_context = SDL_GL_CreateContext(window);
  if (!gl_context) {
	fprintf(stderr, "OpenGL context creation failed: %s\n", SDL_GetError());
	SDL_DestroyWindow(window);
	SDL_Quit();
	return 1;
  }

  run_screenhack_table_sdl(window, gl_context, ft);

  SDL_GL_DeleteContext(gl_context);
  SDL_DestroyWindow(window);
  SDL_Quit();
#else /* ! USE_SDL */
  XWindowAttributes xgwa;
  Window window;
# ifdef DEBUG_PAIR
  Window window2 = 0;
  Widget toplevel2 = 0;
# endif
# ifdef HAVE_RECORD_ANIM
  record_anim_state *anim_state = 0;
# endif
  Bool root_p;
  Window on_window = 0;
  XEvent event;
  Boolean dont_clear;

#ifdef __sgi
  /* We have to do this on SGI to prevent the background color from being
     overridden by the current desktop color scheme (we'd like our backgrounds
     to be black, thanks.)  This should be the same as setting the
     "*useSchemes: none" resource, but it's not -- if that resource is
     present in the `default_defaults' above, it doesn't work, though it
     does work when passed as an -xrm arg on the command line.  So screw it,
     turn them off from C instead.
   */
  SgiUseSchemes ("none");
#endif /* __sgi */

  XtGetApplicationNameAndClass (dpy,
                                (char **) &progname,
                                (char **) &progclass);

  /* half-assed way of avoiding buffer-overrun attacks. */
  if (strlen (progname) >= 100) ((char *) progname)[100] = 0;

  XSetErrorHandler (screenhack_ehandler);

  XA_WM_PROTOCOLS = XInternAtom (dpy, "WM_PROTOCOLS", False);
  XA_WM_DELETE_WINDOW = XInternAtom (dpy, "WM_DELETE_WINDOW", False);
  XA_NET_WM_PID = XInternAtom (dpy, "_NET_WM_PID", False);

  {
    char **s;
    for (s = merged_defaults; *s; s++)
      free(*s);
  }

  free (merged_options);
  free (merged_defaults);
  merged_options = 0;
  merged_defaults = 0;

  dont_clear = get_boolean_resource (dpy, "dontClearRoot", "Boolean");
  mono_p = get_boolean_resource (dpy, "mono", "Boolean");
  if (CellsOfScreen (DefaultScreenOfDisplay (dpy)) <= 2)
    mono_p = True;

  root_p = get_boolean_resource (dpy, "root", "Boolean");

# ifdef EXIT_AFTER
  {
    int secs = get_integer_resource (dpy, "exitAfter", "Integer");
    exit_after = (secs > 0
                  ? time((time_t *) 0) + secs
                  : 0);
  }      
# endif

  {
    char *s = get_string_resource (dpy, "windowID", "WindowID");
    if (s && *s)
      on_window = get_integer_resource (dpy, "windowID", "WindowID");
    if (s) free (s);
  }

  if (on_window)
    {
      window = (Window) on_window;
      XtDestroyWidget (toplevel);
      XGetWindowAttributes (dpy, window, &xgwa);
      visual_warning (xgwa.screen, window, xgwa.visual, xgwa.colormap, True);

      /* Select KeyPress and resize events on the external window.
       */
      xgwa.your_event_mask |= KeyPressMask | StructureNotifyMask;
      XSelectInput (dpy, window, xgwa.your_event_mask);

      /* Select ButtonPress and ButtonRelease events on the external window,
         if no other app has already selected them (only one app can select
         ButtonPress at a time: BadAccess results.)
       */
      if (! (xgwa.all_event_masks & (ButtonPressMask | ButtonReleaseMask)))
        XSelectInput (dpy, window,
                      (xgwa.your_event_mask |
                       ButtonPressMask | ButtonReleaseMask));
    }
  else if (root_p)
    {
      window = VirtualRootWindowOfScreen (XtScreen (toplevel));
      XtDestroyWidget (toplevel);
      XGetWindowAttributes (dpy, window, &xgwa);
      /* With RANDR, the root window can resize! */
      XSelectInput (dpy, window, xgwa.your_event_mask | StructureNotifyMask);
      visual_warning (xgwa.screen, window, xgwa.visual, xgwa.colormap, False);
    }
  else
    {
      Widget new = make_shell (XtScreen (toplevel), toplevel, // TODO - SDLify
                               toplevel->core.width,
                               toplevel->core.height);
      if (new != toplevel)
        {
          XtDestroyWidget (toplevel);
          toplevel = new;
        }

      init_window (dpy, toplevel, version);
      window = XtWindow (toplevel);
      XGetWindowAttributes (dpy, window, &xgwa);

# ifdef DEBUG_PAIR
      if (get_boolean_resource (dpy, "pair", "Boolean"))
        {
          toplevel2 = make_shell (xgwa.screen, 0,
                                  toplevel->core.width,
                                  toplevel->core.height);
          init_window (dpy, toplevel2, version);
          window2 = XtWindow (toplevel2);
        }
# endif /* DEBUG_PAIR */
    }

  if (!dont_clear)
    {
      unsigned int bg = get_pixel_resource (dpy, xgwa.colormap,
                                            "background", "Background");
      XSetWindowBackground (dpy, window, bg);
      XClearWindow (dpy, window);
# ifdef DEBUG_PAIR
      if (window2)
        {
          XSetWindowBackground (dpy, window2, bg);
          XClearWindow (dpy, window2);
        }
# endif
    }

  if (!root_p && !on_window)
    /* wait for it to be mapped */
    XIfEvent (dpy, &event, MapNotify_event_p, (XPointer) window);

  XSync (dpy, False);

  /* This is the one and only place that the random-number generator is
     seeded in any screenhack.  You do not need to seed the RNG again,
     it is done for you before your code is invoked. */
# undef ya_rand_init
  ya_rand_init (0);


#ifdef HAVE_RECORD_ANIM
  {
    int frames = get_integer_resource (dpy, "recordAnim", "Integer");
    if (frames > 0)
      anim_state = screenhack_record_anim_init (xgwa.screen, window, frames);
  }
#endif

  run_screenhack_table (dpy, window, 
# ifdef DEBUG_PAIR
                        window2,
# endif
# ifdef HAVE_RECORD_ANIM
                        anim_state,
# endif
                        ft);

#ifdef HAVE_RECORD_ANIM
  if (anim_state) screenhack_record_anim_free (anim_state);
#endif

  XtDestroyWidget (toplevel);
  XtDestroyApplicationContext (app);
#endif /* USE_SDL */

  return 0;
}
