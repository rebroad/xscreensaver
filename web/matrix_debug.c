#ifdef MATRIX_DEBUG
#include "matrix_debug.h"
#include <stdio.h>
#include <stdlib.h>

// Forward declarations for WebGL functions (needed because xscreensaver_web.c is included later)
#ifdef WEB_BUILD
extern void gluPerspective(GLdouble fovy, GLdouble aspect, GLdouble zNear, GLdouble zFar);
extern void gluLookAt(GLdouble eyex, GLdouble eyey, GLdouble eyez,
                      GLdouble centerx, GLdouble centery, GLdouble centerz,
                      GLdouble upx, GLdouble upy, GLdouble upz);
#endif

#include <string.h>
#include <stdarg.h>
#include <math.h>
#ifdef __linux__
#include <dlfcn.h>
#endif
#ifdef WEB_BUILD
#include <emscripten.h>
#endif

#if !defined(WEB_BUILD) && defined(__linux__)
#define GET_FUNC_PTR(func_ptr, func_name) \
    do { \
        if (!func_ptr) { \
            _Pragma("GCC diagnostic push") \
            _Pragma("GCC diagnostic ignored \"-Wpedantic\"") \
            func_ptr = (typeof(func_ptr))dlsym(RTLD_NEXT, func_name); \
            if (!func_ptr) { \
                func_ptr = (typeof(func_ptr))dlsym(RTLD_DEFAULT, func_name); \
            } \
            _Pragma("GCC diagnostic pop") \
        } \
    } while(0)
#endif

// Macro to handle auto-initialization for native builds
// Both native and web builds use lazy initialization
#define AUTO_INIT_NATIVE(func_ptr) \
    do { \
        if (!func_ptr) { \
            init_matrix_debug_functions(); \
        } \
    } while(0)

// Global variables for matrix state tracking (matching header declarations)
GLenum current_matrix_mode = GL_MODELVIEW;

// Frame counting for debug output limiting
static int frame_count = 0;
static int max_debug_frames = 5;
float modelview_matrix[16] = {
    1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f
};
float projection_matrix[16] = {
    1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f
};
float texture_matrix[16] = {
    1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f
};

static void (*real_glMatrixMode)(GLenum) = NULL;
static void (*real_glLoadIdentity)(void) = NULL;
static void (*real_glOrtho)(GLdouble, GLdouble, GLdouble, GLdouble, GLdouble, GLdouble) = NULL;
static void (*real_glFrustum)(GLdouble, GLdouble, GLdouble, GLdouble, GLdouble, GLdouble) = NULL;
static void (*real_glTranslatef)(GLfloat, GLfloat, GLfloat) = NULL;
static void (*real_glRotatef)(GLfloat, GLfloat, GLfloat, GLfloat) = NULL;
static void (*real_glScalef)(GLfloat, GLfloat, GLfloat) = NULL;
static void (*real_glPushMatrix)(void) = NULL;
static void (*real_glPopMatrix)(void) = NULL;
static void (*real_glMultMatrixf)(const GLfloat*) = NULL;
static void (*real_glMultMatrixd)(const GLdouble*) = NULL;
static void (*real_glViewport)(GLint, GLint, GLsizei, GLsizei) = NULL;
static void (*real_gluPerspective)(GLdouble, GLdouble, GLdouble, GLdouble) = NULL;
static void (*real_gluLookAt)(GLdouble, GLdouble, GLdouble, GLdouble, GLdouble, GLdouble, GLdouble, GLdouble, GLdouble) = NULL;
static void (*real_glTranslated)(GLdouble, GLdouble, GLdouble) = NULL;

// Debug logging function
void matrix_debug_log(const char* format, ...) {
    #ifdef MATRIX_DEBUG
    if (frame_count < max_debug_frames) {
        va_list args;
        va_start(args, format);
        #ifdef WEB_BUILD
        // For web builds, use printf which goes to browser console
        vprintf(format, args);
        #else
        // For native builds, use stderr
        vfprintf(stderr, format, args);
        #endif
        va_end(args);
    }
    #endif
}

