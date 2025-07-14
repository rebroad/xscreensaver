#pragma once

typedef struct jwxyz_XVisualInfo {
  Visual *visual;
  VisualID visualid;
  int screen;
  int depth;
  int class;
  unsigned long red_mask;
  unsigned long green_mask;
  unsigned long blue_mask;
  int colormap_size;
  int bits_per_rgb;
} jwxyz_XVisualInfo;

typedef struct jwxyz_XVisualInfo XVisualInfo;
typedef unsigned long VisualID;

#define VisualScreenMask  (1L << 0)
#define VisualIDMask      (1L << 1)

// Dummy function declarations
VisualID XVisualIDFromVisual(Visual *visual);
XVisualInfo *XGetVisualInfo(Display *display, long vinfo_mask, XVisualInfo *vinfo_template, int *nitems_return); 