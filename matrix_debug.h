#ifndef MATRIX_DEBUG_H
#define MATRIX_DEBUG_H

// Centralized random seed value for consistent debugging
#define MATRIX_DEBUG_SEED 12345

// Function declarations
void init_matrix_debug_consistency(void);
long matrix_debug_random(void);
double matrix_debug_frand(double max);

#include <GL/gl.h>
#ifndef WEB_BUILD
#include <GL/glu.h>
#endif
#include <stdio.h>

// Matrix debugging infrastructure that works for both native and WebGL builds
#ifdef MATRIX_DEBUG

// Debug logging function
void matrix_debug_log(const char* format, ...);

// Frame counter function
void matrix_debug_next_frame(void);

// Matrix validation mode (define this to enable reference math validation)
#define MATRIX_DEBUG_VALIDATE

// Deterministic random functions for reproducible comparisons
long debug_random(void);
double debug_frand(double f);
void debug_random_seed(unsigned int seed);

// Matrix debugging functions for WebGL builds only
#ifdef WEB_BUILD
void debug_matrix(const char* label, const void* matrix);
void debug_matrix_stack(const char* name, void* stack);
#endif

// Matrix utility functions are handled by xscreensaver_web.c for WebGL builds
// No additional matrix utilities needed in matrix_debug.c

// Matrix state tracking
#ifndef WEB_BUILD
extern GLenum current_matrix_mode;
#endif
extern float modelview_matrix[16];
extern float projection_matrix[16];

// Debug matrix operations
void debug_matrix_mode_switch(GLenum old_mode, GLenum new_mode);
void debug_matrix_operation(const char* operation, const float* matrix);
void debug_current_matrix_state(void);
void debug_current_opengl_matrix(const char* label);

// Initialize function pointers (for native builds)
#ifndef WEB_BUILD
void init_matrix_debug_functions(void);
#endif

// Debug wrapper function declarations
void debug_glMatrixMode(GLenum mode);
void debug_glLoadIdentity(void);
void debug_glOrtho(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble near_val, GLdouble far_val);
void debug_glFrustum(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble near_val, GLdouble far_val);
void debug_glTranslatef(GLfloat x, GLfloat y, GLfloat z);
void debug_glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z);
void debug_glScalef(GLfloat x, GLfloat y, GLfloat z);
void debug_glPushMatrix(void);
void debug_glPopMatrix(void);
void debug_glMultMatrixf(const GLfloat* m);

// Additional functions used by hextrail
void debug_glViewport(GLint x, GLint y, GLsizei width, GLsizei height);
// Type-agnostic debug wrappers that work for both native and web
void debug_gluPerspective(GLdouble fovy, GLdouble aspect, GLdouble zNear, GLdouble zFar);
void debug_gluLookAt(GLdouble eyex, GLdouble eyey, GLdouble eyez,
                     GLdouble centerx, GLdouble centery, GLdouble centerz,
                     GLdouble upx, GLdouble upy, GLdouble upz);

// Additional debug functions for both native and web
void debug_glMultMatrixd(const GLdouble* m);
void debug_glTranslated(GLdouble x, GLdouble y, GLdouble z);

// Macro to replace OpenGL calls with debug versions
// Define these for ALL builds (both native and WebGL)
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

// Additional macros for hextrail functions
#define glViewport debug_glViewport

// Unified debug macros that work for both native and web
#define gluPerspective debug_gluPerspective
#define gluLookAt debug_gluLookAt
#define glMultMatrixd debug_glMultMatrixd
#define glTranslated debug_glTranslated

// Deterministic random function macros for reproducible comparisons
#define random matrix_debug_random
#define frand matrix_debug_frand

#ifdef MATRIX_DEBUG_VALIDATE
// Reference matrix math functions for validation
void reference_matrix_identity(float* m);
void reference_matrix_translate(float* m, float x, float y, float z);
void reference_matrix_rotate(float* m, float angle, float x, float y, float z);
void reference_matrix_scale(float* m, float x, float y, float z);
void reference_matrix_multiply(float* result, const float* a, const float* b);
void reference_matrix_frustum(float* m, double left, double right, double bottom, double top, double near_val, double far_val);
void reference_matrix_perspective(float* m, double fovy, double aspect, double zNear, double zFar);
void reference_matrix_lookat(float* m, double eyex, double eyey, double eyez, double centerx, double centery, double centerz, double upx, double upy, double upz);

// Matrix comparison functions
int compare_matrices(const char* operation, const float* reference, const float* actual);
void matrix_debug_validate_init(void);
#endif // MATRIX_DEBUG_VALIDATE

#endif // MATRIX_DEBUG

#endif // MATRIX_DEBUG_H 