// Function to increment frame counter (call this at the start of each frame)
void matrix_debug_next_frame(void) {
    #ifdef MATRIX_DEBUG
    frame_count++;
    if (frame_count == max_debug_frames) {
        matrix_debug_log("Matrix debug: Reached frame limit (%d), stopping debug output\n", max_debug_frames);
    }
    #endif
}

// Matrix debugging functions for WebGL builds only
#ifdef WEB_BUILD
void debug_matrix(const char* label, const void* matrix_ptr) {
    // Cast to float array - the WebGL wrapper uses Matrix4f which is compatible
    const float* matrix = (const float*)matrix_ptr;
    matrix_debug_log("Matrix %s: [%.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f]\n",
                     label, matrix[0], matrix[1], matrix[2], matrix[3], matrix[4], matrix[5], matrix[6], matrix[7],
                     matrix[8], matrix[9], matrix[10], matrix[11], matrix[12], matrix[13], matrix[14], matrix[15]);
}

void debug_matrix_stack(const char* name, void* stack) {
    matrix_debug_log("Matrix Stack %s: (stack pointer: %p)\n", name, stack);
}
#endif

// Deterministic random number generator for reproducible comparisons
static unsigned long debug_random_seed_value = 1;

void debug_random_seed(unsigned int seed) {
    debug_random_seed_value = seed;
    matrix_debug_log("Debug random seed set to: %u\n", seed);
}

long debug_random(void) {
    // Simple linear congruential generator (same as many standard implementations)
    debug_random_seed_value = debug_random_seed_value * 1103515245 + 12345;
    return (debug_random_seed_value / 65536) % 32768;
}

double debug_frand(double f) {
    return f * (debug_random() / 32768.0);
}

// Function to read current OpenGL matrix state
void debug_current_opengl_matrix(const char* label) {
    #ifdef WEB_BUILD
    extern void glGetFloatv(GLenum pname, GLfloat *params);
    #endif
    GLfloat matrix[16];
    GLenum matrix_mode;

    // Determine which matrix to read based on current mode
    switch (current_matrix_mode) {
        case GL_MODELVIEW:
            matrix_mode = GL_MODELVIEW_MATRIX;
            break;
        case GL_PROJECTION:
            matrix_mode = GL_PROJECTION_MATRIX;
            break;
        case GL_TEXTURE:
            matrix_mode = GL_TEXTURE_MATRIX;
            break;
        default:
            matrix_mode = GL_MODELVIEW_MATRIX;
            break;
    }

    // Read the actual OpenGL matrix
    glGetFloatv(matrix_mode, matrix);

    // Display the matrix
    matrix_debug_log("=== %s Matrix ===\n", label);
    for (int i = 0; i < 4; i++) {
        matrix_debug_log("  [%.6f %.6f %.6f %.6f]\n",
           matrix[i*4], matrix[i*4+1], matrix[i*4+2], matrix[i*4+3]);
    }
    matrix_debug_log("================\n");
}

// Debug matrix operations
void debug_matrix_operation(const char* operation, const float* matrix) {
    matrix_debug_log("=== %s Matrix ===\n", operation);
    for (int i = 0; i < 4; i++) {
        matrix_debug_log("  [%.6f %.6f %.6f %.6f]\n",
           matrix[i*4], matrix[i*4+1], matrix[i*4+2], matrix[i*4+3]);
    }
    matrix_debug_log("================\n");
}

void debug_matrix_mode_switch(GLenum old_mode, GLenum new_mode) {
    const char* mode_names[] = {"UNKNOWN", "MODELVIEW", "PROJECTION", "TEXTURE"};
    matrix_debug_log("Matrix Mode Switch: %s -> %s\n",
       mode_names[old_mode - GL_MODELVIEW + 1],
       mode_names[new_mode - GL_MODELVIEW + 1]);
}

void debug_current_matrix_state(void) {
    matrix_debug_log("=== CURRENT MATRIX STATE ===\n");
    matrix_debug_log("Current Matrix Mode: %d\n", current_matrix_mode);
    debug_matrix_operation("ModelView", modelview_matrix);
    debug_matrix_operation("Projection", projection_matrix);
    matrix_debug_log("===========================\n");
}

