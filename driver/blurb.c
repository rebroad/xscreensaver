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

#ifdef HAVE_FCNTL_H
# include <fcntl.h>  /* for flock */
#endif
#ifdef HAVE_SYS_FILE_H
# include <sys/file.h>  /* for flock on some systems */
#endif

const char *progname = "";
int verbose_p = 0;
int logging_to_file_p = 0;
int running_under_systemd_p = 0;

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


/* Format and write a complete log line atomically to prevent interleaving
   when multiple processes write to the same log file.

   Uses file locking (flock) when logging to a file to serialize writes,
   avoiding the need for a buffer. For non-file logging, uses the original
   multiple fprintf approach since interleaving is less of a concern.

   Returns the number of characters written, or -1 on error. */
int
dl_write_atomic (int level, const char *file, int line, const char *fmt, ...)
{
  va_list args;
  int fd = fileno (stderr);
  int locked = 0;

  if (verbose_p < level)
    return 0;

  /* Use file locking when logging to a file to prevent interleaving */
  if (logging_to_file_p && fd >= 0)
    {
# ifdef LOCK_EX
      /* Try to acquire exclusive lock (non-blocking) */
      if (flock (fd, LOCK_EX | LOCK_NB) == 0)
        locked = 1;
      /* If lock fails, continue anyway - better to have interleaved logs than no logs */
# endif
    }

  /* Write the log line using multiple fprintf calls (original approach) */
  if (!running_under_systemd_p || logging_to_file_p)
    {
      fprintf (stderr, "%s: ", blurb());
    }
  fprintf (stderr, "[%s:%d] ", file, line);
  va_start (args, fmt);
  vfprintf (stderr, fmt, args);
  va_end (args);
  fprintf (stderr, "\n");

  /* Flush immediately when logging to file */
  if (logging_to_file_p)
    fflush (stderr);

  /* Release lock if we acquired it */
  if (locked)
    {
# ifdef LOCK_UN
      flock (fd, LOCK_UN);
# endif
    }

  return 0;  /* Can't easily return byte count with this approach */
}

