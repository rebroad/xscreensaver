#include "matrix_debug.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

// Macro to handle auto-initialization for native builds
#ifdef WEB_BUILD
    // WebGL version: no-op (no function pointers needed)
    #define AUTO_INIT_NATIVE(func_ptr) /* No-op when WEB_BUILD is defined */
#else
    // Native version: check and initialize function pointers
    #define AUTO_INIT_NATIVE(func_ptr) \
        do { \
            if (!func_ptr) { \
                init_matrix_debug_functions(); \
            } \
        } while(0)
#endif

// Global variables for matrix state tracking (matching header declarations)
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
float texture_matrix[16] = {
    1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f
};

// Matrix stack for push/pop operations
#define MAX_MATRIX_STACK_DEPTH 32
static struct {
    GLenum mode;
    float matrix[16];
} matrix_stack[MAX_MATRIX_STACK_DEPTH];
static int matrix_stack_depth = 0;

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
static void (*real_glViewport)(GLint, GLint, GLsizei, GLsizei) = NULL;
static void (*real_gluPerspective)(GLdouble, GLdouble, GLdouble, GLdouble) = NULL;
static void (*real_gluLookAt)(GLdouble, GLdouble, GLdouble, GLdouble, GLdouble, GLdouble, GLdouble, GLdouble, GLdouble) = NULL;

// Debug logging function
void matrix_debug_log(const char* format, ...) {
    #ifdef MATRIX_DEBUG
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    #endif
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

// Initialize function pointers (call this once)
void init_matrix_debug_functions(void) {
    #ifdef WEB_BUILD
    // For WebGL builds, get pointers to the real web wrapper functions
    // These are the actual implementations in xscreensaver_web.c
    extern void glMatrixMode(GLenum);
    extern void glLoadIdentity(void);
    extern void glOrtho(GLdouble, GLdouble, GLdouble, GLdouble, GLdouble, GLdouble);
    extern void glFrustum(GLdouble, GLdouble, GLdouble, GLdouble, GLdouble, GLdouble);
    extern void glTranslatef(GLfloat, GLfloat, GLfloat);
    extern void glRotatef(GLfloat, GLfloat, GLfloat, GLfloat);
    extern void glScalef(GLfloat, GLfloat, GLfloat);
    extern void glPushMatrix(void);
    extern void glPopMatrix(void);
    extern void glMultMatrixf(const GLfloat*);
    extern void glViewport(GLint, GLint, GLsizei, GLsizei);
    extern void gluPerspective(GLdouble, GLdouble, GLdouble, GLdouble);
    extern void gluLookAt(GLdouble, GLdouble, GLdouble, GLdouble, GLdouble, GLdouble, GLdouble, GLdouble, GLdouble);

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
    real_glViewport = glViewport;
    real_gluPerspective = gluPerspective;
    real_gluLookAt = gluLookAt;

    matrix_debug_log("Matrix debug: Successfully initialized WebGL function pointers\n");
    #else
    // For now, we'll use dlsym to get the real functions
    // This requires linking with -ldl
    #ifdef __linux__
    #include <dlfcn.h>

    // Try to get the real functions using RTLD_NEXT
    // This will find the next occurrence of these symbols in the library search order
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

    // If RTLD_NEXT didn't work, try RTLD_DEFAULT
    if (!real_glMatrixMode) {
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
    }

    // Log if we found the functions
    if (real_glMatrixMode) {
        matrix_debug_log("Matrix debug: Successfully initialized OpenGL function pointers\n");
    } else {
        matrix_debug_log("Matrix debug: WARNING - Failed to get OpenGL function pointers\n");
    }
    #endif // __linux__
    #endif // else WEB_BUILD
}

// Wrapper function implementations
void debug_glMatrixMode(GLenum mode) {
    AUTO_INIT_NATIVE(real_glMatrixMode);

    GLenum old_mode = current_matrix_mode;
    current_matrix_mode = mode;
    debug_matrix_mode_switch(old_mode, mode);

    // Log the call and call real function (if available)
    matrix_debug_log("glMatrixMode(%d)\n", mode);

    #ifdef WEB_BUILD
        // WebGL version: call the real web wrapper function
        extern void glMatrixMode(GLenum);
        glMatrixMode(mode);
    #else
        // Native version: call real OpenGL function
        if (real_glMatrixMode) {
            real_glMatrixMode(mode);
        }
    #endif
}

void debug_glLoadIdentity(void) {
    AUTO_INIT_NATIVE(real_glLoadIdentity);

    matrix_debug_log("glLoadIdentity()\n");

    #ifdef WEB_BUILD
        // WebGL version: call the real web wrapper function
        extern void glLoadIdentity(void);
        glLoadIdentity();
    #else
        // Native version: call real OpenGL function
        if (real_glLoadIdentity) {
            real_glLoadIdentity();
        }
    #endif
}

void debug_glOrtho(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble near_val, GLdouble far_val) {
    AUTO_INIT_NATIVE(real_glOrtho);

    matrix_debug_log("glOrtho(%f, %f, %f, %f, %f, %f)\n", left, right, bottom, top, near_val, far_val);

    #ifdef WEB_BUILD
        // WebGL version: call the real web wrapper function
        extern void glOrtho(GLdouble, GLdouble, GLdouble, GLdouble, GLdouble, GLdouble);
        glOrtho(left, right, bottom, top, near_val, far_val);
    #else
        // Native version: call real OpenGL function
        if (real_glOrtho) {
            real_glOrtho(left, right, bottom, top, near_val, far_val);
        }
    #endif
}

void debug_glFrustum(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble near_val, GLdouble far_val) {
    AUTO_INIT_NATIVE(real_glFrustum);

    matrix_debug_log("glFrustum(%f, %f, %f, %f, %f, %f)\n", left, right, bottom, top, near_val, far_val);

    #ifdef WEB_BUILD
        // WebGL version: call the real web wrapper function
        extern void glFrustum(GLdouble, GLdouble, GLdouble, GLdouble, GLdouble, GLdouble);
        glFrustum(left, right, bottom, top, near_val, far_val);
    #else
        // Native version: call real OpenGL function
        if (real_glFrustum) {
            real_glFrustum(left, right, bottom, top, near_val, far_val);
        }
    #endif
}

void debug_glTranslatef(GLfloat x, GLfloat y, GLfloat z) {
    AUTO_INIT_NATIVE(real_glTranslatef);

    matrix_debug_log("glTranslatef(%f, %f, %f)\n", x, y, z);

    #ifdef WEB_BUILD
        // WebGL version: call the real web wrapper function
        extern void glTranslatef(GLfloat, GLfloat, GLfloat);
        glTranslatef(x, y, z);
    #else
        // Native version: call real OpenGL function
        if (real_glTranslatef) {
            real_glTranslatef(x, y, z);
        }
    #endif
}

void debug_glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z) {
    AUTO_INIT_NATIVE(real_glRotatef);

    matrix_debug_log("glRotatef(%f, %f, %f, %f)\n", angle, x, y, z);

    #ifdef WEB_BUILD
        // WebGL version: call the real web wrapper function
        extern void glRotatef(GLfloat, GLfloat, GLfloat, GLfloat);
        glRotatef(angle, x, y, z);
    #else
        // Native version: call real OpenGL function
        if (real_glRotatef) {
            real_glRotatef(angle, x, y, z);
        }
    #endif
}

