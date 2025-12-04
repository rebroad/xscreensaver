/* xscreensaver, Copyright © 1991-2022 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "blurb.h"

#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>  /* for getpid() */

const char *progname = "";
int verbose_p = 0;

/* #define BLURB_CENTISECONDS */

const char *
blurb (void)
{
  static char buf[255] = { 0 };
  struct tm tm;
  struct timeval now;
  int i;
  pid_t pid = getpid();

# ifdef GETTIMEOFDAY_TWO_ARGS
  struct timezone tzp;
  gettimeofday (&now, &tzp);
# else
  gettimeofday (&now);
# endif

  localtime_r (&now.tv_sec, &tm);
  i = strlen (progname);
  if (i > 40) i = 40;
  memcpy (buf, progname, i);
  buf[i++] = '[';
  /* Format PID (max 10 digits) */
  {
    char pid_str[12];
    snprintf (pid_str, sizeof(pid_str), "%ld", (long) pid);
    int pid_len = strlen (pid_str);
    if (i + pid_len + 1 < sizeof(buf))
      {
        memcpy (buf + i, pid_str, pid_len);
        i += pid_len;
      }
  }
  buf[i++] = ']';
  buf[i++] = ':';
  buf[i++] = ' ';
  buf[i++] = '0' + (tm.tm_hour >= 10 ? tm.tm_hour/10 : 0);
  buf[i++] = '0' + (tm.tm_hour % 10);
  buf[i++] = ':';
  buf[i++] = '0' + (tm.tm_min >= 10 ? tm.tm_min/10 : 0);
  buf[i++] = '0' + (tm.tm_min % 10);
  buf[i++] = ':';
  buf[i++] = '0' + (tm.tm_sec >= 10 ? tm.tm_sec/10 : 0);
  buf[i++] = '0' + (tm.tm_sec % 10);

# ifdef BLURB_CENTISECONDS
  {
    int c = now.tv_usec / 10000;
    buf[i++] = '.';
    buf[i++] = '0' + (c >= 10 ? c/10 : 0);
    buf[i++] = '0' + (c % 10);
  }
# endif

  buf[i] = 0;
  return buf;
}

/* Debug logging to file */
void
debug_log (const char *format, ...)
{
  static FILE *debug_file = NULL;
  static int debug_file_initialized = 0;
  static int debug_file_cleared = 0;
  va_list args;
  time_t now;
  struct tm *tm_info;
  char timestamp[64];

  if (!debug_file_initialized)
    {
      const char *home = getenv ("HOME");
      char path[512];
      if (home)
        snprintf (path, sizeof(path), "%s/.xscreensaver-debug.log", home);
      else
        snprintf (path, sizeof(path), "/tmp/xscreensaver-debug.log");

      /* Clear the log file on first initialization (startup/restart) */
      if (!debug_file_cleared)
        {
          FILE *f = fopen (path, "w");
          if (f)
            {
              fprintf (f, "=== XScreenSaver Debug Log (cleared on startup) ===\n");
              fclose (f);
            }
          debug_file_cleared = 1;
        }

      debug_file = fopen (path, "a");
      debug_file_initialized = 1;
      if (debug_file)
        setvbuf (debug_file, NULL, _IOLBF, 0);  /* Line buffered */
    }

  if (!debug_file)
    return;

  va_start (args, format);
  vfprintf (debug_file, format, args);
  va_end (args);
  fprintf (debug_file, "\n");
  fflush (debug_file);
}

