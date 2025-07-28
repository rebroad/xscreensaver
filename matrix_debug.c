#include "matrix_debug.h"
#include <stdarg.h>
#include <string.h>
#include <math.h>

#ifdef MATRIX_DEBUG

// Matrix state tracking
GLenum current_matrix_mode = GL_MODELVIEW;
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

// Matrix stack for push/pop operations
#define MAX_MATRIX_STACK_DEPTH 32
float modelview_stack[MAX_MATRIX_STACK_DEPTH][16];
float projection_stack[MAX_MATRIX_STACK_DEPTH][16];
int modelview_stack_top = 0;
int projection_stack_top = 0;

// Debug logging function
void matrix_debug_log(const char* format, ...) {
    va_list args;
    va_start(args, format);
    printf("[MATRIX_DEBUG] ");
    vprintf(format, args);
    va_end(args);
}

// Matrix utility functions
void matrix_identity(float* m) {
    m[0] = 1.0f; m[1] = 0.0f; m[2] = 0.0f; m[3] = 0.0f;
    m[4] = 0.0f; m[5] = 1.0f; m[6] = 0.0f; m[7] = 0.0f;
    m[8] = 0.0f; m[9] = 0.0f; m[10] = 1.0f; m[11] = 0.0f;
    m[12] = 0.0f; m[13] = 0.0f; m[14] = 0.0f; m[15] = 1.0f;
}

