/* hextrail_web_main.c - Minimal Web Entry Point for HexTrail
 *
 * This provides the minimal web-specific code needed to run hextrail
 * using the generic xscreensaver_web wrapper.
 */

#include <emscripten.h>
#include "../xscreensaver_web.c"

// Include our web headers (WEB_BUILD already defined by build script)
#include "../xlockmore_web.h"

// Always include yarandom for WebGL builds to ensure frand() is available
#include "../utils/yarandom.h"

#ifdef MATRIX_DEBUG
// Include matrix debug headers for deterministic random functions
#include "../matrix_debug.h"
#endif

// Now include hextrail.c with our web headers already defined
#include "hextrail.c"

// Web-specific main function
EMSCRIPTEN_KEEPALIVE
int main() {
    // Initialize the web wrapper with hextrail's functions
    return xscreensaver_web_init(
        init_hextrail,         // init function
        draw_hextrail,         // draw function
        reshape_hextrail,      // reshape function
        free_hextrail,         // free function
        hextrail_handle_event  // handle_event function
    );
}
