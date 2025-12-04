/* progname plus timestamp */

#ifndef __BLURB_H__
#define __BLURB_H__

#include <stdarg.h>

extern const char *progname;
extern int verbose_p;
extern const char *blurb (void);

/* Debug logging to file */
extern void debug_log (const char *format, ...);

#endif /*  __BLURB_H__ */