void matrix_multiply(float* result, const float* a, const float* b) {
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

void matrix_translate(float* m, float x, float y, float z) {
    float translate[16];
    matrix_identity(translate);
    translate[12] = x;
    translate[13] = y;
    translate[14] = z;
    matrix_multiply(m, m, translate);
}

void matrix_rotate(float* m, float angle, float x, float y, float z) {
    // Simplified rotation - in practice you'd want a full rotation matrix
    // For debugging purposes, we'll just log the rotation
    matrix_debug_log("Rotation: %.3f degrees around (%.3f, %.3f, %.3f)\n", angle, x, y, z);
}

void matrix_scale(float* m, float x, float y, float z) {
    float scale[16];
    matrix_identity(scale);
    scale[0] = x;
    scale[5] = y;
    scale[10] = z;
    matrix_multiply(m, m, scale);
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

// Function pointers for native OpenGL functions
#ifndef WEB_BUILD
static void (*real_glMatrixMode)(GLenum) = NULL;
static void (*real_glLoadIdentity)(void) = NULL;
static void (*real_glOrtho)(GLdouble, GLdouble, GLdouble, GLdouble, GLdouble, GLdouble) = NULL;
static void (*real_glFrustum)(GLdouble, GLdouble, GLdouble, GLdouble, GLdouble, GLdouble) = NULL;
static void (*real_glTranslatef)(GLfloat, GLfloat, GLfloat) = NULL;
static void (*real_glRotatef)(GLfloat, GLfloat, GLfloat, GLfloat) = NULL;
static void (*real_glScalef)(GLfloat, GLfloat, GLfloat) = NULL;
static void (*real_glPushMatrix)(void) = NULL;
static void (*real_glPopMatrix)(void) = NULL;
static void (*real_glMultMatrixf)(const float*) = NULL;

// Initialize function pointers (call this once)
void init_matrix_debug_functions(void) {
    // For now, we'll use dlsym to get the real functions
    // This requires linking with -ldl
    #ifdef __linux__
    #include <dlfcn.h>
    void* handle = RTLD_DEFAULT;
    real_glMatrixMode = dlsym(handle, "glMatrixMode");
    real_glLoadIdentity = dlsym(handle, "glLoadIdentity");
    real_glOrtho = dlsym(handle, "glOrtho");
    real_glFrustum = dlsym(handle, "glFrustum");
    real_glTranslatef = dlsym(handle, "glTranslatef");
    real_glRotatef = dlsym(handle, "glRotatef");
    real_glScalef = dlsym(handle, "glScalef");
    real_glPushMatrix = dlsym(handle, "glPushMatrix");
    real_glPopMatrix = dlsym(handle, "glPopMatrix");
    real_glMultMatrixf = dlsym(handle, "glMultMatrixf");
    #endif
}
#endif

// Wrapper function implementations
void debug_glMatrixMode(GLenum mode) {
    GLenum old_mode = current_matrix_mode;
    current_matrix_mode = mode;
    debug_matrix_mode_switch(old_mode, mode);
    
    // For both native and WebGL builds, we only log and call real functions
    // The actual matrix work is done by the calling code (xscreensaver_web.c or real OpenGL)
    #ifdef WEB_BUILD
        // WebGL version: only log, don't do matrix work
        // xscreensaver_web.c will handle the actual matrix operations
    #else
        // Native version: call real OpenGL function
        if (real_glMatrixMode) {
            real_glMatrixMode(mode);
        }
    #endif
}

void debug_glLoadIdentity(void) {
    // For both native and WebGL builds, we only log
    // The actual matrix work is done by the calling code
    matrix_debug_log("glLoadIdentity()\n");
    
    #ifdef WEB_BUILD
        // WebGL version: only log, don't do matrix work
        // xscreensaver_web.c will handle the actual matrix operations
    #else
        // Native version: call real OpenGL function
        if (real_glLoadIdentity) {
            real_glLoadIdentity();
        }
    #endif
}

void debug_glOrtho(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble near_val, GLdouble far_val) {
    matrix_debug_log("glOrtho: %.3f, %.3f, %.3f, %.3f, %.3f, %.3f\n", 
       left, right, bottom, top, near_val, far_val);
    
    #ifdef WEB_BUILD
        // WebGL version: only log, don't do matrix work
        // xscreensaver_web.c will handle the actual matrix operations
    #else
        // Native version: call real OpenGL function
        if (real_glOrtho) {
            real_glOrtho(left, right, bottom, top, near_val, far_val);
        }
    #endif
}

void debug_glFrustum(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble near_val, GLdouble far_val) {
    matrix_debug_log("glFrustum: left=%.3f, right=%.3f, bottom=%.3f, top=%.3f, near=%.3f, far=%.3f\n", 
       left, right, bottom, top, near_val, far_val);
    
    #ifdef WEB_BUILD
        // WebGL version: only log, don't do matrix work
        // xscreensaver_web.c will handle the actual matrix operations
    #else
        // Native version: call real OpenGL function
        if (real_glFrustum) {
            real_glFrustum(left, right, bottom, top, near_val, far_val);
        }
    #endif
}

void debug_glTranslatef(GLfloat x, GLfloat y, GLfloat z) {
    matrix_debug_log("glTranslatef: (%.3f, %.3f, %.3f)\n", x, y, z);
    
    #ifdef WEB_BUILD
        // WebGL version: only log, don't do matrix work
        // xscreensaver_web.c will handle the actual matrix operations
    #else
        // Native version: call real OpenGL function
        if (real_glTranslatef) {
            real_glTranslatef(x, y, z);
        }
    #endif
}

void debug_glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z) {
    matrix_debug_log("glRotatef: %.3f degrees around (%.3f, %.3f, %.3f)\n", angle, x, y, z);
    
    #ifdef WEB_BUILD
        // WebGL version: only log, don't do matrix work
        // xscreensaver_web.c will handle the actual matrix operations
    #else
        // Native version: call real OpenGL function
        if (real_glRotatef) {
            real_glRotatef(angle, x, y, z);
        }
    #endif
}

void debug_glScalef(GLfloat x, GLfloat y, GLfloat z) {
    matrix_debug_log("glScalef: (%.3f, %.3f, %.3f)\n", x, y, z);
    
    #ifdef WEB_BUILD
        // WebGL version: only log, don't do matrix work
        // xscreensaver_web.c will handle the actual matrix operations
    #else
        // Native version: call real OpenGL function
        if (real_glScalef) {
            real_glScalef(x, y, z);
        }
    #endif
}

void debug_glPushMatrix(void) {
    matrix_debug_log("glPushMatrix()\n");
    
    #ifdef WEB_BUILD
        // WebGL version: only log, don't do matrix work
        // xscreensaver_web.c will handle the actual matrix operations
    #else
        // Native version: call real OpenGL function
        if (real_glPushMatrix) {
            real_glPushMatrix();
        }
    #endif
}

void debug_glPopMatrix(void) {
    matrix_debug_log("glPopMatrix()\n");
    
    #ifdef WEB_BUILD
        // WebGL version: only log, don't do matrix work
        // xscreensaver_web.c will handle the actual matrix operations
    #else
        // Native version: call real OpenGL function
        if (real_glPopMatrix) {
            real_glPopMatrix();
        }
    #endif
}

void debug_glMultMatrixf(const float* m) {
    matrix_debug_log("glMultMatrixf()\n");
    
    #ifdef WEB_BUILD
        // WebGL version: only log, don't do matrix work
        // xscreensaver_web.c will handle the actual matrix operations
    #else
        // Native version: call real OpenGL function
        if (real_glMultMatrixf) {
            real_glMultMatrixf(m);
        }
    #endif
}

#endif // MATRIX_DEBUG 