/* xscreensaver_web.c - Generic Web Wrapper for XScreenSaver Hacks
 *
 * This provides a generic web interface for any xscreensaver hack,
 * similar to how jwxyz.h provides Android compatibility.
 */

#include <emscripten.h>
#include <GLES3/gl3.h>
#include <emscripten/html5.h>
#include "jwxyz.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <emscripten/emscripten.h>
#include "screenhackI.h"
#include "xlockmore_web.h"

// Web-specific type definitions
#ifndef Bool
typedef int Bool;
#define True 1
#define False 0
#endif

// ModeInfo is now defined in xlockmore_web.h, so we don't need to redefine it here

// XEvent is already defined in jwxyz.h, no need to redefine

// Only provide what's actually missing in WebGL builds

// GLdouble is missing in WebGL, provide it
#ifndef GLdouble
typedef double GLdouble;
#endif

// These constants should be available, but define them if missing
#ifndef GL_MODELVIEW_MATRIX
#define GL_MODELVIEW_MATRIX  0x0BA6
#endif
#ifndef GL_PROJECTION_MATRIX
#define GL_PROJECTION_MATRIX 0x0BA7
#endif

// FPS tracking variables
static double last_fps_time = 0.0;
static int frame_count = 0;
static double current_fps = 0.0;
static double current_load = 0.0;
static int current_polys = 0;

// Only provide gluProject if it's not working with the expected signature
// We'll name it differently to avoid conflicts
int gluProject_web(GLdouble objx, GLdouble objy, GLdouble objz,
                   const GLdouble modelMatrix[16], const GLdouble projMatrix[16],
                   const GLint viewport[4],
                   GLdouble *winx, GLdouble *winy, GLdouble *winz) {
    GLdouble in[4], out[4];

    // Transform by model matrix
    in[0] = objx; in[1] = objy; in[2] = objz; in[3] = 1.0;

    // Model transformation
    out[0] = modelMatrix[0]*in[0] + modelMatrix[4]*in[1] + modelMatrix[8]*in[2] + modelMatrix[12]*in[3];
    out[1] = modelMatrix[1]*in[0] + modelMatrix[5]*in[1] + modelMatrix[9]*in[2] + modelMatrix[13]*in[3];
    out[2] = modelMatrix[2]*in[0] + modelMatrix[6]*in[1] + modelMatrix[10]*in[2] + modelMatrix[14]*in[3];
    out[3] = modelMatrix[3]*in[0] + modelMatrix[7]*in[1] + modelMatrix[11]*in[2] + modelMatrix[15]*in[3];

    // Projection transformation
    in[0] = out[0]; in[1] = out[1]; in[2] = out[2]; in[3] = out[3];
    out[0] = projMatrix[0]*in[0] + projMatrix[4]*in[1] + projMatrix[8]*in[2] + projMatrix[12]*in[3];
    out[1] = projMatrix[1]*in[0] + projMatrix[5]*in[1] + projMatrix[9]*in[2] + projMatrix[13]*in[3];
    out[2] = projMatrix[2]*in[0] + projMatrix[6]*in[1] + projMatrix[10]*in[2] + projMatrix[14]*in[3];
    out[3] = projMatrix[3]*in[0] + projMatrix[7]*in[1] + projMatrix[11]*in[2] + projMatrix[15]*in[3];

    if (out[3] == 0.0) return 0; // GL_FALSE

    // Perspective divide and viewport transform
    out[0] /= out[3]; out[1] /= out[3]; out[2] /= out[3];
    *winx = viewport[0] + (1.0 + out[0]) * viewport[2] / 2.0;
    *winy = viewport[1] + (1.0 + out[1]) * viewport[3] / 2.0;
    *winz = (1.0 + out[2]) / 2.0;

    return 1; // GL_TRUE
}

// Provide glGetDoublev since WebGL only has glGetFloatv
void glGetDoublev_web(GLenum pname, GLdouble *params) {
    GLfloat float_params[16];
    glGetFloatv(pname, float_params);

    // Convert float to double
    int count = (pname == GL_MODELVIEW_MATRIX || pname == GL_PROJECTION_MATRIX) ? 16 : 1;
    for (int i = 0; i < count; i++) {
        params[i] = (GLdouble)float_params[i];
    }
}

// Override the function names for hextrail.c when building for web
#define gluProject gluProject_web
#define glGetDoublev glGetDoublev_web

// Web-specific trackball implementation
typedef struct {
    float q[4];        // quaternion
    float last_x, last_y;
    int dragging;
    float init_x, init_y;
} web_trackball_state;

// Use web_trackball_state directly instead of redefining trackball_state

// Trackball functions with web implementation - using void* to avoid type conflicts
void* gltrackball_init(int ignore_device_rotation_p) {
    web_trackball_state* tb = (web_trackball_state*)calloc(1, sizeof(web_trackball_state));
    if (tb) {
        // Initialize to identity quaternion
        tb->q[0] = 0.0f; tb->q[1] = 0.0f; tb->q[2] = 0.0f; tb->q[3] = 1.0f;
        tb->dragging = 0;
    }
    return (void*)tb;
}

