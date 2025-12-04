/* progname plus timestamp */

#ifndef __BLURB_H__
#define __BLURB_H__

#include <stdarg.h>
#include <stdio.h>

extern const char *progname;
extern int verbose_p;
extern const char *blurb (void);

/* Debug logging macro - checks verbose_p and prints to stderr with file and line.
   Note: systemd/journalctl already adds progname, PID, and timestamp, so we don't use blurb() here. */
#define debug_log(...) \
  do { \
    if (verbose_p) { \
      fprintf (stderr, "[%s:%d] ", __FILE__, __LINE__); \
      fprintf (stderr, __VA_ARGS__); \
      fprintf (stderr, "\n"); \
    } \
  } while (0)

#endif /*  __BLURB_H__ */