// Initialize function pointers (call this once)
void init_matrix_debug_functions(void) {
    #ifdef WEB_BUILD
    printf("MATRIX_DEBUG: init_matrix_debug_functions() starting for WebGL\n");
    // For WebGL builds, point the real_ function pointers directly to the
    // existing WebGL wrapper functions in xscreensaver_web.c

    // Temporarily undefine the debug macros to get the real function pointers
    // This prevents circular references where real_* would point to debug_* functions
    #undef glMatrixMode
    #undef glLoadIdentity
    #undef glOrtho
    #undef glFrustum
    #undef glTranslatef
    #undef glRotatef
    #undef glScalef
    #undef glPushMatrix
    #undef glPopMatrix
    #undef glMultMatrixf
    #undef glMultMatrixd
    #undef glViewport
    #undef gluPerspective
    #undef gluLookAt
    #undef glTranslated

    // Forward declarations for the actual WebGL wrapper functions
    extern void glMatrixMode(GLenum mode);
    extern void glLoadIdentity(void);
    extern void glOrtho(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble near_val, GLdouble far_val);
    extern void glFrustum(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble near_val, GLdouble far_val);
    extern void glTranslatef(GLfloat x, GLfloat y, GLfloat z);
    extern void glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z);
    extern void glScalef(GLfloat x, GLfloat y, GLfloat z);
    extern void glPushMatrix(void);
    extern void glPopMatrix(void);
    extern void glMultMatrixf(const GLfloat* m);
    extern void glMultMatrixd(const GLdouble* m);
    extern void glViewport(GLint x, GLint y, GLsizei width, GLsizei height);
    extern void gluPerspective(GLdouble fovy, GLdouble aspect, GLdouble zNear, GLdouble zFar);
    extern void gluLookAt(GLdouble eyex, GLdouble eyey, GLdouble eyez, GLdouble centerx, GLdouble centery, GLdouble centerz, GLdouble upx, GLdouble upy, GLdouble upz);
    extern void glTranslated(GLdouble x, GLdouble y, GLdouble z);

    // Point real_ function pointers directly to the WebGL wrapper functions
    // This avoids an unnecessary layer of _web functions
    real_glMatrixMode = glMatrixMode;
    real_glLoadIdentity = glLoadIdentity;
    real_glOrtho = glOrtho;
    real_glFrustum = glFrustum;
    real_glTranslatef = glTranslatef;
    real_glRotatef = glRotatef;
    real_glScalef = glScalef;
    real_glPushMatrix = glPushMatrix;
    real_glPopMatrix = glPopMatrix;
    real_glMultMatrixf = glMultMatrixf;
    real_glMultMatrixd = glMultMatrixd;
    real_glViewport = glViewport;
    real_gluPerspective = gluPerspective;
    real_gluLookAt = gluLookAt;
    real_glTranslated = glTranslated;

    // Redefine the debug macros
    #define glMatrixMode debug_glMatrixMode
    #define glLoadIdentity debug_glLoadIdentity
    #define glOrtho debug_glOrtho
    #define glFrustum debug_glFrustum
    #define glTranslatef debug_glTranslatef
    #define glRotatef debug_glRotatef
    #define glScalef debug_glScalef
    #define glPushMatrix debug_glPushMatrix
    #define glPopMatrix debug_glPopMatrix
    #define glMultMatrixf debug_glMultMatrixf
    #define glMultMatrixd debug_glMultMatrixd
    #define glViewport debug_glViewport
    #define gluPerspective debug_gluPerspective
    #define gluLookAt debug_gluLookAt
    #define glTranslated debug_glTranslated

    // Log the assignments
    matrix_debug_log("[MDBG] Function pointers assigned:\n");
    matrix_debug_log("[MDBG]   real_glMatrixMode=%p\n", (void*) real_glMatrixMode);
    matrix_debug_log("[MDBG]   real_glLoadIdentity=%p\n", (void*) real_glLoadIdentity);
    matrix_debug_log("[MDBG]   real_glOrtho=%p\n", (void*) real_glOrtho);
    matrix_debug_log("[MDBG]   real_glFrustum=%p\n", (void*) real_glFrustum);
    matrix_debug_log("[MDBG]   real_glTranslatef=%p\n", (void*) real_glTranslatef);
    matrix_debug_log("[MDBG]   real_glRotatef=%p\n", (void*) real_glRotatef);
    matrix_debug_log("[MDBG]   real_glScalef=%p\n", (void*) real_glScalef);
    matrix_debug_log("[MDBG]   real_glPushMatrix=%p\n", (void*) real_glPushMatrix);
    matrix_debug_log("[MDBG]   real_glPopMatrix=%p\n", (void*) real_glPopMatrix);
    matrix_debug_log("[MDBG]   real_glMultMatrixf=%p\n", (void*) real_glMultMatrixf);
    matrix_debug_log("[MDBG]   real_glMultMatrixd=%p\n", (void*) real_glMultMatrixd);
    matrix_debug_log("[MDBG]   real_glViewport=%p\n", (void*) real_glViewport);
    matrix_debug_log("[MDBG]   real_gluPerspective=%p\n", (void*) real_gluPerspective);
    matrix_debug_log("[MDBG]   real_gluLookAt=%p\n", (void*) real_gluLookAt);
    matrix_debug_log("[MDBG]   real_glTranslated=%p\n", (void*) real_glTranslated);
    matrix_debug_log("Matrix debug: WebGL build - real_ pointers set to WebGL wrapper functions\n");
    #else
    // For native builds, use dlsym to get the real OpenGL functions
    // This is the standard approach for function interception on Linux
    #ifdef __linux__
    #include <dlfcn.h>

    // Try to get the real functions using RTLD_NEXT
    // This will find the next occurrence of these symbols in the library search order
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wpedantic"
    real_glMatrixMode = dlsym(RTLD_NEXT, "glMatrixMode");
    real_glLoadIdentity = dlsym(RTLD_NEXT, "glLoadIdentity");
    real_glOrtho = dlsym(RTLD_NEXT, "glOrtho");
    real_glFrustum = dlsym(RTLD_NEXT, "glFrustum");
    real_glTranslatef = dlsym(RTLD_NEXT, "glTranslatef");
    real_glRotatef = dlsym(RTLD_NEXT, "glRotatef");
    real_glScalef = dlsym(RTLD_NEXT, "glScalef");
    real_glPushMatrix = dlsym(RTLD_NEXT, "glPushMatrix");
    real_glPopMatrix = dlsym(RTLD_NEXT, "glPopMatrix");
    real_glMultMatrixf = dlsym(RTLD_NEXT, "glMultMatrixf");
    real_glViewport = dlsym(RTLD_NEXT, "glViewport");
    real_gluPerspective = dlsym(RTLD_NEXT, "gluPerspective");
    real_gluLookAt = dlsym(RTLD_NEXT, "gluLookAt");
    #pragma GCC diagnostic pop

    // If RTLD_NEXT didn't work, try RTLD_DEFAULT
    if (!real_glMatrixMode) {
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wpedantic"
        real_glMatrixMode = dlsym(RTLD_DEFAULT, "glMatrixMode");
        real_glLoadIdentity = dlsym(RTLD_DEFAULT, "glLoadIdentity");
        real_glOrtho = dlsym(RTLD_DEFAULT, "glOrtho");
        real_glFrustum = dlsym(RTLD_DEFAULT, "glFrustum");
        real_glTranslatef = dlsym(RTLD_DEFAULT, "glTranslatef");
        real_glRotatef = dlsym(RTLD_DEFAULT, "glRotatef");
        real_glScalef = dlsym(RTLD_DEFAULT, "glScalef");
        real_glPushMatrix = dlsym(RTLD_DEFAULT, "glPushMatrix");
        real_glPopMatrix = dlsym(RTLD_DEFAULT, "glPopMatrix");
        real_glMultMatrixf = dlsym(RTLD_DEFAULT, "glMultMatrixf");
        real_glViewport = dlsym(RTLD_DEFAULT, "glViewport");
        real_gluPerspective = dlsym(RTLD_DEFAULT, "gluPerspective");
        real_gluLookAt = dlsym(RTLD_DEFAULT, "gluLookAt");
        #pragma GCC diagnostic pop
    }

    // Log if we found the functions
    if (real_glMatrixMode) {
        matrix_debug_log("Matrix debug: Successfully initialized OpenGL function pointers\n");
    } else {
        matrix_debug_log("Matrix debug: WARNING - Failed to get OpenGL function pointers\n");
    }
    #endif // __linux__
    #endif // else WEB_BUILD

    // Initialize matrix validation for both native and WebGL builds
    matrix_debug_validate_init();

    #ifdef WEB_BUILD
    printf("MATRIX_DEBUG: init_matrix_debug_functions() completed for WebGL\n");
    #endif
}

