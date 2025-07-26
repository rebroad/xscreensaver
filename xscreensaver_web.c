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
#include "xlockmore_web.h"

// Missing OpenGL constants that might not be defined
#ifndef GL_COLOR_BUFFER_BIT
#define GL_COLOR_BUFFER_BIT 0x00004000
#endif

#ifndef GL_DEPTH_BUFFER_BIT
#define GL_DEPTH_BUFFER_BIT 0x00000100
#endif

#ifndef GL_DEPTH_TEST
#define GL_DEPTH_TEST 0x0B71
#endif

#ifndef GL_TRIANGLES
#define GL_TRIANGLES 0x0004
#endif

#ifndef GL_ARRAY_BUFFER
#define GL_ARRAY_BUFFER 0x8892
#endif

#ifndef GL_STATIC_DRAW
#define GL_STATIC_DRAW 0x88E4
#endif

#ifndef GL_FLOAT
#define GL_FLOAT 0x1406
#endif

#ifndef GL_FALSE
#define GL_FALSE 0
#endif

#ifndef GL_VERTEX_SHADER
#define GL_VERTEX_SHADER 0x8B31
#endif

#ifndef GL_FRAGMENT_SHADER
#define GL_FRAGMENT_SHADER 0x8B30
#endif

#ifndef GL_COMPILE_STATUS
#define GL_COMPILE_STATUS 0x8B81
#endif

#ifndef GL_LINK_STATUS
#define GL_LINK_STATUS 0x8B82
#endif

#ifndef GL_NO_ERROR
#define GL_NO_ERROR 0
#endif

#ifndef GL_MODELVIEW
#define GL_MODELVIEW 0x1700
#endif

#ifndef GL_PROJECTION
#define GL_PROJECTION 0x1701
#endif

#ifndef GL_LIGHTING
#define GL_LIGHTING 0x0B50
#endif

#ifndef GL_ALL_ATTRIB_BITS
#define GL_ALL_ATTRIB_BITS 0x000FFFFF
#endif

#ifndef GL_QUADS
#define GL_QUADS 0x0007
#endif

#ifndef GL_FRONT
#define GL_FRONT 0x0404
#endif

#ifndef GL_AMBIENT_AND_DIFFUSE
#define GL_AMBIENT_AND_DIFFUSE 0x1602
#endif

#ifndef GL_SMOOTH
#define GL_SMOOTH 0x1D01
#endif

#ifndef GL_NORMALIZE
#define GL_NORMALIZE 0x0BA1
#endif

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
static void* gltrackball_init(int ignore_device_rotation_p) {
    web_trackball_state* tb = (web_trackball_state*)calloc(1, sizeof(web_trackball_state));
    if (tb) {
        // Initialize to identity quaternion
        tb->q[0] = 0.0f; tb->q[1] = 0.0f; tb->q[2] = 0.0f; tb->q[3] = 1.0f;
        tb->dragging = 0;
    }
    return (void*)tb;
}

static void gltrackball_free(void* tb) {
    if (tb) free(tb);
}

