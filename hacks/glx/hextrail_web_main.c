/* hextrail_web_main.c - Web Entry Point for HexTrail
 *
 * This provides the web-specific code needed to run hextrail
 * using the generic xscreensaver_web wrapper.
 */

#include <emscripten.h>
#include "../xscreensaver_web.c"

// Include our web headers (WEB_BUILD already defined by build script)
#include "../xlockmore_web.h"

// Now include hextrail.c with our web headers already defined
#include "hextrail.c"

// External variables from hextrail.c that we need to control
extern GLfloat speed;
extern GLfloat thickness;
extern Bool do_spin;
extern Bool do_wander;

// Update callback for hextrail rotator - defined after hextrail.c is included
void update_hextrail_rotator(void* value) {
    // This will be called when spin/wander settings change
    // The actual rotator update is handled in hextrail.c
    (void)value; // Suppress unused parameter warning
}

// Web-specific main function
EMSCRIPTEN_KEEPALIVE
int main() {
    // Register hextrail's parameters for web UI control
    register_web_parameter("speed", "Speed", WEB_PARAM_FLOAT, &speed, NULL, 0.1, 5.0, 0.1);
    register_web_parameter("thickness", "Thickness", WEB_PARAM_FLOAT, &thickness, NULL, 0.01, 1.0, 0.01);
    register_web_parameter("spin", "Spin", WEB_PARAM_BOOL, &do_spin, update_hextrail_rotator, 0, 1, 1);
    register_web_parameter("wander", "Wander", WEB_PARAM_BOOL, &do_wander, update_hextrail_rotator, 0, 1, 1);

    // Initialize the web wrapper with hextrail's functions
    return xscreensaver_web_init(
        init_hextrail,         // init function
        draw_hextrail,         // draw function
        reshape_hextrail,      // reshape function
        free_hextrail,         // free function
        hextrail_handle_event  // handle_event function
    );
}