// Ensure initialization happens even in WEB_BUILD, where AUTO_INIT_NATIVE is a no-op.
#ifdef MATRIX_DEBUG
// TEMPORARILY DISABLED: constructor might be causing hanging
// __attribute__((constructor))
// static void matrix_debug_constructor(void) {
//     init_matrix_debug_functions();
// }
#endif

// Wrapper function implementations
void debug_glMatrixMode(GLenum mode) {
    AUTO_INIT_NATIVE(real_glMatrixMode);

    GLenum old_mode = current_matrix_mode;
    current_matrix_mode = mode;
    debug_matrix_mode_switch(old_mode, mode);

    // Log the call and call real function (if available)
    matrix_debug_log("glMatrixMode(%d)\n", mode);

    // Call real function (works for both native and WebGL)
    if (real_glMatrixMode) {
        real_glMatrixMode(mode);
    }
}

void debug_glLoadIdentity(void) {
    AUTO_INIT_NATIVE(real_glLoadIdentity);

    matrix_debug_log("glLoadIdentity()\n");

    // Call real function (works for both native and WebGL)
    if (real_glLoadIdentity) {
        real_glLoadIdentity();
    }
}

void debug_glOrtho(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble near_val, GLdouble far_val) {
    AUTO_INIT_NATIVE(real_glOrtho);

    matrix_debug_log("glOrtho(%f, %f, %f, %f, %f, %f) - Matrix will be modified\n", left, right, bottom, top, near_val, far_val);

    // Call real function (works for both native and WebGL)
    if (real_glOrtho) {
        real_glOrtho(left, right, bottom, top, near_val, far_val);
    }
}