static void gltrackball_reset(void* tb, GLfloat x, GLfloat y) {
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

static void gltrackball_rotate(void* tb) {
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

static Bool gltrackball_event_handler(XEvent* event, void* tb,
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


// Web implementations of missing functions
void MI_INIT(ModeInfo *mi, void *bps) {
    // Initialize the ModeInfo structure
    // This is a simplified version for web builds
    mi->screen_number = 0;
    mi->batchcount = 1;
    mi->wireframe_p = 0;
    mi->polygon_count = 0;
    mi->fps_p = 0;  // Will be set by resource system, not hardcoded
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

// Web-specific FPS tracking state
typedef struct {
    double last_frame_time;
    double last_fps_update_time;
    int frame_count;
    double current_fps;
    double current_load;
    int current_polys;
    double target_frame_time;  // Target frame time (e.g., 1.0/30.0 for 30 FPS)
    double total_idle_time;
    double last_idle_start;
} web_fps_state;

static web_fps_state web_fps_state_var = {0};

// Generic UI control system
#define MAX_WEB_PARAMETERS 20
static web_parameter web_parameters[MAX_WEB_PARAMETERS];
static int web_parameter_count = 0;

// Register a parameter for web UI control
void register_web_parameter(const char* name, const char* display_name, web_param_type type,
                           void* variable, void (*update_callback)(void*),
                           double min_value, double max_value, double step_value) {
    if (web_parameter_count >= MAX_WEB_PARAMETERS) return;

    web_parameters[web_parameter_count].name = name;
    web_parameters[web_parameter_count].display_name = display_name;
    web_parameters[web_parameter_count].type = type;
    web_parameters[web_parameter_count].variable = variable;
    web_parameters[web_parameter_count].update_callback = update_callback;
    web_parameters[web_parameter_count].min_value = min_value;
    web_parameters[web_parameter_count].max_value = max_value;
    web_parameters[web_parameter_count].step_value = step_value;

    web_parameter_count++;
}

// Generic setter functions that can be exported
EMSCRIPTEN_KEEPALIVE
void web_set_parameter(const char* name, double value) {
    for (int i = 0; i < web_parameter_count; i++) {
        if (strcmp(web_parameters[i].name, name) == 0) {
            switch (web_parameters[i].type) {
                case WEB_PARAM_FLOAT:
                    *(float*)web_parameters[i].variable = (float)value;
                    break;
                case WEB_PARAM_INT:
                    *(int*)web_parameters[i].variable = (int)value;
                    break;
                case WEB_PARAM_BOOL:
                    *(int*)web_parameters[i].variable = (int)value;
                    break;
            }

            if (web_parameters[i].update_callback) {
                web_parameters[i].update_callback(web_parameters[i].variable);
            }
            break;
        }
    }
}

// Get parameter info for web UI generation
EMSCRIPTEN_KEEPALIVE
int web_get_parameter_count() {
    return web_parameter_count;
}

EMSCRIPTEN_KEEPALIVE
const char* web_get_parameter_name(int index) {
    if (index >= 0 && index < web_parameter_count) {
        return web_parameters[index].name;
    }
    return NULL;
}

EMSCRIPTEN_KEEPALIVE
const char* web_get_parameter_display_name(int index) {
    if (index >= 0 && index < web_parameter_count) {
        return web_parameters[index].display_name;
    }
    return NULL;
}

EMSCRIPTEN_KEEPALIVE
int web_get_parameter_type(int index) {
    if (index >= 0 && index < web_parameter_count) {
        return web_parameters[index].type;
    }
    return -1;
}

EMSCRIPTEN_KEEPALIVE
double web_get_parameter_value(int index) {
    if (index >= 0 && index < web_parameter_count) {
        switch (web_parameters[index].type) {
            case WEB_PARAM_FLOAT:
                return *(float*)web_parameters[index].variable;
            case WEB_PARAM_INT:
                return *(int*)web_parameters[index].variable;
            case WEB_PARAM_BOOL:
                return *(int*)web_parameters[index].variable;
        }
    }
    return 0.0;
}

EMSCRIPTEN_KEEPALIVE
double web_get_parameter_min(int index) {
    if (index >= 0 && index < web_parameter_count) {
        return web_parameters[index].min_value;
    }
    return 0.0;
}

EMSCRIPTEN_KEEPALIVE
double web_get_parameter_max(int index) {
    if (index >= 0 && index < web_parameter_count) {
        return web_parameters[index].max_value;
    }
    return 1.0;
}

EMSCRIPTEN_KEEPALIVE
double web_get_parameter_step(int index) {
    if (index >= 0 && index < web_parameter_count) {
        return web_parameters[index].step_value;
    }
    return 0.1;
}

// Web-compatible FPS display function that integrates with existing FPS system
void do_fps(ModeInfo *mi) {
    // This function should be called by hacks that want to display FPS
    // It will use the existing FPS system if available, or provide a web fallback

    // Check if FPS is enabled via the resource system
    if (!mi || !mi->fps_p) return;

    // Initialize FPS state if needed
    if (web_fps_state_var.last_frame_time == 0.0) {
        web_fps_state_var.last_frame_time = emscripten_get_now() / 1000.0;
        web_fps_state_var.last_fps_update_time = web_fps_state_var.last_frame_time;
        web_fps_state_var.frame_count = 0;
        web_fps_state_var.current_fps = 0.0;
        web_fps_state_var.current_load = 0.0;
        web_fps_state_var.current_polys = 0;
        web_fps_state_var.target_frame_time = 1.0 / 30.0;  // Default to 30 FPS target
        web_fps_state_var.total_idle_time = 0.0;
        web_fps_state_var.last_idle_start = 0.0;
    }

    // Get current time
    double current_time = emscripten_get_now() / 1000.0;
    double frame_time = current_time - web_fps_state_var.last_frame_time;

    // Update frame count
    web_fps_state_var.frame_count++;

    // Calculate FPS and load every second
    if (current_time - web_fps_state_var.last_fps_update_time >= 1.0) {
        double update_interval = current_time - web_fps_state_var.last_fps_update_time;
        web_fps_state_var.current_fps = web_fps_state_var.frame_count / update_interval;

        // Calculate load based on idle time
        // Load = (1 - idle_time/total_time) * 100
        double total_time = update_interval;
        double idle_time = web_fps_state_var.total_idle_time;
        web_fps_state_var.current_load = fmax(0.0, fmin(100.0, (1.0 - idle_time / total_time) * 100.0));

        // Get polygon count from ModeInfo
        web_fps_state_var.current_polys = mi->polygon_count;

        // Reset counters
        web_fps_state_var.frame_count = 0;
        web_fps_state_var.last_fps_update_time = current_time;
        web_fps_state_var.total_idle_time = 0.0;
    }

    // Track idle time if we're ahead of target frame rate
    if (frame_time < web_fps_state_var.target_frame_time) {
        double idle_time = web_fps_state_var.target_frame_time - frame_time;
        web_fps_state_var.total_idle_time += idle_time;
    }

    web_fps_state_var.last_frame_time = current_time;

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

    // Set up text rendering
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    // Position text in bottom left (matching native behavior)
    int x = 10;
    int y = MI_HEIGHT(mi) - 10 - 14 * 3; // 3 lines of text, 14px height

    // Display actual FPS values
    char fps_text[256];
    snprintf(fps_text, sizeof(fps_text), "FPS: %.1f", web_fps_state_var.current_fps);
    render_text_simple(x, y, fps_text);

    snprintf(fps_text, sizeof(fps_text), "Load: %.1f%%", web_fps_state_var.current_load);
    render_text_simple(x, y - 14, fps_text);

    snprintf(fps_text, sizeof(fps_text), "Polys: %d", web_fps_state_var.current_polys);
    render_text_simple(x, y - 28, fps_text);

    // Restore OpenGL state
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopAttrib();
}

// Function to set the target frame rate for FPS calculations
void web_fps_set_target(double target_fps) {
    if (target_fps > 0.0) {
        web_fps_state_var.target_frame_time = 1.0 / target_fps;
    }
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
    glVertex2i(x + strlen(text) * 8, y + 14);
    glVertex2i(x, y + 14);
    glEnd();
}

// WebGL 2.0 shader program
static GLuint shader_program = 0;
static GLuint vertex_shader = 0;
static GLuint fragment_shader = 0;

// Matrix stack management
#define MAX_MATRIX_STACK_DEPTH 32
typedef struct {
    GLfloat m[16];
} Matrix4f;

typedef struct {
    Matrix4f stack[MAX_MATRIX_STACK_DEPTH];
    int top;
} MatrixStack;

static MatrixStack modelview_stack;
static MatrixStack projection_stack;
static MatrixStack texture_stack;
static GLenum current_matrix_mode = GL_MODELVIEW;

// Immediate mode state
#define MAX_VERTICES 100000
typedef struct {
    GLfloat x, y, z;
} Vertex3f;

typedef struct {
    GLfloat r, g, b, a;
} Color4f;

typedef struct {
    GLfloat x, y, z;
} Normal3f;

typedef struct {
    Vertex3f vertices[MAX_VERTICES];
    Color4f colors[MAX_VERTICES];
    Normal3f normals[MAX_VERTICES];
    int vertex_count;
    GLenum primitive_type;
    Bool in_begin_end;
} ImmediateMode;

static ImmediateMode immediate;
static Color4f current_color = {1.0f, 1.0f, 1.0f, 1.0f};
static Normal3f current_normal = {0.0f, 0.0f, 1.0f};
static int total_vertices_this_frame = 0;

// Matrix utility functions
static void matrix_identity(Matrix4f *m) {
    for (int i = 0; i < 16; i++) {
        m->m[i] = (i % 5 == 0) ? 1.0f : 0.0f;
    }
}

static void matrix_multiply(Matrix4f *result, const Matrix4f *a, const Matrix4f *b) {
    Matrix4f temp;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            temp.m[i * 4 + j] = 0;
            for (int k = 0; k < 4; k++) {
                temp.m[i * 4 + j] += a->m[i * 4 + k] * b->m[k * 4 + j];
            }
        }
    }
    *result = temp;
}

static void matrix_translate(Matrix4f *m, GLfloat x, GLfloat y, GLfloat z) {
    Matrix4f translate;
    matrix_identity(&translate);
    translate.m[12] = x;
    translate.m[13] = y;
    translate.m[14] = z;
    matrix_multiply(m, m, &translate);
}

static void matrix_rotate(Matrix4f *m, GLfloat angle, GLfloat x, GLfloat y, GLfloat z) {
    // Simplified rotation - only around Z axis for now
    if (z != 0) {
        GLfloat rad = angle * M_PI / 180.0f;
        GLfloat c = cos(rad);
        GLfloat s = sin(rad);
        Matrix4f rotate;
        matrix_identity(&rotate);
        rotate.m[0] = c;
        rotate.m[1] = s;
        rotate.m[4] = -s;
        rotate.m[5] = c;
        matrix_multiply(m, m, &rotate);
    }
}

static void matrix_scale(Matrix4f *m, GLfloat x, GLfloat y, GLfloat z) {
    Matrix4f scale;
    matrix_identity(&scale);
    scale.m[0] = x;
    scale.m[5] = y;
    scale.m[10] = z;
    matrix_multiply(m, m, &scale);
}

static MatrixStack* get_current_matrix_stack(void) {
    switch (current_matrix_mode) {
        case GL_MODELVIEW:
            return &modelview_stack;
        case GL_PROJECTION:
            return &projection_stack;
        case GL_TEXTURE:
            return &texture_stack;
        default:
            return &modelview_stack;
    }
}

// WebGL 2.0 shader compilation
static GLuint compile_shader(const char *source, GLenum type) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLchar info_log[512];
        glGetShaderInfoLog(shader, 512, NULL, info_log);
        printf("Shader compilation error: %s\n", info_log);
    }

    return shader;
}

