/* debug.c - Debug logging utilities for xscreensaver hacks
 *
 * This provides a common debug logging interface that can be used
 * by all xscreensaver hacks, both native and web builds.
 */

#include "debug.h"
#include <stdio.h>
#include <stdarg.h>

// Global debug level - can be modified at runtime
int debug_level = -1;  // Default to no debug output (not even stderr)

// Debug logging function
// level 0: errors (stderr)
// level 1+: info/debug (stdout)
void DL(int level, const char* format, ...) {
  if (level <= debug_level) {
    va_list args;
    va_start(args, format);
    if (level == 0) {
      vfprintf(stderr, format, args);
    } else {
      vfprintf(stdout, format, args);
    }
    va_end(args);
  }
}