void debug_glFrustum(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble near_val, GLdouble far_val) {
    AUTO_INIT_NATIVE(real_glFrustum);

    matrix_debug_log("glFrustum(%f, %f, %f, %f, %f, %f)\n", left, right, bottom, top, near_val, far_val);

    // Show matrix state before frustum
    debug_current_opengl_matrix("Before Frustum");

    // Call real function (works for both native and WebGL)
    if (real_glFrustum) {
        real_glFrustum(left, right, bottom, top, near_val, far_val);
    }

    // Show matrix state after frustum
    debug_current_opengl_matrix("After Frustum");
}

void debug_glTranslatef(GLfloat x, GLfloat y, GLfloat z) {
    AUTO_INIT_NATIVE(real_glTranslatef);

    // Log the operation parameters
    matrix_debug_log("glTranslatef(%.3f, %.3f, %.3f)\n", x, y, z);

    // Show matrix state before transformation
    debug_current_opengl_matrix("Before Translate");

    // Call real function (works for both native and WebGL)
    if (real_glTranslatef) {
        real_glTranslatef(x, y, z);
    }

    // Show matrix state after transformation
    debug_current_opengl_matrix("After Translate");
}

void debug_glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z) {
    AUTO_INIT_NATIVE(real_glRotatef);

    // Log the operation parameters
    matrix_debug_log("glRotatef(%.3f degrees around %.3f, %.3f, %.3f)\n", angle, x, y, z);

    // Show matrix state before transformation
    debug_current_opengl_matrix("Before Rotate");

    // Call real function (works for both native and WebGL)
    if (real_glRotatef) {
        real_glRotatef(angle, x, y, z);
    }

    // Show matrix state after transformation
    debug_current_opengl_matrix("After Rotate");
}

void debug_glScalef(GLfloat x, GLfloat y, GLfloat z) {
    AUTO_INIT_NATIVE(real_glScalef);

    // Log the operation parameters
    matrix_debug_log("glScalef(%.3f, %.3f, %.3f)\n", x, y, z);

    // Show matrix state before transformation
    debug_current_opengl_matrix("Before Scale");

    // Call real function (works for both native and WebGL)
    if (real_glScalef) {
        real_glScalef(x, y, z);
    }

    // Show matrix state after transformation
    debug_current_opengl_matrix("After Scale");
}

