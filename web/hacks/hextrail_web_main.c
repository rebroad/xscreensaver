/* hextrail_web_main.c - Web Entry Point for HexTrail
 *
 * This provides the web-specific code needed to run hextrail
 * using the generic xscreensaver_web wrapper.
 */

#include <emscripten.h>
#define WEB_HEXTRAIL_PRIVATE_RESOURCES 1
#include "../xscreensaver_web.c"

// Include our web headers (WEB_BUILD already defined by build script)
#include "../xlockmore.h"

// Enable matrix debug wrappers for the hack when requested
#ifdef MATRIX_DEBUG
#include "../matrix_debug.h"
#endif

// Now include hextrail.c with our web headers already defined
#include "../../hacks/glx/hextrail.c"

EMSCRIPTEN_KEEPALIVE
void set_speed(GLfloat new_speed) {
    speed = new_speed;
    DL(1, "Animation speed set to: %f\n", speed);
}

EMSCRIPTEN_KEEPALIVE
void set_thickness(GLfloat new_thickness) {
    DL(1, "DEBUG: set_thickness called with %.3f, current thickness=%.3f\n",
       new_thickness, thickness);
    thickness = new_thickness;
    DL(1, "Thickness set to: %f\n", thickness);
}

EMSCRIPTEN_KEEPALIVE
void set_spin(int new_spin_enabled) {
    extern void update_hextrail_rotator(ModeInfo *mi);
    DL(2, "DEBUG: set_spin called with %d, current do_spin=%d\n",
       new_spin_enabled, do_spin);
    do_spin = new_spin_enabled;
    DL(1, "Spin %s (do_spin now=%d)\n",
       do_spin ? "enabled" : "disabled", do_spin);
    update_hextrail_rotator(&web_mi);
}

EMSCRIPTEN_KEEPALIVE
void set_wander(int new_wander_enabled) {
    extern void update_hextrail_rotator(ModeInfo *mi);
    DL(2, "DEBUG: set_wander called with %d, current do_wander=%d\n",
       new_wander_enabled, do_wander);
    do_wander = new_wander_enabled;
    DL(1, "Wander %s (do_wander now=%d)\n",
       do_wander ? "enabled" : "disabled", do_wander);
    update_hextrail_rotator(&web_mi);
}

// Web-specific main function
EMSCRIPTEN_KEEPALIVE
int main() {
    // Initialize the web wrapper with hextrail's functions
    return xscreensaver_web_init(
        init_hextrail,         // init function
        draw_hextrail,         // draw function
        reshape_hextrail,      // reshape function
        free_hextrail,         // free function
        hextrail_handle_event, // handle_event function
        &hextrail_opts,        // ModeSpecOpt (vars/opts)
        DEFAULTS               // defaults string
    );
}
