/* test-xdpms.c --- playing with the XDPMS extension.
 * xscreensaver, Copyright © 1998-2021 Jamie Zawinski <jwz@jwz.org>
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

#include <stdlib.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include <stdio.h>
#include <time.h>
#include <sys/time.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Intrinsic.h>

#include <X11/Xproto.h>
#include <X11/extensions/dpms.h>
/*#include <X11/extensions/dpmsstr.h>*/

#include "blurb.h"
char *progclass = "XScreenSaver";

static Bool error_handler_hit_p = False;

static int
ignore_all_errors_ehandler (Display *dpy, XErrorEvent *error)
{
  error_handler_hit_p = True;
  return 0;
}


int
main (int argc, char **argv)
{
  int delay = 10;

  int event_number, error_number;
  int major, minor;
  CARD16 standby, suspend, off;
  CARD16 state;
  BOOL onoff;

  XtAppContext app;
  Widget toplevel_shell = XtAppInitialize (&app, progclass, 0, 0,
					   &argc, argv, 0, 0, 0);
  Display *dpy = XtDisplay (toplevel_shell);

  if (!DPMSQueryExtension(dpy, &event_number, &error_number))
    {
      DL(0, "DPMSQueryExtension(dpy, ...) ==> False");
      DL(0, "server does not support the XDPMS extension");
      exit(1);
    }
  else
    DL(0, "DPMSQueryExtension(dpy, ...) ==> %d, %d", event_number, error_number);

  if (!DPMSCapable(dpy))
    {
      DL(0, "DPMSCapable(dpy) ==> False");
      DL(0, "server says hardware doesn't support DPMS");
      exit(1);
    }
  else
    DL(0, "DPMSCapable(dpy) ==> True");

  if (!DPMSGetVersion(dpy, &major, &minor))
    {
      DL(0, "DPMSGetVersion(dpy, ...) ==> False");
      DL(0, "server didn't report XDPMS version numbers?");
    }
  else
    DL(0, "DPMSGetVersion(dpy, ...) ==> %d, %d", major, minor);

  if (!DPMSGetTimeouts(dpy, &standby, &suspend, &off))
    {
      DL(0, "DPMSGetTimeouts(dpy, ...) ==> False");
      DL(0, "server didn't report DPMS timeouts?");
    }
  else
    DL(0, "DPMSGetTimeouts(dpy, ...) ==> standby = %d, suspend = %d, off = %d", standby, suspend, off);

  while (1)
    {
      if (!DPMSInfo(dpy, &state, &onoff))
	{
      DL(0, "DPMSInfo(dpy, ...) ==> False");
      DL(0, "couldn't read DPMS state?");
	  onoff = 0;
	  state = -1;
	}
      else
	{
      DL(0, "DPMSInfo(dpy, ...) ==> %s, %s",
         (state == DPMSModeOn ? "DPMSModeOn" :
          state == DPMSModeStandby ? "DPMSModeStandby" :
          state == DPMSModeSuspend ? "DPMSModeSuspend" :
          state == DPMSModeOff ? "DPMSModeOff" : "???"),
         (onoff == 1 ? "On" : onoff == 0 ? "Off" : "???"));
	}

      if (state == DPMSModeStandby ||
	  state == DPMSModeSuspend ||
	  state == DPMSModeOff)
	{
	  int st;
      DL(0, "monitor is off; turning it on");

          XSync (dpy, False);
          error_handler_hit_p = False;
          XSetErrorHandler (ignore_all_errors_ehandler);
          XSync (dpy, False);
	  st = DPMSForceLevel (dpy, DPMSModeOn);
          XSync (dpy, False);
          if (error_handler_hit_p) st = -666;

      DL(0, "DPMSForceLevel (dpy, DPMSModeOn) ==> %s", (st == -666 ? "X Error" : st ? "Ok" : "Error"));
	}

      sleep (delay);
    }
}
