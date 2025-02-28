#ifdef USE_SDL

/* SDL stubs: Minimal implementation for OpenGL compatibility */
Visual *
get_visual (Screen *screen, const char *string, Bool prefer_writable_cells,
            Bool verbose_p) {
  return NULL; // SDL uses OpenGL context, no X11 Visual needed
}

Visual *
get_visual_resource (Screen *screen, char *name, char *class,
                     Bool prefer_writable_cells) {
  return NULL; // Default to SDL's OpenGL setup
}

int visual_depth (Screen *screen, Visual *visual) {
  return 24; // Assume 24-bit color depth (RGB888)
}

int visual_pixmap_depth (Screen *screen, Visual *visual) {
  return 32; // Assume 32-bit pixmap depth (RGBA8888)
}

int visual_class (Screen *screen, Visual *visual) {
  return TrueColor; // SDL with OpenGL typically uses TrueColor
}

Bool has_writable_cells (Screen *screen, Visual *visual) {
  return False; // OpenGL doesn't use writable cells
}

void
describe_visual (FILE *f, Screen *screen, Visual *visual, Bool private_cmap_p) {
  fprintf(f, "SDL/OpenGL (TrueColor depth: 24)\n");
}

int screen_number (Screen *screen) {
  return 0; // Single screen in SDL
}

int visual_cells (Screen *screen, Visual *visual) {
  return 0; // Not applicable in OpenGL
}

Visual * find_similar_visual(Screen *screen, Visual *old_visual) {
  return NULL; // No visual management in SDL
}

void
visual_rgb_masks (Screen *screen, Visual *visual, unsigned long *red_mask,
                  unsigned long *green_mask, unsigned long *blue_mask) {
  *red_mask = 0xFF0000;
  *green_mask = 0x00FF00;
  *blue_mask = 0x0000FF;
}

#endif /* USE_SDL */
