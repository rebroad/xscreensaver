/* debug.h - Debug logging utilities for xscreensaver hacks
 *
 * This provides declarations for the common debug logging interface
 * that can be used by all xscreensaver hacks.
 */

#ifndef DEBUG_H
#define DEBUG_H

// Global debug level - can be modified at runtime
extern int debug_level;

// Debug logging function
// level 0: errors (stderr)
// level 1+: info/debug (stdout)
extern void DL(int level, const char* format, ...);

#endif /* DEBUG_H */
