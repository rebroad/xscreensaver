/* xlockmore_web.h - Minimal xlockmore.h for Web Builds
 *
 * This provides the minimal definitions needed by xscreensaver hacks
 * when building for the web, avoiding the need for the full X11 headers.
 */

#ifndef XLOCKMORE_WEB_H
#define XLOCKMORE_WEB_H

#include "jwxyz.h"
#include <math.h>

// Essential macros and definitions - only if not already defined
#ifndef ENTRYPOINT
#define ENTRYPOINT
#endif

#ifndef XSCREENSAVER_MODULE
#define XSCREENSAVER_MODULE(name, func)
#endif

// ModeInfo structure for web builds - define this first so it's available everywhere
typedef struct {
    void *display;
    void *window;       // Changed to pointer to match jwxyz expectations
    void *visual;
    unsigned long colormap;
    int screen;
    int screen_number;  // Added for MI_SCREEN macro
    int batchcount;     // Added for MI_COUNT macro
    int wireframe_p;    // Added for MI_IS_WIREFRAME macro
    int polygon_count;  // Added for polygon counting
    int fps_p;          // Added for FPS display

    // Additional fields needed by real XScreenSaver headers
    void *dpy;          // Display pointer (alias for display)
    struct {
        int width;
        int height;
        void *visual;
    } xgwa;             // XGetWindowAttributes structure

    // Add other fields as needed
} ModeInfo;

// Common xscreensaver types and macros
// ModeInfo is now defined above

// Common function declarations - these are already declared in real headers
// No need to redeclare them here

// Trackball function declarations for web builds
#ifndef GLTRACKBALL_DECLARED
#define GLTRACKBALL_DECLARED
// Trackball functions are defined in xscreensaver_web.c
extern void* gltrackball_init(int ignore_device_rotation_p);
extern void gltrackball_free(void* tb);
extern void gltrackball_reset(void* tb, GLfloat x, GLfloat y);
extern void gltrackball_rotate(void* tb);
extern Bool gltrackball_event_handler(XEvent* event, void* tb,
                                     int window_width, int window_height,
                                     Bool* button_down_p);
#endif

// Additional function declarations needed for web builds
#ifndef WEB_FUNCTIONS_DECLARED
#define WEB_FUNCTIONS_DECLARED
extern void MI_INIT(ModeInfo *mi, void *bps);
extern GLXContext *init_GL(ModeInfo *mi);
extern void do_fps(ModeInfo *mi);
extern void render_text_simple(int x, int y, const char* text);
#endif

// Common utility functions - use web-specific names to avoid conflicts
// web_frand removed - not implemented in wrapper

// Type definitions needed for web builds
typedef struct {
    void *var;
    char *name;
    char *desc;
    char *def;
    int type;
} argtype;

typedef struct {
    int count;
    void *opts;  // XrmOptionDescRec* - simplified for web
    int vars_count;
    argtype *vars;
    char *desc;
} ModeSpecOpt;

// Type constants
#define t_Bool 1
#define t_Float 2
#define t_Int 3
#define t_String 4

// Utility macros - only if not already defined
#ifndef countof
#define countof(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif

// Missing ModeInfo macros - only if not already defined
#ifndef MI_IS_WIREFRAME
#define MI_IS_WIREFRAME(mi) 0
#endif
#ifndef MI_WINDOW
#define MI_WINDOW(mi) ((mi)->window)
#endif
#ifndef MI_DISPLAY
#define MI_DISPLAY(mi) ((mi)->display)
#endif
#ifndef MI_VISUAL
#define MI_VISUAL(mi) ((mi)->visual)
#endif
#ifndef MI_COLORMAP
#define MI_COLORMAP(mi) ((mi)->colormap)
#endif
#ifndef MI_SCREEN
#define MI_SCREEN(mi) ((mi)->screen_number)
#endif
#ifndef MI_COUNT
#define MI_COUNT(mi) ((mi)->batchcount)
#endif
#ifndef MI_WIDTH
#define MI_WIDTH(mi) ((mi)->xgwa.width)
#endif
#ifndef MI_HEIGHT
#define MI_HEIGHT(mi) ((mi)->xgwa.height)
#endif

// Common constants - only if not already defined
#ifndef XK_Up
#define XK_Up        0xFF52
#endif
#ifndef XK_Down
#define XK_Down      0xFF54
#endif
#ifndef XK_Left
#define XK_Left      0xFF51
#endif
#ifndef XK_Right
#define XK_Right     0xFF53
#endif
#ifndef XK_Prior
#define XK_Prior     0xFF55
#endif
#ifndef XK_Next
#define XK_Next      0xFF56
#endif

#endif /* XLOCKMORE_WEB_H */
