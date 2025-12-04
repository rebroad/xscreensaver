/* xscreensaver, Copyright © 1992-2021 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 */

#ifndef __FADE_H__
#define __FADE_H__

/* Returns true if canceled by user activity.
   If interrupted_ratio is non-NULL and fade-out is interrupted, stores the
   current fade ratio (0.0-1.0) in *interrupted_ratio.
   If start_ratio is >= 0.0 and out_p is False, fade-in starts from start_ratio
   instead of 0.0 (used when resuming from an interrupted fade-out).
 */
extern Bool fade_screens (XtAppContext app, Display *dpy,
						  Window *black_windows, int nwindows,
			  double seconds, Bool out_p, Bool from_desktop_p,
						  void **closureP, double *interrupted_ratio, double start_ratio);

/* Like XDestroyWindow, but destroys the window later, on a timer.  This is
   necessary to work around a KDE 5 compositor bug.  Without this, destroying
   an old window causes the desktop to briefly become visible, even though a
   new window has already been mapped that is obscuring both of them!
 */
extern void defer_XDestroyWindow (XtAppContext, Display *, Window);

#endif /* __FADE_H__ */
