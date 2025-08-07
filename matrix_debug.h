#ifndef MATRIX_DEBUG_H
#define MATRIX_DEBUG_H

#include <GL/gl.h>
#include <stdio.h>

// Matrix debugging infrastructure that works for both native and WebGL builds
#ifdef MATRIX_DEBUG

// Debug logging function
void matrix_debug_log(const char* format, ...);

// Frame counter function
void matrix_debug_next_frame(void);

// Matrix debugging functions for WebGL builds only
#ifdef WEB_BUILD
void debug_matrix(const char* label, const void* matrix);
void debug_matrix_stack(const char* name, void* stack);
#endif

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

// Initialize function pointers (for native builds)
#ifndef WEB_BUILD
void init_matrix_debug_functions(void);
#endif

// Debug wrapper function declarations
void debug_glMatrixMode(GLenum mode);
void debug_glLoadIdentity(void);
void debug_glOrtho(GLfloat left, GLfloat right, GLfloat bottom, GLfloat top, GLfloat near_val, GLfloat far_val);
void debug_glFrustum(GLfloat left, GLfloat right, GLfloat bottom, GLfloat top, GLfloat near_val, GLfloat far_val);
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
// Only define these for native builds (not WebGL to avoid duplicate symbols)
#ifndef WEB_BUILD
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
#endif // !WEB_BUILD

#endif // MATRIX_DEBUG

#endif // MATRIX_DEBUG_H 