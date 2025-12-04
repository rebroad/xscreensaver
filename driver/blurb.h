/* progname plus timestamp */

#ifndef __BLURB_H__
#define __BLURB_H__

#include <stdarg.h>
#include <stdio.h>

extern const char *progname;
extern int verbose_p;
extern const char *blurb (void);

/* DL macro - takes verbose level as first argument, then format string and args.
   Note: systemd/journalctl already adds progname, PID, and timestamp, so we don't use blurb() here. */
#define DL(level, ...) \
  do { \
    if (verbose_p >= (level)) { \
      fprintf (stderr, "[%s:%d] ", __FILE__, __LINE__); \
      fprintf (stderr, __VA_ARGS__); \
      fprintf (stderr, "\n"); \
    } \
  } while (0)

/* Debug logging macro - always logs (uses DL with level 0) */
#define debug_log(...) DL(0, __VA_ARGS__)

#endif /*  __BLURB_H__ */