void debug_glScalef(GLfloat x, GLfloat y, GLfloat z) {
    AUTO_INIT_NATIVE(real_glScalef);

    matrix_debug_log("glScalef(%f, %f, %f)\n", x, y, z);

    #ifdef WEB_BUILD
        // WebGL version: call the real web wrapper function
        extern void glScalef(GLfloat, GLfloat, GLfloat);
        glScalef(x, y, z);
    #else
        // Native version: call real OpenGL function
        if (real_glScalef) {
            real_glScalef(x, y, z);
        }
    #endif
}

void debug_glPushMatrix(void) {
    AUTO_INIT_NATIVE(real_glPushMatrix);

    matrix_debug_log("glPushMatrix()\n");

    #ifdef WEB_BUILD
        // WebGL version: call the real web wrapper function
        extern void glPushMatrix(void);
        glPushMatrix();
    #else
        // Native version: call real OpenGL function
        if (real_glPushMatrix) {
            real_glPushMatrix();
        }
    #endif
}

void debug_glPopMatrix(void) {
    AUTO_INIT_NATIVE(real_glPopMatrix);

    matrix_debug_log("glPopMatrix()\n");

    #ifdef WEB_BUILD
        // WebGL version: call the real web wrapper function
        extern void glPopMatrix(void);
        glPopMatrix();
    #else
        // Native version: call real OpenGL function
        if (real_glPopMatrix) {
            real_glPopMatrix();
        }
    #endif
}