static void init_shaders() {
    const char *vertex_source =
        "#version 300 es\n"
        "in vec3 position;\n"
        "in vec3 color;\n"
        "in vec3 normal;\n"
        "uniform mat4 modelview;\n"
        "uniform mat4 projection;\n"
        "out vec3 frag_color;\n"
        "out vec3 frag_normal;\n"
        "void main() {\n"
        "    gl_Position = projection * modelview * vec4(position, 1.0);\n"
        "    frag_color = color;\n"
        "    frag_normal = normal;\n"
        "}\n";

    const char *fragment_source =
        "#version 300 es\n"
        "precision mediump float;\n"
        "in vec3 frag_color;\n"
        "in vec3 frag_normal;\n"
        "out vec4 out_color;\n"
        "void main() {\n"
        "    out_color = vec4(frag_color, 1.0);\n"
        "}\n";

    vertex_shader = compile_shader(vertex_source, GL_VERTEX_SHADER);
    fragment_shader = compile_shader(fragment_source, GL_FRAGMENT_SHADER);

    shader_program = glCreateProgram();
    glAttachShader(shader_program, vertex_shader);
    glAttachShader(shader_program, fragment_shader);
    glLinkProgram(shader_program);

    GLint success;
    glGetProgramiv(shader_program, GL_LINK_STATUS, &success);
    if (!success) {
        GLchar info_log[512];
        glGetProgramInfoLog(shader_program, 512, NULL, info_log);
        printf("Shader linking error: %s\n", info_log);
    }
}

