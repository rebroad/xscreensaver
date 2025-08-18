/* hextrail_web_main.c - Web Entry Point for HexTrail
 *
 * This provides the web-specific code needed to run hextrail
 * using the generic xscreensaver_web wrapper.
 */

#include <emscripten.h>
#include "../xscreensaver_web.c"

// Include our web headers (WEB_BUILD already defined by build script)
#include "../xlockmore.h"

// Enable matrix debug wrappers for the hack when requested
#ifdef MATRIX_DEBUG
#include "../matrix_debug.h"
#endif

// Now include hextrail.c with our web headers already defined
#include "../../hacks/glx/hextrail.c"

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