void debug_glMultMatrixf(const GLfloat* m) {
    AUTO_INIT_NATIVE(real_glMultMatrixf);

    matrix_debug_log("glMultMatrixf([%f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f])\n",
                     m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7],
                     m[8], m[9], m[10], m[11], m[12], m[13], m[14], m[15]);

    #ifdef WEB_BUILD
        // WebGL version: call the real web wrapper function
        extern void glMultMatrixf(const GLfloat*);
        glMultMatrixf(m);
    #else
        // Native version: call real OpenGL function
        if (real_glMultMatrixf) {
            real_glMultMatrixf(m);
        }
    #endif
}

// Additional functions used by hextrail
void debug_glViewport(GLint x, GLint y, GLsizei width, GLsizei height) {
    matrix_debug_log("glViewport(%d, %d, %d, %d)\n", x, y, width, height);

    #ifdef WEB_BUILD
        // WebGL version: call the real web wrapper function
        extern void glViewport(GLint, GLint, GLsizei, GLsizei);
        glViewport(x, y, width, height);
    #else
        // For native builds, we need to get the real function pointer
        static void (*real_glViewport)(GLint, GLint, GLsizei, GLsizei) = NULL;
        if (!real_glViewport) {
            #ifdef __linux__
            #include <dlfcn.h>
            real_glViewport = dlsym(RTLD_NEXT, "glViewport");
            if (!real_glViewport) {
                real_glViewport = dlsym(RTLD_DEFAULT, "glViewport");
            }
            #endif
        }

        if (real_glViewport) {
            real_glViewport(x, y, width, height);
        }
    #endif
}

void debug_gluPerspective(GLdouble fovy, GLdouble aspect, GLdouble zNear, GLdouble zFar) {
    matrix_debug_log("gluPerspective(%f, %f, %f, %f)\n", fovy, aspect, zNear, zFar);

    #ifdef WEB_BUILD
        // WebGL version: call the real web wrapper function
        extern void gluPerspective(GLdouble, GLdouble, GLdouble, GLdouble);
        gluPerspective(fovy, aspect, zNear, zFar);
    #else
        // For native builds, we need to get the real function pointer
        static void (*real_gluPerspective)(GLdouble, GLdouble, GLdouble, GLdouble) = NULL;
        if (!real_gluPerspective) {
            #ifdef __linux__
            #include <dlfcn.h>
            real_gluPerspective = dlsym(RTLD_NEXT, "gluPerspective");
            if (!real_gluPerspective) {
                real_gluPerspective = dlsym(RTLD_DEFAULT, "gluPerspective");
            }
            #endif
        }

        if (real_gluPerspective) {
            real_gluPerspective(fovy, aspect, zNear, zFar);
        }
    #endif
}

void debug_gluLookAt(GLdouble eyex, GLdouble eyey, GLdouble eyez,
                     GLdouble centerx, GLdouble centery, GLdouble centerz,
                     GLdouble upx, GLdouble upy, GLdouble upz) {
    matrix_debug_log("gluLookAt(eye=(%f, %f, %f), center=(%f, %f, %f), up=(%f, %f, %f))\n",
                     eyex, eyey, eyez, centerx, centery, centerz, upx, upy, upz);

    #ifdef WEB_BUILD
        // WebGL version: call the real web wrapper function
        extern void gluLookAt(GLdouble, GLdouble, GLdouble, GLdouble, GLdouble, GLdouble, GLdouble, GLdouble, GLdouble);
        gluLookAt(eyex, eyey, eyez, centerx, centery, centerz, upx, upy, upz);
    #else
        // For native builds, we need to get the real function pointer
        static void (*real_gluLookAt)(GLdouble, GLdouble, GLdouble, GLdouble, GLdouble, GLdouble, GLdouble, GLdouble, GLdouble) = NULL;
        if (!real_gluLookAt) {
            #ifdef __linux__
            #include <dlfcn.h>
            real_gluLookAt = dlsym(RTLD_NEXT, "gluLookAt");
            if (!real_gluLookAt) {
                real_gluLookAt = dlsym(RTLD_DEFAULT, "gluLookAt");
            }
            #endif
        }

        if (real_gluLookAt) {
            real_gluLookAt(eyex, eyey, eyez, centerx, centery, centerz, upx, upy, upz);
        }
    #endif
}