// Initialize OpenGL state
static void init_opengl_state() {
    // Initialize matrix stacks
    matrix_identity(&modelview_stack.stack[0]);
    matrix_identity(&projection_stack.stack[0]);
    matrix_identity(&texture_stack.stack[0]);
    modelview_stack.top = 0;
    projection_stack.top = 0;
    texture_stack.top = 0;

    // Initialize immediate mode
    immediate.in_begin_end = False;
    immediate.vertex_count = 0;

    // Initialize shaders
    init_shaders();

    // Set up basic OpenGL state
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

// Immediate mode OpenGL function implementations
void glMatrixMode(GLenum mode) {
    current_matrix_mode = mode;
}

void glLoadIdentity(void) {
    MatrixStack *stack = get_current_matrix_stack();
    matrix_identity(&stack->stack[stack->top]);
}

void glPushMatrix(void) {
    MatrixStack *stack = get_current_matrix_stack();
    if (stack->top < MAX_MATRIX_STACK_DEPTH - 1) {
        stack->top++;
        stack->stack[stack->top] = stack->stack[stack->top - 1];
    }
}

void glPopMatrix(void) {
    MatrixStack *stack = get_current_matrix_stack();
    if (stack->top > 0) {
        stack->top--;
    }
}

void glTranslatef(GLfloat x, GLfloat y, GLfloat z) {
    MatrixStack *stack = get_current_matrix_stack();
    matrix_translate(&stack->stack[stack->top], x, y, z);
}

void glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z) {
    MatrixStack *stack = get_current_matrix_stack();
    matrix_rotate(&stack->stack[stack->top], angle, x, y, z);
}

void glScalef(GLfloat x, GLfloat y, GLfloat z) {
    MatrixStack *stack = get_current_matrix_stack();
    matrix_scale(&stack->stack[stack->top], x, y, z);
}

void glOrtho(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble near_val, GLdouble far_val) {
    MatrixStack *stack = get_current_matrix_stack();

    // Create orthographic projection matrix
    Matrix4f ortho;
    matrix_identity(&ortho);

    GLfloat tx = -(right + left) / (right - left);
    GLfloat ty = -(top + bottom) / (top - bottom);
    GLfloat tz = -(far_val + near_val) / (far_val - near_val);

    ortho.m[0] = 2.0f / (right - left);
    ortho.m[5] = 2.0f / (top - bottom);
    ortho.m[10] = -2.0f / (far_val - near_val);
    ortho.m[12] = tx;
    ortho.m[13] = ty;
    ortho.m[14] = tz;

    matrix_multiply(&stack->stack[stack->top], &ortho, &stack->stack[stack->top]);
}