void gltrackball_free(void* tb) {
    if (tb) free(tb);
}

void gltrackball_reset(void* tb, GLfloat x, GLfloat y) {
    if (!tb) return;
    web_trackball_state* wtb = (web_trackball_state*)tb;
    wtb->init_x = x;
    wtb->init_y = y;
    // Set initial rotation based on x, y
    wtb->q[0] = x; wtb->q[1] = y; wtb->q[2] = 0.0f; wtb->q[3] = 1.0f;
    // Normalize quaternion
    float len = sqrt(wtb->q[0]*wtb->q[0] + wtb->q[1]*wtb->q[1] + wtb->q[2]*wtb->q[2] + wtb->q[3]*wtb->q[3]);
    if (len > 0) {
        wtb->q[0] /= len; wtb->q[1] /= len; wtb->q[2] /= len; wtb->q[3] /= len;
    }
}

void gltrackball_rotate(void* tb) {
    if (!tb) return;
    web_trackball_state* wtb = (web_trackball_state*)tb;

    // Convert quaternion to rotation matrix and apply
    float q0 = wtb->q[0], q1 = wtb->q[1], q2 = wtb->q[2], q3 = wtb->q[3];
    float matrix[16] = {
        1-2*(q1*q1 + q2*q2), 2*(q0*q1 - q2*q3),   2*(q0*q2 + q1*q3),   0,
        2*(q0*q1 + q2*q3),   1-2*(q0*q0 + q2*q2), 2*(q1*q2 - q0*q3),   0,
        2*(q0*q2 - q1*q3),   2*(q1*q2 + q0*q3),   1-2*(q0*q0 + q1*q1), 0,
        0,                   0,                   0,                   1
    };
    glMultMatrixf(matrix);
}

Bool gltrackball_event_handler(XEvent* event, void* tb,
                              int window_width, int window_height,
                              Bool* button_down_p) {
    if (!tb || !event) return False;
    web_trackball_state* wtb = (web_trackball_state*)tb;

    // For web builds, we'll handle mouse events
    // This is a simplified version - real trackball would be more complex
    static int mouse_button_down = 0;
    static float last_mouse_x = 0, last_mouse_y = 0;

    if (event->xany.type == ButtonPress) {
        mouse_button_down = 1;
        last_mouse_x = event->xbutton.x;
        last_mouse_y = event->xbutton.y;
        wtb->dragging = 1;
        *button_down_p = True;
        return True;
    } else if (event->xany.type == ButtonRelease) {
        mouse_button_down = 0;
        wtb->dragging = 0;
        *button_down_p = False;
        return True;
    } else if (event->xany.type == MotionNotify && mouse_button_down) {
        float dx = (event->xmotion.x - last_mouse_x) / (float)window_width;
        float dy = (event->xmotion.y - last_mouse_y) / (float)window_height;

        // Simple trackball rotation - convert mouse delta to quaternion rotation
        float angle = sqrt(dx*dx + dy*dy) * 2.0f;
        if (angle > 0) {
            float axis_x = -dy / angle;
            float axis_y = dx / angle;
            float axis_z = 0;

            float s = sin(angle * 0.5f);
            float c = cos(angle * 0.5f);

            // Create rotation quaternion
            float dq[4] = { axis_x * s, axis_y * s, axis_z * s, c };

            // Multiply current quaternion by rotation
            float new_q[4];
            new_q[0] = wtb->q[3]*dq[0] + wtb->q[0]*dq[3] + wtb->q[1]*dq[2] - wtb->q[2]*dq[1];
            new_q[1] = wtb->q[3]*dq[1] - wtb->q[0]*dq[2] + wtb->q[1]*dq[3] + wtb->q[2]*dq[0];
            new_q[2] = wtb->q[3]*dq[2] + wtb->q[0]*dq[1] - wtb->q[1]*dq[0] + wtb->q[2]*dq[3];
            new_q[3] = wtb->q[3]*dq[3] - wtb->q[0]*dq[0] - wtb->q[1]*dq[1] - wtb->q[2]*dq[2];

            // Normalize and store
            float len = sqrt(new_q[0]*new_q[0] + new_q[1]*new_q[1] + new_q[2]*new_q[2] + new_q[3]*new_q[3]);
            if (len > 0) {
                wtb->q[0] = new_q[0] / len;
                wtb->q[1] = new_q[1] / len;
                wtb->q[2] = new_q[2] / len;
                wtb->q[3] = new_q[3] / len;
            }
        }

        last_mouse_x = event->xmotion.x;
        last_mouse_y = event->xmotion.y;
        return True;
    }

    return False;
}

