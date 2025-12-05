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
extern int dl_write_atomic (int verbose_level, int level, const char *file, int line, const char *fmt, ...);

/* DL macro - takes verbose level as first argument, then format string and args.
   When not running under systemd and not logging to file, include timestamp and PID via blurb().
   systemd/journalctl automatically adds progname, PID, and timestamp, so we skip blurb() in that case.

   This macro formats the entire log line into a buffer and writes it atomically in one call
   to prevent interleaving when multiple processes write to the same log file.

   Uses the verbose_p variable (global or local if shadowed). To use a per-instance verbose
   setting, declare a local variable: int verbose_p = p->verbose_p; before using DL. */
#define DL(level, ...) \
  do { \
    dl_write_atomic (verbose_p, level, __FILE__, __LINE__, __VA_ARGS__); \
  } while (0)

/* Debug logging macro - always logs (uses DL with level 0) */
#define debug_log(...) DL(0, __VA_ARGS__)

#endif /*  __BLURB_H__ */