void glFrustum(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble near_val, GLdouble far_val) {
    MatrixStack *stack = get_current_matrix_stack();

    // Create perspective projection matrix
    Matrix4f frustum;
    matrix_identity(&frustum);

    GLfloat a = (right + left) / (right - left);
    GLfloat b = (top + bottom) / (top - bottom);
    GLfloat c = -(far_val + near_val) / (far_val - near_val);
    GLfloat d = -(2 * far_val * near_val) / (far_val - near_val);

    frustum.m[0] = 2 * near_val / (right - left);
    frustum.m[5] = 2 * near_val / (top - bottom);
    frustum.m[8] = a;
    frustum.m[9] = b;
    frustum.m[10] = c;
    frustum.m[11] = -1;
    frustum.m[14] = d;
    frustum.m[15] = 0;

    matrix_multiply(&stack->stack[stack->top], &stack->stack[stack->top], &frustum);
}

void glMultMatrixf(const GLfloat *m) {
    Matrix4f matrix;
    for (int i = 0; i < 16; i++) {
        matrix.m[i] = m[i];
    }

    MatrixStack *stack = get_current_matrix_stack();
    matrix_multiply(&stack->stack[stack->top], &stack->stack[stack->top], &matrix);
}

void glColor3f(GLfloat r, GLfloat g, GLfloat b) {
    current_color.r = r;
    current_color.g = g;
    current_color.b = b;
    current_color.a = 1.0f;
}

void glColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a) {
    current_color.r = r;
    current_color.g = g;
    current_color.b = b;
    current_color.a = a;
}

void glNormal3f(GLfloat nx, GLfloat ny, GLfloat nz) {
    current_normal.x = nx;
    current_normal.y = ny;
    current_normal.z = nz;
}

void glBegin(GLenum mode) {
    immediate.in_begin_end = True;
    immediate.primitive_type = mode;
    immediate.vertex_count = 0;
}

void glVertex3f(GLfloat x, GLfloat y, GLfloat z) {
    if (!immediate.in_begin_end) return;

    if (immediate.vertex_count >= MAX_VERTICES) {
        printf("WARNING: Vertex limit reached (%d), dropping vertex!\n", MAX_VERTICES);
        return;
    }

    immediate.vertices[immediate.vertex_count].x = x;
    immediate.vertices[immediate.vertex_count].y = y;
    immediate.vertices[immediate.vertex_count].z = z;
    immediate.colors[immediate.vertex_count] = current_color;
    immediate.normals[immediate.vertex_count] = current_normal;
    immediate.vertex_count++;
    total_vertices_this_frame++;
}

void glEnd(void) {
    if (!immediate.in_begin_end || immediate.vertex_count == 0) {
        immediate.in_begin_end = False;
        return;
    }

    // Use our WebGL 2.0 shader program
    glUseProgram(shader_program);

    // Set up projection matrix
    GLint projection_loc = glGetUniformLocation(shader_program, "projection");
    if (projection_loc != -1) {
        glUniformMatrix4fv(projection_loc, 1, GL_FALSE, projection_stack.stack[projection_stack.top].m);
    }

    // Set up modelview matrix
    GLint modelview_loc = glGetUniformLocation(shader_program, "modelview");
    if (modelview_loc != -1) {
        glUniformMatrix4fv(modelview_loc, 1, GL_FALSE, modelview_stack.stack[modelview_stack.top].m);
    }

    // Create and bind VBOs
    GLuint vbo_vertices, vbo_colors, vbo_normals;
    glGenBuffers(1, &vbo_vertices);
    glGenBuffers(1, &vbo_colors);
    glGenBuffers(1, &vbo_normals);

    glBindBuffer(GL_ARRAY_BUFFER, vbo_vertices);
    glBufferData(GL_ARRAY_BUFFER, immediate.vertex_count * sizeof(Vertex3f), immediate.vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, vbo_colors);
    glBufferData(GL_ARRAY_BUFFER, immediate.vertex_count * sizeof(Color4f), immediate.colors, GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, vbo_normals);
    glBufferData(GL_ARRAY_BUFFER, immediate.vertex_count * sizeof(Normal3f), immediate.normals, GL_STATIC_DRAW);

    // Set up vertex attributes
    glBindBuffer(GL_ARRAY_BUFFER, vbo_vertices);
    GLint pos_attrib = glGetAttribLocation(shader_program, "position");
    if (pos_attrib != -1) {
        glEnableVertexAttribArray(pos_attrib);
        glVertexAttribPointer(pos_attrib, 3, GL_FLOAT, GL_FALSE, 0, 0);
    }

    glBindBuffer(GL_ARRAY_BUFFER, vbo_colors);
    GLint color_attrib = glGetAttribLocation(shader_program, "color");
    if (color_attrib != -1) {
        glEnableVertexAttribArray(color_attrib);
        glVertexAttribPointer(color_attrib, 4, GL_FLOAT, GL_FALSE, 0, 0);
    }

    glBindBuffer(GL_ARRAY_BUFFER, vbo_normals);
    GLint normal_attrib = glGetAttribLocation(shader_program, "normal");
    if (normal_attrib != -1) {
        glEnableVertexAttribArray(normal_attrib);
        glVertexAttribPointer(normal_attrib, 3, GL_FLOAT, GL_FALSE, 0, 0);
    }

    // Draw
    glDrawArrays(immediate.primitive_type, 0, immediate.vertex_count);

    // Cleanup
    glDeleteBuffers(1, &vbo_vertices);
    glDeleteBuffers(1, &vbo_colors);
    glDeleteBuffers(1, &vbo_normals);

    immediate.in_begin_end = False;
}