void debug_glPushMatrix(void) {
    AUTO_INIT_NATIVE(real_glPushMatrix);

    matrix_debug_log("glPushMatrix()\n");

    // Show the matrix being pushed onto the stack
    debug_current_opengl_matrix("Matrix Being Pushed");

    // Call real function (works for both native and WebGL)
    if (real_glPushMatrix) {
        real_glPushMatrix();
    }

    matrix_debug_log("✓ Matrix pushed to stack\n");
}

void debug_glPopMatrix(void) {
    AUTO_INIT_NATIVE(real_glPopMatrix);

    matrix_debug_log("glPopMatrix()\n");

    // Show the current matrix before popping (what will be discarded)
    debug_current_opengl_matrix("Current Matrix (Before Pop)");

    // Call real function (works for both native and WebGL)
    if (real_glPopMatrix) {
        real_glPopMatrix();
    }

    // Show the matrix after popping (what was restored from stack)
    debug_current_opengl_matrix("Restored Matrix (After Pop)");
    matrix_debug_log("✓ Matrix popped from stack\n");
}

void debug_glMultMatrixf(const GLfloat* m) {
    AUTO_INIT_NATIVE(real_glMultMatrixf);

    matrix_debug_log("glMultMatrixf([%f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f])\n",
                     m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7],
                     m[8], m[9], m[10], m[11], m[12], m[13], m[14], m[15]);

    // Call real function (works for both native and WebGL)
    if (real_glMultMatrixf) {
        real_glMultMatrixf(m);
    }
}

// Additional functions used by hextrail
void debug_glViewport(GLint x, GLint y, GLsizei width, GLsizei height) {
    matrix_debug_log("glViewport(%d, %d, %d, %d)\n", x, y, width, height);

    // Call real function (works for both native and WebGL)
    if (real_glViewport) {
        real_glViewport(x, y, width, height);
    }
}

void debug_gluPerspective(GLdouble fovy, GLdouble aspect, GLdouble zNear, GLdouble zFar) {
    matrix_debug_log("gluPerspective(%f, %f, %f, %f)\n", fovy, aspect, zNear, zFar);

    // Call real function (works for both native and WebGL)
    if (real_gluPerspective) {
        real_gluPerspective(fovy, aspect, zNear, zFar);
    }
}

void debug_gluLookAt(GLdouble eyex, GLdouble eyey, GLdouble eyez,
                     GLdouble centerx, GLdouble centery, GLdouble centerz,
                     GLdouble upx, GLdouble upy, GLdouble upz) {
    matrix_debug_log("gluLookAt(eye=(%f, %f, %f), center=(%f, %f, %f), up=(%f, %f, %f))\n",
                     eyex, eyey, eyez, centerx, centery, centerz, upx, upy, upz);

    // Call real function (works for both native and WebGL)
    if (real_gluLookAt) {
        real_gluLookAt(eyex, eyey, eyez, centerx, centery, centerz, upx, upy, upz);
    }
}

// Additional debug functions for both native and web
void debug_glMultMatrixd(const GLdouble* m) {
    matrix_debug_log("glMultMatrixd - Input Matrix:\n");
    for (int i = 0; i < 4; i++) {
        matrix_debug_log("  [%.6f %.6f %.6f %.6f]\n",
           m[i*4], m[i*4+1], m[i*4+2], m[i*4+3]);
    }

    // Show matrix state before multiplication
    debug_current_opengl_matrix("Before MultMatrixd");

    // Call real function (works for both native and WebGL)
    if (real_glMultMatrixd) {
        real_glMultMatrixd(m);
    }

    // Show matrix state after multiplication
    debug_current_opengl_matrix("After MultMatrixd");
}

void debug_glTranslated(GLdouble x, GLdouble y, GLdouble z) {
    matrix_debug_log("glTranslated(%f, %f, %f)\n", x, y, z);

    // Call real function (works for both native and WebGL)
    if (real_glTranslated) {
        real_glTranslated(x, y, z);
    }
}

// Reference matrix state for validation
static float reference_modelview[16];
static float reference_projection[16];
static float reference_texture[16];
static GLenum reference_matrix_mode = GL_MODELVIEW;

