/* xscreensaver, Copyright (c) 1992-2013 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#ifndef __COLORS_H__
#define __COLORS_H__

#include "sdlcompat.h"

/* Like XFreeColors, but works on `XColor *' instead of `unsigned long *'
 */
#ifndef USE_SDL
extern void free_colors (Screen *, Colormap, XColor *, int ncolors);

/* Allocates writable, non-contiguous color cells.  The number requested is
   passed in *ncolorsP, and the number actually allocated is returned there.
   (Unlike XAllocColorCells(), this will allocate as many as it can, instead
   of failing if they can't all be allocated.)
 */
extern void allocate_writable_colors (Screen *, Colormap,
				      unsigned long *pixels, int *ncolorsP);

#endif

/* Generates a sequence of colors evenly spaced between the given pair
   of HSV coordinates.

   If closed_p is true, the colors will go from the first point to the
   second then back to the first.

   If allocate_p is true, the colors will be allocated from the map;
   if enough colors can't be allocated, we will try for less, and the
   result will be returned to ncolorsP.

   If writable_p is true, writable color cells will be allocated;
   otherwise, read-only cells will be allocated.

   If allocate_p is false, screen and cmap are unused (OpenGL usage).
 */
#ifdef USE_SDL
extern void make_color_ramp (
#else
extern void make_color_ramp (Screen *, Visual *, Colormap,
#endif
			     int h1, double s1, double v1,
			     int h2, double s2, double v2,
#ifdef USE_SDL
				 SDL_Color *colors,
#else
			     XColor *colors,
#endif
				 int *ncolorsP,
			     Bool closed_p,
			     Bool allocate_p,
			     Bool *writable_pP);

/* Generates a sequence of colors evenly spaced around the triangle
   indicated by the thee HSV coordinates.

   If allocate_p is true, the colors will be allocated from the map;
   if enough colors can't be allocated, we will try for less, and the
   result will be returned to ncolorsP.

   If writable_p is true, writable color cells will be allocated;
   otherwise, read-only cells will be allocated.

   If allocate_p is false, screen, visual and cmap are unused (OpenGL usage).
 */
extern void make_color_loop (
#ifndef USE_SDL
                 Screen *, Visual *, Colormap,
#endif
			     int h1, double s1, double v1,
			     int h2, double s2, double v2,
			     int h3, double s3, double v3,
#ifdef USE_SDL
				 SDL_Color *colors,
#else
			     XColor *colors,
#endif
				 int *ncolorsP, Bool allocate_p, Bool *writable_pP);


/* Allocates a hopefully-interesting colormap, which will be a closed loop
   without any sudden transitions.

   If allocate_p is true, the colors will be allocated from the map;
   if enough colors can't be allocated, we will try for less, and the
   result will be returned to ncolorsP.  An error message will be
   printed on stderr (if verbose_p).

   If *writable_pP is true, writable color cells will be allocated;
   otherwise, read-only cells will be allocated.  If no writable cells
   cannot be allocated, we will try to allocate unwritable cells
   instead, and print a message on stderr to that effect (if verbose_p).

   If allocate_p is false, screen, visual and cmap are unused (OpenGL usage).
 */
extern void make_smooth_colormap(
#ifdef USE_SDL
                  SDL_Color *,
#else
                  Screen *, Visual *, Colormap, XColor *,
#endif
				  int *, Bool, Bool *, Bool);

/* Allocates a uniform colormap which touches each hue of the spectrum,
   evenly spaced.  The saturation and intensity are chosen randomly, but
   will be high enough to be visible.

   If allocate_p is true, the colors will be allocated from the map;
   if enough colors can't be allocated, we will try for less, and the
   result will be returned to ncolorsP.  An error message will be
   printed on stderr (if verbose_p).

   If *writable_pP is true, writable color cells will be allocated;
   otherwise, read-only cells will be allocated.  If no writable cells
   cannot be allocated, we will try to allocate unwritable cells
   instead, and print a message on stderr to that effect (if verbose_p).

   If allocate_p is false, screen, visual and cmap are unused (OpenGL usage).
 */
extern void make_uniform_colormap (
#ifdef USE_SDL
                   SDL_Color *,
#else
		           Screen *, Visual *, Colormap, XColor *,
#endif
				   int *, Bool, Bool *, Bool);

/* Allocates a random colormap (the colors are unrelated to one another.)
   If `bright_p' is false, the colors will be completely random; if it is
   true, all of the colors will be bright enough to see on a black background.

   If allocate_p is true, the colors will be allocated from the map;
   if enough colors can't be allocated, we will try for less, and the
   result will be returned to ncolorsP.  An error message will be
   printed on stderr (if verbose_p).

   If *writable_pP is true, writable color cells will be allocated;
   otherwise, read-only cells will be allocated.  If no writable cells
   cannot be allocated, we will try to allocate unwritable cells
   instead, and print a message on stderr to that effect (if verbose_p).

   If allocate_p is false, screen, visual and cmap are unused (OpenGL usage).
 */
#ifndef USE_SDL
extern void make_random_colormap (Screen *, Visual *, Colormap,
				  XColor *colors, int *ncolorsP,
				  Bool bright_p,
				  Bool allocate_p,
				  Bool *writable_pP,
				  Bool verbose_p);
#endif

/* Assuming that the array of colors indicates the current state of a set
   of writable color cells, this rotates the contents of the array by
   `distance' steps, moving the colors of cell N to cell (N - distance).
 */
#ifndef USE_SDL
extern void rotate_colors (Screen *, Colormap,
			   XColor *, int ncolors, int distance);
#endif

#endif /* __COLORS_H__ */