// Additional OpenGL functions
void glClear(GLbitfield mask) {
    if (mask & GL_COLOR_BUFFER_BIT) {
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    }
    if (mask & GL_DEPTH_BUFFER_BIT) {
        glClear(GL_DEPTH_BUFFER_BIT);
    }
}

void glEnable(GLenum cap) {
    if (cap == GL_DEPTH_TEST) {
        glEnable(GL_DEPTH_TEST);
    }
}

void glDisable(GLenum cap) {
    if (cap == GL_DEPTH_TEST) {
        glDisable(GL_DEPTH_TEST);
    }
}

// WebGL context handle
static EMSCRIPTEN_WEBGL_CONTEXT_HANDLE webgl_context = -1;

// Global ModeInfo for web
static ModeInfo web_mi;

// Function pointers for the hack's functions
typedef void (*init_func)(ModeInfo *);
typedef void (*draw_func)(ModeInfo *);
typedef void (*reshape_func)(ModeInfo *, int, int);
typedef void (*free_func)(ModeInfo *);
typedef Bool (*handle_event_func)(ModeInfo *, XEvent *);

static init_func hack_init = NULL;
static draw_func hack_draw = NULL;
static reshape_func hack_reshape = NULL;
static free_func hack_free = NULL;
static handle_event_func hack_handle_event = NULL;

// Animation state
static Bool rendering_enabled = True;
static int frame_count = 0;

// Main loop callback
void main_loop(void) {
    frame_count++;
    total_vertices_this_frame = 0; // Reset vertex counter each frame

    // Check if rendering is disabled
    if (!rendering_enabled) {
        return; // Skip rendering entirely
    }

    // Clear the screen
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (hack_draw) {
        hack_draw(&web_mi);
    }
}

// Initialize WebGL context and OpenGL state
static int init_webgl() {
    EmscriptenWebGLContextAttributes attrs;
    emscripten_webgl_init_context_attributes(&attrs);
    attrs.alpha = 0;
    attrs.depth = 1;
    attrs.stencil = 0;
    attrs.antialias = 1;
    attrs.premultipliedAlpha = 0;
    attrs.preserveDrawingBuffer = 0;
    attrs.powerPreference = EM_WEBGL_POWER_PREFERENCE_DEFAULT;
    attrs.failIfMajorPerformanceCaveat = 0;
    attrs.enableExtensionsByDefault = 1;
    attrs.explicitSwapControl = 0;
    attrs.proxyContextToMainThread = EMSCRIPTEN_WEBGL_CONTEXT_PROXY_DISALLOW;
    attrs.renderViaOffscreenBackBuffer = 0;
    attrs.majorVersion = 2;
    attrs.minorVersion = 0;

    webgl_context = emscripten_webgl_create_context("#canvas", &attrs);
    if (webgl_context < 0) {
        printf("Failed to create WebGL context! Error: %lu\n", webgl_context);
        return 0;
    }

    if (emscripten_webgl_make_context_current(webgl_context) != EMSCRIPTEN_RESULT_SUCCESS) {
        printf("Failed to make WebGL context current!\n");
        return 0;
    }

    // Initialize OpenGL state
    init_opengl_state();

    return 1;
}

