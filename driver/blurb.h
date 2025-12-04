/* progname plus timestamp */

#ifndef __BLURB_H__
#define __BLURB_H__

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

extern const char *progname;
extern int verbose_p;
extern int logging_to_file_p;
extern int running_under_systemd_p;
extern const char *blurb (void);

/* DL macro - takes verbose level as first argument, then format string and args.
   When not running under systemd and not logging to file, include timestamp and PID via blurb().
   systemd/journalctl automatically adds progname, PID, and timestamp, so we skip blurb() in that case. */
#define DL(level, ...) \
  do { \
    if (verbose_p >= (level)) { \
      if (!running_under_systemd_p || logging_to_file_p) { \
        fprintf (stderr, "%s: ", blurb()); \
      } \
      fprintf (stderr, "[%s:%d] ", __FILE__, __LINE__); \
      fprintf (stderr, __VA_ARGS__); \
      fprintf (stderr, "\n"); \
    } \
  } while (0)

/* Debug logging macro - always logs (uses DL with level 0) */
#define debug_log(...) DL(0, __VA_ARGS__)

#endif /*  __BLURB_H__ */