// Get current reference matrix based on mode
static float* get_reference_matrix(void) {
    switch (reference_matrix_mode) {
        case GL_MODELVIEW: return reference_modelview;
        case GL_PROJECTION: return reference_projection;
        case GL_TEXTURE: return reference_texture;
        default: return reference_modelview;
    }
}

// Reference matrix math functions
void reference_matrix_identity(float* m) {
    m[0] = 1.0f; m[1] = 0.0f; m[2] = 0.0f; m[3] = 0.0f;
    m[4] = 0.0f; m[5] = 1.0f; m[6] = 0.0f; m[7] = 0.0f;
    m[8] = 0.0f; m[9] = 0.0f; m[10] = 1.0f; m[11] = 0.0f;
    m[12] = 0.0f; m[13] = 0.0f; m[14] = 0.0f; m[15] = 1.0f;
}

void reference_matrix_multiply(float* result, const float* a, const float* b) {
    float temp[16];
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            temp[i*4 + j] = 0.0f;
            for (int k = 0; k < 4; k++) {
                temp[i*4 + j] += a[i*4 + k] * b[k*4 + j];
            }
        }
    }
    memcpy(result, temp, 16 * sizeof(float));
}

void reference_matrix_translate(float* m, float x, float y, float z) {
    float translate[16];
    reference_matrix_identity(translate);
    translate[12] = x;
    translate[13] = y;
    translate[14] = z;
    reference_matrix_multiply(m, m, translate);
}

void reference_matrix_scale(float* m, float x, float y, float z) {
    float scale[16];
    reference_matrix_identity(scale);
    scale[0] = x;
    scale[5] = y;
    scale[10] = z;
    reference_matrix_multiply(m, m, scale);
}

void reference_matrix_rotate(float* m, float angle, float x, float y, float z) {
    // Convert angle to radians
    float rad = angle * M_PI / 180.0f;
    float c = cosf(rad);
    float s = sinf(rad);

    // Normalize the axis
    float len = sqrtf(x*x + y*y + z*z);
    if (len == 0.0f) return; // Invalid axis
    x /= len; y /= len; z /= len;

    // Build rotation matrix
    float rotate[16];
    rotate[0] = x*x*(1-c) + c;     rotate[1] = x*y*(1-c) - z*s;   rotate[2] = x*z*(1-c) + y*s;   rotate[3] = 0;
    rotate[4] = y*x*(1-c) + z*s;   rotate[5] = y*y*(1-c) + c;     rotate[6] = y*z*(1-c) - x*s;   rotate[7] = 0;
    rotate[8] = z*x*(1-c) - y*s;   rotate[9] = z*y*(1-c) + x*s;   rotate[10] = z*z*(1-c) + c;    rotate[11] = 0;
    rotate[12] = 0;                rotate[13] = 0;                rotate[14] = 0;                rotate[15] = 1;

    reference_matrix_multiply(m, m, rotate);
}

// Matrix comparison with tolerance for floating point errors
int compare_matrices(const char* operation, const float* reference, const float* actual) {
    const float tolerance = 1e-5f;
    int differences = 0;

    matrix_debug_log("=== Matrix Validation: %s ===\n", operation);

    for (int i = 0; i < 16; i++) {
        float diff = fabsf(reference[i] - actual[i]);
        if (diff > tolerance) {
            differences++;
            matrix_debug_log("  [%d]: REF=%.6f ACT=%.6f DIFF=%.6f ❌\n", i, reference[i], actual[i], diff);
        }
    }

    if (differences == 0) {
        matrix_debug_log("✅ Matrix validation PASSED for %s\n", operation);
    } else {
        matrix_debug_log("❌ Matrix validation FAILED for %s (%d differences)\n", operation, differences);
    }

    return differences;
}

void matrix_debug_validate_init(void) {
    // Initialize reference matrices to identity
    reference_matrix_identity(reference_modelview);
    reference_matrix_identity(reference_projection);
    reference_matrix_identity(reference_texture);
    reference_matrix_mode = GL_MODELVIEW;

    // Set deterministic seed for reproducible comparisons
    debug_random_seed(12345);

    matrix_debug_log("Matrix validation initialized with deterministic random seed\n");
}

#endif // MATRIX_DEBUG