// Generic web initialization
EMSCRIPTEN_KEEPALIVE
int xscreensaver_web_init(init_func init, draw_func draw, reshape_func reshape, free_func free, handle_event_func handle_event) {
    printf("xscreensaver_web_init called\n");

    hack_init = init;
    hack_draw = draw;
    hack_reshape = reshape;
    hack_free = free;
    hack_handle_event = handle_event;

    printf("Function pointers set: init=%p, draw=%p, reshape=%p, free=%p, handle_event=%p\n",
           (void*)init, (void*)draw, (void*)reshape, (void*)free, (void*)handle_event);

    // Initialize ModeInfo
    web_mi.screen = 0;
    web_mi.window = (void*)1;
    web_mi.display = (void*)1;
    web_mi.visual = (void*)1;
    web_mi.colormap = 1;
    web_mi.screen_number = 0;
    web_mi.batchcount = 1;
    web_mi.wireframe_p = 0;
    web_mi.polygon_count = 0;
    web_mi.fps_p = 0;
    web_mi.dpy = (void*)1;
    web_mi.xgwa.width = 800;
    web_mi.xgwa.height = 600;
    web_mi.xgwa.visual = (void*)1;

    printf("ModeInfo initialized: width=%d, height=%d\n", web_mi.xgwa.width, web_mi.xgwa.height);

    // Check canvas size
    int canvas_width, canvas_height;
    emscripten_get_canvas_element_size("#canvas", &canvas_width, &canvas_height);
    printf("Canvas size: %dx%d\n", canvas_width, canvas_height);

    // Initialize WebGL
    printf("Initializing WebGL...\n");
    if (!init_webgl()) {
        printf("WebGL initialization failed!\n");
        return 0;
    }
    printf("WebGL initialized successfully\n");

    // Call the hack's init function
    if (hack_init) {
        printf("Calling hack_init...\n");
        hack_init(&web_mi);
        printf("hack_init completed\n");
    } else {
        printf("hack_init is NULL!\n");
    }

    // Set up reshape
    if (hack_reshape) {
        printf("Calling hack_reshape...\n");
        hack_reshape(&web_mi, web_mi.xgwa.width, web_mi.xgwa.height);
        printf("hack_reshape completed\n");
    } else {
        printf("hack_reshape is NULL!\n");
    }

    // Set up the main loop (60 FPS)
    printf("Setting up main loop...\n");
    emscripten_set_main_loop(main_loop, 60, 1);
    printf("Main loop set up successfully\n");

    return 1;
}

// Web-specific function exports for UI controls
EMSCRIPTEN_KEEPALIVE
void stop_rendering() {
    rendering_enabled = False;
    printf("Rendering stopped\n");
}

EMSCRIPTEN_KEEPALIVE
void start_rendering() {
    rendering_enabled = True;
    printf("Rendering started\n");
}

EMSCRIPTEN_KEEPALIVE
void reshape_hextrail_wrapper(int width, int height) {
    if (hack_reshape) {
        web_mi.xgwa.width = width;
        web_mi.xgwa.height = height;
        hack_reshape(&web_mi, width, height);
        printf("Reshaped to %dx%d\n", width, height);
    }
}

// Cleanup function
EMSCRIPTEN_KEEPALIVE
void xscreensaver_web_cleanup() {
    if (hack_free) {
        hack_free(&web_mi);
    }

    if (webgl_context >= 0) {
        emscripten_webgl_destroy_context(webgl_context);
        webgl_context = -1;
    }
}

// Missing OpenGL functions
void glPushAttrib(GLbitfield mask) {
    // WebGL doesn't support attribute stacks, so this is a no-op
    (void)mask;
}

void glPopAttrib(void) {
    // WebGL doesn't support attribute stacks, so this is a no-op
}

void glRasterPos2i(GLint x, GLint y) {
    // WebGL doesn't support raster position, so this is a no-op
    (void)x;
    (void)y;
}

void glVertex2i(GLint x, GLint y) {
    // Convert to 3D by adding z=0
    glVertex3f((GLfloat)x, (GLfloat)y, 0.0f);
}

void glColor4fv(const GLfloat *v) {
    glColor4f(v[0], v[1], v[2], v[3]);
}

void glMaterialfv(GLenum face, GLenum pname, const GLfloat *params) {
    // WebGL doesn't support materials, so this is a no-op
    (void)face;
    (void)pname;
    (void)params;
}

void glShadeModel(GLenum mode) {
    // WebGL doesn't support shade model, so this is a no-op
    (void)mode;
}

void gluPerspective(GLfloat fovy, GLfloat aspect, GLfloat zNear, GLfloat zFar) {
    GLfloat xmin, xmax, ymin, ymax;

    ymax = zNear * tan(fovy * M_PI / 360.0f);
    ymin = -ymax;
    xmin = ymin * aspect;
    xmax = ymax * aspect;

    glFrustum(xmin, xmax, ymin, ymax, zNear, zFar);
}

