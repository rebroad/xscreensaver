#include <stdio.h>
#include <stdlib.h>
#include <GL/gl.h>

// Include our matrix debug framework
#include "matrix_debug.h"

// Test program that simulates hextrail matrix operations
int main() {
    printf("=== Matrix Test Program ===\n");
    printf("This simulates the matrix operations used by hextrail\n\n");

    // Enable matrix debugging
    #ifdef MATRIX_DEBUG
        printf("Matrix debugging enabled\n\n");
        #ifndef WEB_BUILD
        // Initialize function pointers for native builds
        init_matrix_debug_functions();
        #endif
    #else
        printf("Matrix debugging disabled - compile with -DMATRIX_DEBUG to enable\n\n");
    #endif

    // Simulate typical hextrail matrix setup
    printf("1. Setting up projection matrix (orthographic)\n");
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-1.0, 1.0, -1.0, 1.0, -1.0, 1.0);

    printf("\n2. Setting up modelview matrix\n");
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    printf("\n3. Applying some transformations\n");
    glTranslatef(0.0, 0.0, -5.0);  // Move back
    glRotatef(45.0, 1.0, 0.0, 0.0); // Rotate around X axis
    glScalef(0.5, 0.5, 0.5);       // Scale down

    printf("\n4. Testing perspective projection\n");
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(-1.0, 1.0, -1.0, 1.0, 1.0, 100.0);

    printf("\n5. Final matrix state\n");
    debug_current_matrix_state();

    printf("\n=== Test Complete ===\n");
    return 0;
}
