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
#include <unistd.h>  /* for getpid(), write(), fsync() */
#include <errno.h>   /* for errno, EINTR */

/* Include sys/file.h for flock() - standard on Linux and most Unix systems.
   Try to include it - if the system doesn't have it, the #ifdef LOCK_EX will
   prevent using flock. */
#include <sys/file.h>

const char *progname = "";
int verbose_p = 0;
int logging_to_file_p = 0;
int running_under_systemd_p = 0;

#define BLURB_CENTISECONDS

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

# if defined(BLURB_CENTISECONDS) || defined(BLURB_MILLISECONDS)
  {
    int ms = now.tv_usec / 1000;
    buf[i++] = '.';
    buf[i++] = '0' + (ms >= 100 ? ms/100 : 0);
    buf[i++] = '0' + ((ms % 100) >= 10 ? (ms % 100)/10 : 0);
# if defined(BLURB_MILLISECONDS)
    buf[i++] = '0' + (ms % 10);
# endif
  }
# endif

  buf[i] = 0;
  return buf;
}


/* Format and write a complete log line atomically to prevent interleaving
   when multiple processes write to the same log file.

   Formats the entire log line into a single buffer, then writes it in one
   atomic operation while holding a file lock. This ensures that log lines
   from different processes cannot be interleaved.

   verbose_level: the current verbose level to check against (typically verbose_p or p->verbose_p)
   level: the minimum verbose level required for this message to be logged
   file, line: source file and line number for the log message
   fmt, ...: printf-style format string and arguments

   Returns the number of characters written, or -1 on error. */
int
dl_write_atomic (int verbose_level, int level, const char *file, int line, const char *fmt, ...)
{
  va_list args;
  int fd = fileno (stderr);
  int locked = 0;
  char buffer[4096];
  int pos = 0;
  int written = 0;
  const char *blurb_str = NULL;

  if (verbose_level < level)
    return 0;

  /* Use file locking when logging to a file to prevent interleaving */
  if (logging_to_file_p && fd >= 0)
    {
# ifdef LOCK_EX
      /* Acquire exclusive lock (blocking) to ensure atomic writes */
      if (flock (fd, LOCK_EX) == 0)
        locked = 1;
      /* If lock fails, continue anyway - better to have interleaved logs than no logs */
# endif
    }

  /* Format the entire log line into a single buffer */
  if (!running_under_systemd_p || logging_to_file_p)
    {
      blurb_str = blurb();
      pos = snprintf (buffer, sizeof(buffer), "%s: ", blurb_str);
      if (pos < 0 || pos >= (int)sizeof(buffer))
        pos = sizeof(buffer) - 1;
    }

  /* Add file:line prefix */
  {
    int n = snprintf (buffer + pos, sizeof(buffer) - pos, "[%s:%d] ", file, line);
    if (n > 0 && pos + n < (int)sizeof(buffer))
      pos += n;
    else if (pos < (int)sizeof(buffer))
      pos = sizeof(buffer) - 1;
  }

  /* Format the message */
  va_start (args, fmt);
  {
    int n = vsnprintf (buffer + pos, sizeof(buffer) - pos, fmt, args);
    if (n > 0 && pos + n < (int)sizeof(buffer))
      pos += n;
    else if (pos < (int)sizeof(buffer))
      pos = sizeof(buffer) - 1;
  }
  va_end (args);

  /* Add newline */
  if (pos < (int)sizeof(buffer) - 1)
    {
      buffer[pos++] = '\n';
      buffer[pos] = '\0';
    }
  else
    {
      buffer[sizeof(buffer) - 2] = '\n';
      buffer[sizeof(buffer) - 1] = '\0';
      pos = sizeof(buffer) - 1;
    }

  /* Write the entire formatted line in one operation */
  /* Loop to handle partial writes */
  {
    ssize_t total_written = 0;
    ssize_t n;
    while (total_written < pos)
      {
        n = write (fd, buffer + total_written, pos - total_written);
        if (n < 0)
          {
            if (errno == EINTR)
              continue;  /* Interrupted, try again */
            break;  /* Error, give up */
          }
        total_written += n;
      }
    written = (int)total_written;  /* Safe cast since buffer is limited to 4096 */
  }

  /* Flush immediately when logging to file */
  if (logging_to_file_p)
    fsync (fd);  /* Ensure data is written to disk */

  /* Release lock if we acquired it */
  if (locked)
    {
# ifdef LOCK_UN
      flock (fd, LOCK_UN);
# endif
    }

  return written;
}