void gluLookAt(GLfloat eyex, GLfloat eyey, GLfloat eyez,
               GLfloat centerx, GLfloat centery, GLfloat centerz,
               GLfloat upx, GLfloat upy, GLfloat upz) {
    GLfloat m[16];
    GLfloat x[3], y[3], z[3];
    GLfloat mag;

    /* Make rotation matrix */

    /* Z vector */
    z[0] = eyex - centerx;
    z[1] = eyey - centery;
    z[2] = eyez - centerz;
    mag = sqrt(z[0] * z[0] + z[1] * z[1] + z[2] * z[2]);
    if (mag) {
        z[0] /= mag;
        z[1] /= mag;
        z[2] /= mag;
    }

    /* Y vector */
    y[0] = upx;
    y[1] = upy;
    y[2] = upz;

    /* X vector = Y cross Z */
    x[0] = y[1] * z[2] - y[2] * z[1];
    x[1] = -y[0] * z[2] + y[2] * z[0];
    x[2] = y[0] * z[1] - y[1] * z[0];

    /* Recompute Y = Z cross X */
    y[0] = z[1] * x[2] - z[2] * x[1];
    y[1] = -z[0] * x[2] + z[2] * x[0];
    y[2] = z[0] * x[1] - z[1] * x[0];

    /* cross product gives area of parallelogram, which is < 1.0 for
     * non-perpendicular unit vectors; so normalize x, y here
     */

    mag = sqrt(x[0] * x[0] + x[1] * x[1] + x[2] * x[2]);
    if (mag) {
        x[0] /= mag;
        x[1] /= mag;
        x[2] /= mag;
    }

    mag = sqrt(y[0] * y[0] + y[1] * y[1] + y[2] * y[2]);
    if (mag) {
        y[0] /= mag;
        y[1] /= mag;
        y[2] /= mag;
    }

#define M(row,col)  m[col*4+row]
    M(0,0) = x[0];  M(0,1) = x[1];  M(0,2) = x[2];  M(0,3) = 0.0;
    M(1,0) = y[0];  M(1,1) = y[1];  M(1,2) = y[2];  M(1,3) = 0.0;
    M(2,0) = z[0];  M(2,1) = z[1];  M(2,2) = z[2];  M(2,3) = 0.0;
    M(3,0) = 0.0;   M(3,1) = 0.0;   M(3,2) = 0.0;   M(3,3) = 1.0;
#undef M
    glMultMatrixf(m);

    /* Translate Eye to Origin */
    glTranslatef(-eyex, -eyey, -eyez);
}

Bool screenhack_event_helper(void *display, void *window, void *event) {
    // Web stub - events are handled differently in web environment
    (void)display;
    (void)window;
    (void)event;
    return False;
}

// Missing X11 function for web builds
int XLookupString(XKeyEvent *event_struct, char *buffer_return, int bytes_buffer, KeySym *keysym_return, XComposeStatus *status_in_out) {
    // Web stub - return empty string for now
    if (buffer_return && bytes_buffer > 0) {
        buffer_return[0] = '\0';
    }
    if (keysym_return) *keysym_return = 0;
    return 0;
}

// Web-specific function exports for UI controls (using generic system)
EMSCRIPTEN_KEEPALIVE
void set_speed(GLfloat new_speed) {
    web_set_parameter("speed", (double)new_speed);
}

EMSCRIPTEN_KEEPALIVE
void set_thickness(GLfloat new_thickness) {
    web_set_parameter("thickness", (double)new_thickness);
}

EMSCRIPTEN_KEEPALIVE
void set_spin(int new_spin_enabled) {
    web_set_parameter("spin", (double)new_spin_enabled);
}

EMSCRIPTEN_KEEPALIVE
void set_wander(int new_wander_enabled) {
    web_set_parameter("wander", (double)new_wander_enabled);
}

EMSCRIPTEN_KEEPALIVE
void handle_mouse_drag(int delta_x, int delta_y) {
    // This would need to be implemented per-hack
    (void)delta_x;
    (void)delta_y;
}

EMSCRIPTEN_KEEPALIVE
void handle_mouse_wheel(int delta) {
    // This would need to be implemented per-hack
    (void)delta;
}

EMSCRIPTEN_KEEPALIVE
void handle_keypress(int keycode, int charcode) {
    // This would need to be implemented per-hack
    (void)keycode;
    (void)charcode;
}

// Random number generation for hextrail
static unsigned int random_seed = 1;
static unsigned int webgl_random() {
    random_seed = random_seed * 1103515245 + 12345;
    return (random_seed >> 16) & 0x7fff;
}

double frand(double max) {
    return ((double)webgl_random() / 32767.0) * max;
}


