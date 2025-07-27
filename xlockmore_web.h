/* xlockmore_web.h - Minimal xlockmore.h for Web Builds
 *
 * This provides the minimal definitions needed by xscreensaver hacks
 * when building for the web, avoiding the need for the full X11 headers.
 */

#ifndef XLOCKMORE_WEB_H
#define XLOCKMORE_WEB_H

#include "jwxyz.h"
#include <math.h>

// GLdouble is missing in WebGL, provide it
#ifndef GLdouble
typedef double GLdouble;
#endif

// Essential macros and definitions
#define ENTRYPOINT
#define XSCREENSAVER_MODULE(name, func)

// Generic UI control system types
typedef enum {
    WEB_PARAM_FLOAT,
    WEB_PARAM_INT,
    WEB_PARAM_BOOL
} web_param_type;

typedef struct {
    const char* name;
    const char* display_name;
    web_param_type type;
    void* variable;
    void (*update_callback)(void*);
    double min_value;
    double max_value;
    double step_value;
} web_parameter;

// Common xscreensaver types and macros
// ModeInfo is now defined above

// Common function declarations - these are already declared in real headers
// No need to redeclare them here

// Trackball function declarations for web builds
#ifndef GLTRACKBALL_DECLARED
#define GLTRACKBALL_DECLARED
// Trackball functions are now static internal functions in xscreensaver_web.c
// No need to declare them here
#endif

// Additional function declarations needed for web builds
#ifndef WEB_FUNCTIONS_DECLARED
#define WEB_FUNCTIONS_DECLARED
extern void MI_INIT(ModeInfo *mi, void *bps);
extern GLXContext *init_GL(ModeInfo *mi);
extern void do_fps(ModeInfo *mi);
extern void render_text_simple(int x, int y, const char* text);
extern void web_fps_set_target(double target_fps);

// Generic UI control system
extern void register_web_parameter(const char* name, const char* display_name, web_param_type type, 
                                  void* variable, void (*update_callback)(void*),
                                  double min_value, double max_value, double step_value);
extern void web_set_parameter(const char* name, double value);
extern int web_get_parameter_count();
extern const char* web_get_parameter_name(int index);
extern const char* web_get_parameter_display_name(int index);
extern int web_get_parameter_type(int index);
extern double web_get_parameter_value(int index);
extern double web_get_parameter_min(int index);
extern double web_get_parameter_max(int index);
extern double web_get_parameter_step(int index);

// Immediate mode OpenGL functions
extern void glMatrixMode(GLenum mode);
extern void glLoadIdentity(void);
extern void glPushMatrix(void);
extern void glPopMatrix(void);
extern void glTranslatef(GLfloat x, GLfloat y, GLfloat z);
extern void glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z);
extern void glScalef(GLfloat x, GLfloat y, GLfloat z);
extern void glOrtho(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble near_val, GLdouble far_val);
extern void glFrustum(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble near_val, GLdouble far_val);
extern void glMultMatrixf(const GLfloat *m);
extern void glColor3f(GLfloat r, GLfloat g, GLfloat b);
extern void glColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a);
extern void glColor4fv(const GLfloat *v);
extern void glNormal3f(GLfloat nx, GLfloat ny, GLfloat nz);
extern void glBegin(GLenum mode);
extern void glVertex3f(GLfloat x, GLfloat y, GLfloat z);
extern void glVertex2i(GLint x, GLint y);
extern void glEnd(void);
extern void glClear(GLbitfield mask);
extern void glEnable(GLenum cap);
extern void glDisable(GLenum cap);
extern void glPushAttrib(GLbitfield mask);
extern void glPopAttrib(void);
extern void glRasterPos2i(GLint x, GLint y);
extern void glMaterialfv(GLenum face, GLenum pname, const GLfloat *params);
extern void glShadeModel(GLenum mode);
extern void gluPerspective(GLfloat fovy, GLfloat aspect, GLfloat zNear, GLfloat zFar);
extern void gluLookAt(GLfloat eyex, GLfloat eyey, GLfloat eyez,
                      GLfloat centerx, GLfloat centery, GLfloat centerz,
                      GLfloat upx, GLfloat upy, GLfloat upz);
extern Bool screenhack_event_helper(void *display, void *window, void *event);
extern double frand(double max);
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

// These constants should be available, but define them if missing

#endif /* XLOCKMORE_WEB_H */