// Web wrapper initialization function
EMSCRIPTEN_KEEPALIVE
int xscreensaver_web_init(
    void (*init_func)(ModeInfo *mi),
    void (*draw_func)(ModeInfo *mi),
    void (*reshape_func)(ModeInfo *mi, int width, int height),
    void (*free_func)(ModeInfo *mi),
    Bool (*handle_event_func)(ModeInfo *mi, XEvent *event)
) {
    // Initialize web-specific ModeInfo
    static ModeInfo web_mi = {0};
    web_mi.display = NULL;  // Not used in web
    web_mi.window = NULL;   // Not used in web
    web_mi.visual = NULL;   // Not used in web
    web_mi.colormap = 0;    // Not used in web
    web_mi.screen = 0;      // Single screen for web
    web_mi.screen_number = 0;  // Single screen for web
    web_mi.batchcount = 1;     // Single instance for web
    web_mi.wireframe_p = 0;    // No wireframe for web
    web_mi.polygon_count = 0;  // Initialize polygon counter
    web_mi.fps_p = 1;          // Enable FPS display for web
    web_mi.dpy = NULL;         // Display pointer (alias)
    web_mi.xgwa.width = 800;   // Default width
    web_mi.xgwa.height = 600;  // Default height
    web_mi.xgwa.visual = NULL; // Visual (not used in web)

    // Store function pointers for later use
    // (In a real implementation, these would be stored globally)

    // Initialize the hack
    init_func(&web_mi);

    // Set up the web canvas and start rendering loop
    // This is a simplified version - real implementation would be more complex

    return 0; // Success
}

// Web implementations of missing functions
void MI_INIT(ModeInfo *mi, void *bps) {
    // Initialize the ModeInfo structure
    // This is a simplified version for web builds
    mi->screen_number = 0;
    mi->batchcount = 1;
    mi->wireframe_p = 0;
    mi->polygon_count = 0;
    mi->fps_p = 1;  // Enable FPS display by default
    mi->dpy = NULL;
    mi->xgwa.width = 800;
    mi->xgwa.height = 600;
    mi->xgwa.visual = NULL;
}

GLXContext *init_GL(ModeInfo *mi) {
    // Initialize OpenGL context for web
    // This is a simplified version - in a real implementation,
    // you would set up the WebGL context here
    static GLXContext context = NULL;
    if (!context) {
        // Create a dummy context for web builds
        context = (GLXContext)malloc(sizeof(void*));
    }
    return &context;
}

// FPS display function for web builds - matches native version
void do_fps(ModeInfo *mi) {
    if (!mi || !mi->fps_p) return;

    // Get current time
    double current_time = emscripten_get_now() / 1000.0;

    // Update frame count
    frame_count++;

    // Calculate FPS every second
    if (current_time - last_fps_time >= 1.0) {
        current_fps = frame_count / (current_time - last_fps_time);
        frame_count = 0;
        last_fps_time = current_time;

        // Simulate load based on FPS (for demo purposes)
        current_load = fmax(0.0, fmin(100.0, (60.0 - current_fps) * 2.0));

        // Get polygon count from ModeInfo
        current_polys = mi->polygon_count;
    }

    // Save OpenGL state
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, MI_WIDTH(mi), 0, MI_HEIGHT(mi), -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    // Disable depth testing for 2D overlay
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);

    // Set up text rendering (simplified - in real implementation you'd use proper font rendering)
    glColor4f(FPS_COLOR_R, FPS_COLOR_G, FPS_COLOR_B, FPS_COLOR_A);

    // Position text in bottom left
    int x = FPS_X_OFFSET;
    int y = MI_HEIGHT(mi) - FPS_Y_OFFSET - FPS_FONT_SIZE * 3; // 3 lines of text

    // Render FPS text (simplified - using basic OpenGL primitives)
    // In a real implementation, you'd use proper text rendering
    char fps_text[256];
    snprintf(fps_text, sizeof(fps_text), "FPS: %.1f", current_fps);
    render_text_simple(x, y, fps_text);

    snprintf(fps_text, sizeof(fps_text), "Load: %.1f%%", current_load);
    render_text_simple(x, y - FPS_FONT_SIZE, fps_text);

    snprintf(fps_text, sizeof(fps_text), "Polys: %d", current_polys);
    render_text_simple(x, y - FPS_FONT_SIZE * 2, fps_text);

    // Restore OpenGL state
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopAttrib();
}

// Simple text rendering function for web builds
void render_text_simple(int x, int y, const char* text) {
    // This is a very basic text renderer using OpenGL primitives
    // In a real implementation, you'd use proper font rendering
    glRasterPos2i(x, y);

    // For now, we'll just draw a simple rectangle to represent text
    // This is a placeholder - real text rendering would be much more complex
    glBegin(GL_QUADS);
    glVertex2i(x, y);
    glVertex2i(x + strlen(text) * 8, y);
    glVertex2i(x + strlen(text) * 8, y + FPS_FONT_SIZE);
    glVertex2i(x, y + FPS_FONT_SIZE);
    glEnd();
}


