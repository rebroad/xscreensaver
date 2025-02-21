/* hextrail, Copyright (c) 2022 Jamie Zawinski <jwz@jwz.org>
 */

#define DEFAULTS	"*delay:	30000       \n" \
            "*showFPS:      False       \n" \
            "*wireframe:    False       \n" \
            "*count:        20          \n" \
            "*suppressRotationAnimation: True\n" \

# define release_hextrail 0

#ifdef USE_SDL
#include <SDL3/SDL.h>
//#include <SDL3/SDL_keycode.h>
#define Bool int
#ifdef _Win32
#include <windows.h>
#endif
#include <GL/gl.h>
#include <GL/glu.h>
#endif
#include <string.h>
#include "xlockmore.h"
#include "colors.h"
#include "normals.h"
#include "rotator.h"
#include "gltrackball.h"
#include <ctype.h>
#include <math.h>
#include <time.h>

#ifdef USE_GL /* whole file */

/*#ifdef USE_SDL
SDL_Window* window;
SDL_GLContext glContext;
#endif*/

#define DEF_SPIN        "True"
#define DEF_WANDER      "True"
#define DEF_GLOW        "True"
#define DEF_NEON        "False"
#define DEF_SPEED       "1.0"
#define DEF_THICKNESS   "0.15"

#define BELLRAND(n) ((frand((n)) + frand((n)) + frand((n))) / 3)

typedef enum { EMPTY, IN, WAIT, OUT, DONE } state_t;

typedef struct {
  state_t state;
  GLfloat ratio;
  GLfloat speed;
} arm;

typedef struct hexagon hexagon;

struct hexagon {
  XYZ pos;
  hexagon *neighbors[6];
  arm arms[6];
  int ccolor;
  state_t border_state;
  GLfloat border_ratio;
  Bool ignore;
  Bool doing;
};

typedef struct {
#ifdef USE_SDL
  SDL_Window *window;
  SDL_GLContext gl_context;
  SDL_Color *colors;
#else
  GLXContext *glx_context;
  XColor *colors;
#endif
  rotator *rot;
  trackball_state *trackball;
  Bool button_down_p;
  Bool button_pressed;

  int grid_w, grid_h;
  hexagon *hexagons;
  int doing;
  int ignored;
  enum { FIRST, DRAW, FADE } state;
  GLfloat fade_ratio;

  int ncolors;
} hextrail_configuration;

static hextrail_configuration *bps = NULL;

static Bool do_spin;
static GLfloat speed;
static Bool do_wander;
static Bool do_glow;
static Bool do_neon;
static GLfloat thickness;

static XrmOptionDescRec opts[] = {
  { "-spin",   ".spin",   XrmoptionNoArg, "True" },
  { "+spin",   ".spin",   XrmoptionNoArg, "False" },
  { "-speed",  ".speed",  XrmoptionSepArg, 0 },
  { "-wander", ".wander", XrmoptionNoArg, "True" },
  { "+wander", ".wander", XrmoptionNoArg, "False" },
  { "-glow",   ".glow",   XrmoptionNoArg, "True" },
  { "+glow",   ".glow",   XrmoptionNoArg, "False" },
  { "-neon",   ".neon",   XrmoptionNoArg, "True" },
  { "+neon",   ".neon",   XrmoptionNoArg, "False" },
  { "-thickness", ".thickness", XrmoptionSepArg, 0 },
#ifdef USE_SDL
  { 0, 0, 0, 0 }
#endif
};

static argtype vars[] = {
  {&do_spin,   "spin",   "Spin",   DEF_SPIN,   t_Bool},
  {&do_wander, "wander", "Wander", DEF_WANDER, t_Bool},
  {&do_glow,   "glow",   "Glow",   DEF_GLOW,   t_Bool},
  {&do_neon,   "neon",   "Neon",   DEF_NEON,   t_Bool},
  {&speed,     "speed",  "Speed",  DEF_SPEED,  t_Float},
  {&thickness, "thickness", "Thickness", DEF_THICKNESS, t_Float},
};

ENTRYPOINT ModeSpecOpt hextrail_opts = {countof(opts), opts, countof(vars), vars, NULL};

static void update_neighbors(ModeInfo *mi) {
  hextrail_configuration *bp = &bps[MI_SCREEN(mi)];
  int x, y;

  for (y = 0; y < bp->grid_h; y++)
    for (x = 0; x < bp->grid_w; x++) {
      hexagon *h0 = &bp->hexagons[y * bp->grid_w + x];

# define NEIGHBOR(I,XE,XO,Y) do {                                     \
        int x1 = x + (y & 1 ? (XO) : (XE));                           \
        int y1 = y + (Y);                                             \
        if (x1 >= 0 && x1 < bp->grid_w && y1 >= 0 && y1 < bp->grid_h) \
          h0->neighbors[(I)] = &bp->hexagons[y1 * bp->grid_w + x1];   \
        else                                                          \
          h0->neighbors[(I)] = NULL;                                  \
      } while (0)

      NEIGHBOR (0,  0,  1, -1);
      NEIGHBOR (1,  1,  1,  0);
      NEIGHBOR (2,  0,  1,  1);
      NEIGHBOR (3, -1,  0,  1);
      NEIGHBOR (4, -1, -1,  0);
      NEIGHBOR (5, -1,  0, -1);

# undef NEIGHBOR
	}
}

static void make_plane (ModeInfo *mi) {
  hextrail_configuration *bp = &bps[MI_SCREEN(mi)];
  int x, y;
  GLfloat size, w, h;
  hexagon *grid;

  bp->grid_w = MI_COUNT(mi) * 2;
  bp->grid_h = bp->grid_w;

  grid = (bp->hexagons
          ? bp->hexagons
          : (hexagon *) malloc (bp->grid_w * bp->grid_h * sizeof(*grid)));
  memset (grid, 0, bp->grid_w * bp->grid_h * sizeof(*grid));

  bp->ncolors = 8;
  printf("Setting button_pressed to False\n");
  bp->button_pressed = False;
  if (!bp->colors) {
#ifdef USE_SDL
    bp->colors = (SDL_Color *) calloc(bp->ncolors, sizeof(SDL_Color));
    make_smooth_colormap(bp->colors, &bp->ncolors, False, 0, False);
#else
    bp->colors = (XColor *) calloc(bp->ncolors, sizeof(XColor));
    make_smooth_colormap (0, 0, 0, bp->colors, &bp->ncolors, False, 0, False);
#endif
  }

  size = 2.0 / bp->grid_w;
  w = size;
  h = size * sqrt(3) / 2;

  bp->hexagons = grid;

  for (y = 0; y < bp->grid_h; y++) {
    for (x = 0; x < bp->grid_w; x++) {
	  int i = y * bp->grid_w + x;
      hexagon *h0 = &grid[i];
      h0->pos.x = (x - bp->grid_w/2) * w;
      h0->pos.y = (y - bp->grid_h/2) * h;
	  h0->pos.z = 0;
      h0->border_state = EMPTY;
      h0->border_ratio = 0;
	  h0->ignore = False;
	  h0->doing = False;

      if (y & 1) h0->pos.x += w / 2; // Stagger into hex arrangement

      h0->ccolor = random() % bp->ncolors;
    }
  }

  update_neighbors(mi);
}


static Bool empty_hexagon_p (hexagon *h) {
  for (int i = 0; i < 6; i++)
    if (h->arms[i].state != EMPTY) return False;
  return True;
}

static int add_arms (ModeInfo *mi, hexagon *h0, Bool out_p) {
  hextrail_configuration *bp = &bps[MI_SCREEN(mi)];
  int i;
  int added = 0;
  int target = 1 + (random() % 4);	/* Aim for 1-5 arms */

  int idx[6];				/* Traverse in random order */
  for (i = 0; i < 6; i++) idx[i] = i;
  for (i = 0; i < 6; i++) {
    int j = random() % 6;
    int swap = idx[j];
    idx[j] = idx[i];
    idx[i] = swap;
  }

  if (out_p) target--;

  for (i = 0; i < 6; i++) {
    int j = idx[i];
    hexagon *h1 = h0->neighbors[j];
    arm *a0 = &h0->arms[j];
    arm *a1;
    if (!h1) continue;			/* No neighboring cell */
    if (! empty_hexagon_p (h1)) continue;	/* Occupado */
    if (a0->state != EMPTY) continue;		/* Arm already exists */

    a1 = &h1->arms[(j + 3) % 6];		/* Opposite arm */

    if (a1->state != EMPTY) abort();
    a0->state = (out_p ? OUT : IN);
    a1->state = WAIT;
    a0->ratio = 0;
    a1->ratio = 0;
    a0->speed = 0.05 * speed * (0.8 + frand(1.0));
    a1->speed = a0->speed;

    if (h1->border_state == EMPTY) {
      h1->border_state = IN;

      /* Mostly keep the same color */
      h1->ccolor = h0->ccolor;
      if (! (random() % 5)) h1->ccolor = (h0->ccolor + 1) % bp->ncolors;
	}

    added++;
    if (added >= target) break;
  }

  if (added) {
    if (!h0->doing) {
      h0->doing = True;
      bp->doing++;
      printf("%s: added=%d pos=%.1f,%.1f doing=%d->%d ignored=%d\n", __func__, added,
			  h0->pos.x, h0->pos.y, bp->doing-1, bp->doing, bp->ignored);
    }
  } else {
    if (h0->doing) {
      h0->doing = False;
      bp->doing--;
	  if (h0->ignore) {
		bp->ignored--;
        printf("%s: pos=%.1f,%.1f ignored=%d->%d doing=%d->%d\n", __func__,
				h0->pos.x, h0->pos.y, bp->ignored+1, bp->ignored, bp->doing+1, bp->doing);
		if (bp->ignored < 2) abort();
	  } else
        printf("%s: pos=%.1f,%.1f ignored=%d doing=%d->%d\n", __func__, h0->pos.x, h0->pos.y,
				bp->ignored, bp->doing+1, bp->doing);
    }
  }

  return added;
}

static time_t debug_time = 0;

/* Check if a point is within the visible frustum */
static Bool point_visible(ModeInfo *mi, int i, hexagon *h0) {
  Bool ignore = h0->ignore;
  XYZ point = h0->pos;
  GLdouble model[16], proj[16];
  GLint viewport[4];
  GLdouble winX, winY, winZ;

  time_t current_time = time(NULL);

  /* Get current matrices and viewport */
  glGetDoublev(GL_MODELVIEW_MATRIX, model);
  glGetDoublev(GL_PROJECTION_MATRIX, proj);
  glGetIntegerv(GL_VIEWPORT, viewport);

  /* Project point to screen coordinates */
  gluProject((GLdouble)point.x, (GLdouble)point.y, (GLdouble)point.z,
             model, proj, viewport,
             &winX, &winY, &winZ);

  Bool is_visible = (winX >= viewport[0] && winX <= viewport[0] + viewport[2] &&
          winY >= viewport[1] && winY <= viewport[1] + viewport[3] &&
          winZ > 0 && winZ < 1);

  if (current_time > debug_time + 4) {
	debug_time = current_time;
	printf("\npos=%.1f,%.1f %s hexagon is o%sscreen\n", h0->pos.x, h0->pos.y, ignore ? "ignored" : "watched",
			is_visible ? "n" : "ff");
    printf("World coords (x,y,z): %.2f, %.2f, %.2f\n", point.x, point.y, point.z);
    printf("Screen coords (x,y,z): %.2f, %.2f, %.2f\n", winX, winY, winZ);
    printf("Viewport: x=%d, y=%d, w=%d, h=%d\n", 
           viewport[0], viewport[1], viewport[2], viewport[3]);
  }

  return is_visible;
}

/* Expand grid in a given direction */
static void expand_plane(ModeInfo *mi, int direction) {
  hextrail_configuration *bp = &bps[MI_SCREEN(mi)];
  int old_grid_w = bp->grid_w; int old_grid_h = bp->grid_h;
  hexagon *old_hexagons = bp->hexagons;
  int expansion = MI_COUNT(mi);  /* Amount to expand by */
  int x, y;

  /* Increase grid size */
  switch(direction) {
    case 0: /* Right */
    case 3: /* Left */
      bp->grid_w += expansion; break;
    case 1: /* Top */
    case 4: /* Bottom */
      bp->grid_h += expansion; break;
  }

  /* Allocate new grid */
  bp->hexagons = (hexagon *) calloc(bp->grid_w * bp->grid_h, sizeof(hexagon));

  /* Copy existing hexagons to new grid with offset based on direction */
  int x_offset = (direction == 3) ? expansion : 0;
  int y_offset = (direction == 4) ? expansion : 0;

  for (y = 0; y < old_grid_h; y++) for (x = 0; x < old_grid_w; x++) {
    hexagon *old_hex = &old_hexagons[y * old_grid_w + x];
    hexagon *new_hex = &bp->hexagons[(y + y_offset) * bp->grid_w + (x + x_offset)];
    *new_hex = *old_hex;

    /* Adjust position for offset */
    new_hex->pos.x += x_offset * (2.0 / old_grid_w);
    new_hex->pos.y += y_offset * (2.0 / old_grid_h);
  }

  /* Initialize new hexagons */
  GLfloat size = 2.0 / bp->grid_w;
  GLfloat h = size * sqrt(3) / 2;

  for (y = 0; y < bp->grid_h; y++) for (x = 0; x < bp->grid_w; x++) {
    int i = y *bp->grid_w + x;

    /* Skip existing hexagons */
    if (x >= x_offset && x < old_grid_w + x_offset &&
        y >= y_offset && y < old_grid_h + y_offset)
      continue;

    hexagon *h0 = &bp->hexagons[i];
    h0->pos.x = (x - bp->grid_w/2) * size;
    h0->pos.y = (y - bp->grid_h/2) * h;
	h0->pos.z = 0;
    h0->border_state = EMPTY;
    h0->border_ratio = 0;
    h0->ignore = False;
    h0->doing = False;

    if (y & 1) h0->pos.x += size / 2;

    h0->ccolor = random() % bp->ncolors;
  }

  update_neighbors(mi);

  free(old_hexagons);
}

static void tick_hexagons (ModeInfo *mi) {
  hextrail_configuration *bp = &bps[MI_SCREEN(mi)];
  int i, j;
  Bool needs_expansion = False;
  int dir = -1;

  for (i = 0; i < bp->grid_w * bp->grid_h; i++) {
    hexagon *h0 = &bp->hexagons[i];
    /* Check if we need to expand the grid */
    if (!bp->button_pressed) {
      /* Check if this is an edge hexagon with active arms */
      Bool is_edge = (h0->pos.x <= -1 || h0->pos.x >= 1 ||
                     h0->pos.y <= -1 || h0->pos.y >= 1);
	  Bool is_visible = point_visible(mi, i, h0);

	  time_t current_time = time(NULL);

	  if (is_edge && current_time > debug_time + 6) {
		  debug_time = current_time;
		  printf("\npos=%.1f,%.1f is_edge, is_visible = %d\n", h0->pos.x, h0->pos.y, is_visible);
	  }

      /* Update activity state based on visibility */
	  if (!is_visible && !h0->ignore && h0->doing) {
		  bp->ignored++;
		  printf("%s: pos=%.1f,%.1f doing=%d ignored=%d->%d\n", __func__,
				  h0->pos.x, h0->pos.y, bp->doing, bp->ignored-1, bp->ignored);
	  } else if (is_visible && h0->ignore && h0->doing) {
		  bp->ignored--;
		  printf("%s: pos=%.1f,%.1f doing=%d ignored=%d->%d\n", __func__,
				  h0->pos.x, h0->pos.y, bp->doing, bp->ignored+1, bp->ignored);
	  }
      h0->ignore = !is_visible;

      if (is_edge && is_visible) {
        for (int j = 0; j < 6; j++) {
          if (h0->arms[j].state != EMPTY) {
            needs_expansion = True;
            /* Determine expansion direction based on position */
            if (h0->pos.x >= 1) dir = 0;      /* Right */
            else if (h0->pos.y >= 1) dir = 1; /* Top */
            else if (h0->pos.x <= -1) dir = 3;/* Left */
            else if (h0->pos.y <= -1) dir = 4;/* Bottom */
            break;
          }
        }
      }

      if (needs_expansion && dir >= 0) {
        char *str;
        if (dir == 0) str = "Right";
        else if (dir == 1) str = "Up";
        else if (dir == 3) str = "Left";
        else if (dir == 4) str = "Down";
		else str = "???";
        printf("pos=%.1f,%.1f Expanding plane %s is_edge=%d is_visible=%d is_doing=%d\n",
				h0->pos.x, h0->pos.y, str, is_edge, is_visible, h0->doing);
        expand_plane(mi, dir);
      }
    } // Button never pressed

    //if (h0->frozen) continue;

    /* Enlarge any still-growing arms if active.  */
    for (j = 0; j < 6; j++) {
      arm *a0 = &h0->arms[j];
      switch (a0->state) {
        case OUT:
          if (a0->speed <= 0) abort();
          a0->ratio += a0->speed;
          if (a0->ratio > 1) {
            /* Just finished growing from center to edge.
               Pass the baton to this waiting neighbor. */
            hexagon *h1 = h0->neighbors[j];
            arm *a1 = &h1->arms[(j + 3) % 6];
            if (a1->state != WAIT) abort();
            a0->state = DONE;
            a0->ratio = 1;
            a1->state = IN;
            a1->ratio = 0;
            a1->speed = a0->speed;
		  }
          break;
        case IN:
          if (a0->speed <= 0) abort();
          a0->ratio += a0->speed;
          if (a0->ratio > 1) {
            /* Just finished growing from edge to center.
               Look for any available exits. */
            a0->state = DONE;
            a0->ratio = 1;
            add_arms(mi, h0, True);
		  }
          break;
        case EMPTY: case WAIT: case DONE:
          break;
        default:
          abort(); break;
	  }
	} // 6 arms

    switch (h0->border_state) {
      case IN:
        h0->border_ratio += 0.05 * speed;
        if (h0->border_ratio >= 1) {
          h0->border_ratio = 1;
          h0->border_state = WAIT;
        }
        break;
      case OUT:
        h0->border_ratio -= 0.05 * speed;
        if (h0->border_ratio <= 0) {
          h0->border_ratio = 0;
          h0->border_state = EMPTY;
        }
      case WAIT:
        if (! (random() % 50)) h0->border_state = OUT;
        break;
      case EMPTY:
/*
        if (! (random() % 3000))
          h0->border_state = IN;
 */
        break;
      default:
        abort(); break;
	}
  } // Loop through each hexagon

  /* Start a new cell growing.  */
  if ((bp->doing - bp->ignored) <= 0) {
	printf("New cell: doing=%d, ignored=%d\n", bp->doing, bp->ignored);
    for (i = 0; i < (bp->grid_w * bp->grid_h) / 3; i++) {
      hexagon *h0;
      int x, y;
      if (bp->state == FIRST) {
        x = bp->grid_w / 2;
        y = bp->grid_h / 2;
        bp->state = DRAW;
        bp->fade_ratio = 1;
      } else {
        x = random() % bp->grid_w;
        y = random() % bp->grid_h;
      }
      h0 = &bp->hexagons[y * bp->grid_w + x];
      if (empty_hexagon_p (h0) && add_arms (mi, h0, True)) break;
    }
  }

  if ((bp->doing - bp->ignored) <= 0 && bp->state != FADE) {
    bp->state = FADE;
    bp->fade_ratio = 1;

    for (i = 0; i < bp->grid_w * bp->grid_h; i++) {
      hexagon *h = &bp->hexagons[i];
      if (h->border_state == IN || h->border_state == WAIT)
        h->border_state = OUT;
	}
  } else if (bp->state == FADE) {
    bp->fade_ratio -= 0.01 * speed;
    if (bp->fade_ratio <= 0) {
      make_plane (mi); // TODO - not reset_hextrail?
      bp->state = FIRST;
      bp->fade_ratio = 1;
    }
  }
}


static void draw_hexagons (ModeInfo *mi) {
  hextrail_configuration *bp = &bps[MI_SCREEN(mi)];
  int wire = MI_IS_WIREFRAME(mi);
  GLfloat length = sqrt(3) / 3;
  GLfloat size = length / MI_COUNT(mi);
  GLfloat thick2 = thickness * bp->fade_ratio;
  int i;

# undef H
# define H 0.8660254037844386   /* sqrt(3)/2 */
  const XYZ corners[] = {{  0, -1,   0 },       /*      0      */
                         {  H, -0.5, 0 },       /*  5       1  */
                         {  H,  0.5, 0 },       /*             */
                         {  0,  1,   0 },       /*  4       2  */
                         { -H,  0.5, 0 },       /*      3      */
                         { -H, -0.5, 0 }};

  glFrontFace (GL_CCW);
  glBegin (wire ? GL_LINES : GL_TRIANGLES);
  glNormal3f (0, 0, 1);

  for (i = 0; i < bp->grid_w * bp->grid_h; i++) {
    hexagon *h = &bp->hexagons[i];
    int total_arms = 0;
    GLfloat color[4];
    int j;

    for (j = 0; j < 6; j++) {
      arm *a = &h->arms[j];
      if (a->state == OUT || a->state == DONE) total_arms++;
    }

#ifdef USE_SDL
    # define HEXAGON_COLOR(V,H) do { \
      int idx = (H)->ccolor; \
      (V)[0] = bp->colors[idx].r / 255.0f * bp->fade_ratio; \
      (V)[1] = bp->colors[idx].g / 255.0f * bp->fade_ratio; \
      (V)[2] = bp->colors[idx].b / 255.0f * bp->fade_ratio; \
      (V)[3] = bp->colors[idx].a / 255.0f; \
    } while (0)
#else
    # define HEXAGON_COLOR(V,H) do { \
      (V)[0] = bp->colors[(H)->ccolor].red   / 65535.0 * bp->fade_ratio; \
      (V)[1] = bp->colors[(H)->ccolor].green / 65535.0 * bp->fade_ratio; \
      (V)[2] = bp->colors[(H)->ccolor].blue  / 65535.0 * bp->fade_ratio; \
      (V)[3] = 1; \
    } while (0)
#endif

    HEXAGON_COLOR (color, h);

    for (j = 0; j < 6; j++) {
      arm *a = &h->arms[j];
      GLfloat margin = thickness * 0.4;
      GLfloat size1 = size * (1 - margin * 2);
      GLfloat size2 = size * (1 - margin * 3);
      int k = (j + 1) % 6;
      XYZ p[6];

      if (h->border_state != EMPTY) {
        GLfloat color1[3];
        memcpy (color1, color, sizeof(color1));
        color1[0] *= h->border_ratio;
        color1[1] *= h->border_ratio;
        color1[2] *= h->border_ratio;

        glColor4fv (color1);
        glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, color1);

        /* Outer edge of hexagon border */
        p[0].x = h->pos.x + corners[j].x * size1;
        p[0].y = h->pos.y + corners[j].y * size1;
        p[0].z = h->pos.z;

        p[1].x = h->pos.x + corners[k].x * size1;
        p[1].y = h->pos.y + corners[k].y * size1;
        p[1].z = h->pos.z;

        /* Inner edge of hexagon border */
        p[2].x = h->pos.x + corners[k].x * size2;
        p[2].y = h->pos.y + corners[k].y * size2;
        p[2].z = h->pos.z;
        p[3].x = h->pos.x + corners[j].x * size2;
        p[3].y = h->pos.y + corners[j].y * size2;
        p[3].z = h->pos.z;

        glVertex3f (p[0].x, p[0].y, p[0].z);
        glVertex3f (p[1].x, p[1].y, p[1].z);
        if (! wire) glVertex3f (p[2].x, p[2].y, p[2].z);
        mi->polygon_count++;

        glVertex3f (p[2].x, p[2].y, p[2].z);
        glVertex3f (p[3].x, p[3].y, p[3].z);
        if (! wire) glVertex3f (p[0].x, p[0].y, p[0].z);
        mi->polygon_count++;
      }

      /* Line from center to edge, or edge to center.  */
      if (a->state == IN || a->state == OUT || a->state == DONE) {
        GLfloat x   = (corners[j].x + corners[k].x) / 2;
        GLfloat y   = (corners[j].y + corners[k].y) / 2;
        GLfloat xoff = corners[k].x - corners[j].x;
        GLfloat yoff = corners[k].y - corners[j].y;
        GLfloat line_length = h->arms[j].ratio;
        GLfloat start, end;
        GLfloat ncolor[4];
        GLfloat color1[4];
        GLfloat color2[4];

        /* Color of the outer point of the line is average color of
           this and the neighbor. */
        HEXAGON_COLOR (ncolor, h->neighbors[j]);
        ncolor[0] = (ncolor[0] + color[0]) / 2;
        ncolor[1] = (ncolor[1] + color[1]) / 2;
        ncolor[2] = (ncolor[2] + color[2]) / 2;
        ncolor[3] = (ncolor[3] + color[3]) / 2;

        if (a->state == OUT) {
          start = 0;
          end = size * line_length;
          memcpy (color1, color,  sizeof(color1));
          memcpy (color2, ncolor, sizeof(color1));
        } else {
          start = size;
          end = size * (1 - line_length);
          memcpy (color1, ncolor, sizeof(color1));
          memcpy (color2, color,  sizeof(color1));
        }

        if (! h->neighbors[j]) abort();  /* arm/neighbor mismatch */

        /* Center */
        p[0].x = h->pos.x + xoff * size2 * thick2 + x * start;
        p[0].y = h->pos.y + yoff * size2 * thick2 + y * start;
        p[0].z = h->pos.z;
        p[1].x = h->pos.x - xoff * size2 * thick2 + x * start;
        p[1].y = h->pos.y - yoff * size2 * thick2 + y * start;
        p[1].z = h->pos.z;

        /* Edge */
        p[2].x = h->pos.x - xoff * size2 * thick2 + x * end;
        p[2].y = h->pos.y - yoff * size2 * thick2 + y * end;
        p[2].z = h->pos.z;
        p[3].x = h->pos.x + xoff * size2 * thick2 + x * end;
        p[3].y = h->pos.y + yoff * size2 * thick2 + y * end;
        p[3].z = h->pos.z;

        if (do_glow || do_neon) {
          Bool debug_now = False;
          /*static time_t debug_time = 0;
          time_t current_time = time(NULL);
          if (current_time != debug_time) {
            debug_now = True;
            debug_time = current_time;
          }*/

          if (debug_now) {
            printf("\nGLOW DEBUG:\n");
            printf("Current color: %.2f, %.2f, %.2f\n",
                   color[0], color[1], color[2]);
            printf("Current p[0]: %.2f, %.2f, %.2f\n", p[0].x, p[0].y, p[0].z);
            printf("Current p[3]: %.2f, %.2f, %.2f\n", p[3].x, p[3].y, p[3].z);
            printf("Size value: %.2f\n", size);
          }

          GLenum err = glGetError();
          if (err != GL_NO_ERROR && debug_now)
            printf("GL Error before glow: %d\n", err);

          glEnd();

          glEnable(GL_BLEND);
          if (debug_now) {
            err = glGetError();
            if (err != GL_NO_ERROR)
              printf("GL Error after enable blend: %d\n", err);

            GLboolean blend_enabled;
            glGetBooleanv(GL_BLEND, &blend_enabled);
            printf("Blend enabled: %d\n", blend_enabled);
          }

          if (do_neon)
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
          else
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);  // More natural blending

          const int glow_layers = 4;

          for (int layer = 0; layer < glow_layers; layer++) {
            GLfloat glow_scale, glow_alpha;

            if (do_neon) {
              glow_scale = 1.0 + (layer * 0.2);
              //glow_scale = 1.0 + (layer * 0.5);
              glow_alpha = 0.3 / ((layer + 1) * (layer + 1));
              //glow_alpha = 0.8 / (layer + 1);
            } else {
              glow_scale = 0.1 + ((layer+1) * 0.1);
              //glow_alpha = 0.15 / ((layer + 1) * (layer + 1));
              glow_alpha = 0.15 * pow(0.5, layer);
            }

            /* Make the glow color brighter than the base color */
            GLfloat *glow_color = color;
            /*GLfloat glow_color[4] = {
              fmin(color[0] * 2.0, 1.0),
              fmin(color[1] * 2.0, 1.0),
              fmin(color[2] * 2.0, 1.0),
              color[3]
            };*/

            float dx = p[3].x - p[0].x;
            float dy = p[3].y - p[0].y;
            float length = sqrt(dx*dx + dy*dy);

            if (debug_now && layer == 2) {
              printf("\nLayer %d:\n", layer);
              printf("Glow scale: %.2f\n", glow_scale);
              printf("Glow alpha: %.2f\n", glow_alpha);
              printf("Bright color: %.2f, %.2f, %.2f\n",
                     glow_color[0], glow_color[1], glow_color[2]);
              printf("Arm length: %.2f\n", length);
            }

            /* Center point glow */
            glBegin(GL_TRIANGLE_FAN);
            glColor4f(glow_color[0], glow_color[1], glow_color[2], glow_alpha);
            glVertex3f(p[0].x, p[0].y, p[0].z);
            for (int g = 0; g <= 16; g++) {
            //for (int g = 0; g <= 8; g++) {
              float angle = g * M_PI / 8;
              //float angle = g * M_PI / 4;
              float x = p[0].x + cos(angle) * size * glow_scale;
              float y = p[0].y + sin(angle) * size * glow_scale;
              glVertex3f(x, y, p[0].z);
            }
            glEnd();

            /* End point glow */
            glBegin(GL_TRIANGLE_FAN);
            if (!do_neon)
              glColor4f(glow_color[0], glow_color[1], glow_color[2], glow_alpha); // Needed?
            glVertex3f(p[3].x, p[3].y, p[3].z);
            for (int g = 0; g <= 16; g++) {
            //for (int g = 0; g <= 8; g++) {
              float angle = g * M_PI / 8;
              //float angle = g * M_PI / 4;
              float x = p[3].x + cos(angle) * size * glow_scale;
              float y = p[3].y + sin(angle) * size * glow_scale;
              glVertex3f(x, y, p[3].z);
            }
            glEnd();

            /* Arm glow */
            if (do_neon)
              glBegin(GL_TRIANGLE_STRIP);
            else
              glBegin(GL_QUADS);
            float nx = -dy/length * size * glow_scale;
            float ny = dx/length * size * glow_scale;

            if (!do_neon)
              glColor4f(glow_color[0], glow_color[1], glow_color[2], glow_alpha); // Needed?
            glVertex3f(p[0].x + nx, p[0].y + ny, p[0].z);
            glVertex3f(p[0].x - nx, p[0].y - ny, p[0].z);
            glVertex3f(p[3].x + nx, p[3].y + ny, p[3].z);
            glVertex3f(p[3].x - nx, p[3].y - ny, p[3].z);
            glEnd();
          }

          if (debug_now) {
            err = glGetError();
            if (err != GL_NO_ERROR) {
              printf("GL Error after glow drawing: %d\n", err);
            }
          }

          glDisable(GL_BLEND);
          glBegin(wire ? GL_LINES : GL_TRIANGLES);
        }

        glColor4fv (color2);
        glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, color2);
        glVertex3f (p[3].x, p[3].y, p[3].z);
        glColor4fv (color1);
        glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, color1);
        glVertex3f (p[0].x, p[0].y, p[0].z);
        if (! wire) glVertex3f (p[1].x, p[1].y, p[1].z);
        mi->polygon_count++;

        glVertex3f (p[1].x, p[1].y, p[1].z);

        glColor4fv (color2);
        glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, color2);
        glVertex3f (p[2].x, p[2].y, p[2].z);
        if (! wire) glVertex3f (p[3].x, p[3].y, p[3].z);
        mi->polygon_count++;
      }

      /* Hexagon (one triangle of) in center to hide line miter/bevels.  */
      if (total_arms) {
        GLfloat size3 = size * thick2 * 0.8;
        if (total_arms == 1) size3 *= 2;

        p[0] = h->pos;

        p[1].x = h->pos.x + corners[j].x * size3;
        p[1].y = h->pos.y + corners[j].y * size3;
        p[1].z = h->pos.z;

        /* Inner edge of hexagon border */
        p[2].x = h->pos.x + corners[k].x * size3;
        p[2].y = h->pos.y + corners[k].y * size3;
        p[2].z = h->pos.z;

        glColor4fv (color);
        glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, color);
        if (! wire) glVertex3f (p[0].x, p[0].y, p[0].z);
        glVertex3f (p[1].x, p[1].y, p[1].z);
        glVertex3f (p[2].x, p[2].y, p[2].z);
        mi->polygon_count++;
      }
    }
  }

  glEnd();
}


/* Window management, etc
 */
ENTRYPOINT void
reshape_hextrail (ModeInfo *mi, int width, int height)
{
  GLfloat h = (GLfloat) height / (GLfloat) width;
  int y = 0;

  if (width > height * 3) {   /* tiny window: show middle */
    height = width * 9/16;
    y = -height/2;
    h = height / (GLfloat) width;
  }

  glViewport (0, y, (GLint) width, (GLint) height);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective (30.0, 1/h, 1.0, 100.0);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  gluLookAt( 0.0, 0.0, 30.0,
             0.0, 0.0, 0.0,
             0.0, 1.0, 0.0);

  {
    GLfloat s = (MI_WIDTH(mi) < MI_HEIGHT(mi)
                 ? (MI_WIDTH(mi) / (GLfloat) MI_HEIGHT(mi))
                 : 1);
    glScalef (s, s, s);
  }

  glClear(GL_COLOR_BUFFER_BIT);
}

#ifndef USE_SDL
static void reset_hextrail(ModeInfo *mi) {
  hextrail_configuration *bp = &bps[MI_SCREEN(mi)];
  if (MI_COUNT(mi) < 1) MI_COUNT(mi) = 1;
  free (bp->hexagons);
  bp->hexagons = 0;
  bp->state = FIRST;
  bp->fade_ratio = 1;
  bp->doing = 0;
  bp->ignored = 0;
  make_plane (mi);
}
#endif

ENTRYPOINT Bool hextrail_handle_event (ModeInfo *mi,
#ifdef USE_SDL
        SDL_Event *event
#else
        XEvent *event
#endif
        ) {
  hextrail_configuration *bp = &bps[MI_SCREEN(mi)];

  if (gltrackball_event_handler (event, bp->trackball,
              MI_WIDTH (mi), MI_HEIGHT (mi), &bp->button_down_p)) return True;
#ifdef USE_SDL
  else if (event->type == SDL_EVENT_KEY_DOWN) {
    SDL_Keycode keysym = event->key.key;
    char c = (char)event->key.key;
#else
  else if (event->xany.type == KeyPress) {
    KeySym keysym;
    char c = 0;
    XLookupString (&event->xkey, &c, 1, &keysym, 0);
#endif
    printf("%s: c=%c (%d)\n", __func__, c, c);

    if (c == ' ' || c == '\t' || c == '\r' || c == '\n') ;
    else if (c == '>' || c == '.' || c == '+' || c == '=' ||
#ifdef USE_SDL
            keysym == SDLK_RIGHT || keysym == SDLK_UP || keysym == SDLK_PAGEDOWN
#else
            keysym == XK_Right || keysym == XK_Up || keysym == XK_Next
#endif
            )
      MI_COUNT(mi)++;
    else if (c == '<' || c == ',' || c == '-' || c == '_' ||
               c == '\010' || c == '\177' ||
#ifdef USE_SDL
            keysym == SDLK_LEFT || keysym == SDLK_DOWN || keysym == SDLK_PAGEUP
#else
            keysym == XK_Left || keysym == XK_Down || keysym == XK_Prior
#endif
            )
      MI_COUNT(mi)--;
#ifdef USE_SDL
    else if (event->type == SDL_EVENT_QUIT) ;
#else
    else if (screenhack_event_helper (MI_DISPLAY(mi), MI_WINDOW(mi), event)) ;
#endif
    else return False;

#ifndef USE_SDL
  RESET:
    reset_hextrail(mi);
    return True;
#endif
  }
#ifndef USE_SDL
  else if (screenhack_event_helper (MI_DISPLAY(mi), MI_WINDOW(mi), event))
    goto RESET; // TODO - why?
#endif

  return False;
}

ENTRYPOINT void init_hextrail (ModeInfo *mi) {
  hextrail_configuration *bp;

  MI_INIT (mi, bps);

  bp = &bps[MI_SCREEN(mi)];

#ifndef USE_SDL
  bp->glx_context = init_GL(mi);
#endif

  reshape_hextrail (mi, MI_WIDTH(mi), MI_HEIGHT(mi));

  {
    double spin_speed   = 0.002;
    double wander_speed = 0.003;
    double spin_accel   = 1.0;

    bp->rot = make_rotator (do_spin ? spin_speed : 0,
                            do_spin ? spin_speed : 0,
                            do_spin ? spin_speed : 0,
                            spin_accel,
                            do_wander ? wander_speed : 0,
                            False);
    bp->trackball = gltrackball_init (True);
  }


  /* Let's tilt the scene a little. */
  gltrackball_reset (bp->trackball,
                     -0.4 + frand(0.8),
                     -0.4 + frand(0.8));

  if (thickness < 0.05) thickness = 0.05;
  if (thickness > 0.5) thickness = 0.5;

  make_plane (mi);
  bp->state = FIRST;
  bp->fade_ratio = 1;
}


ENTRYPOINT void draw_hextrail (ModeInfo *mi) {
  hextrail_configuration *bp = &bps[MI_SCREEN(mi)];

#ifdef USE_SDL
  if (!bp->gl_context) return;
  SDL_GL_MakeCurrent(bp->window, bp->gl_context);
#else
  if (!bp->glx_context) return;
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);
  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *bp->glx_context);
#endif

  glShadeModel(GL_SMOOTH);

  glDisable(GL_DEPTH_TEST);
  glEnable(GL_NORMALIZE);
  glDisable(GL_CULL_FACE);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glPushMatrix ();

  {
    double x, y, z;
    get_position (bp->rot, &x, &y, &z, !bp->button_down_p);
    glTranslatef((x - 0.5) * 6,
                 (y - 0.5) * 6,
                 (z - 0.5) * 12);

    gltrackball_rotate (bp->trackball);

    get_rotation (bp->rot, &x, &y, &z, !bp->button_down_p);
    glRotatef (z * 360, 0.0, 0.0, 1.0);
  }

  mi->polygon_count = 0;

  {
    GLfloat s = 18;
    glScalef (s, s, s);
  }

  if (! bp->button_down_p) tick_hexagons (mi);
  else {
	  if (!bp->button_pressed) printf("Button pressed\n");
	  bp->button_pressed = True;
  }
  draw_hexagons (mi);

  glPopMatrix ();

  if (mi->fps_p) do_fps (mi);
  glFinish();

#ifdef USE_SDL
  SDL_GL_SwapWindow(bp->window);
#else
  glXSwapBuffers(dpy, window);
#endif
}


ENTRYPOINT void free_hextrail (ModeInfo *mi) {
  hextrail_configuration *bp = &bps[MI_SCREEN(mi)];

#ifdef USE_SDL
  if (!bp->gl_context) return;
#else
  if (!bp->glx_context) return;
  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *bp->glx_context);
  if (bp->trackball) gltrackball_free (bp->trackball);
  if (bp->rot) free_rotator (bp->rot);
#endif
  if (bp->colors) free (bp->colors);
  free (bp->hexagons);

#ifdef USE_SDL
  if (bp->gl_context)
    SDL_GL_DestroyContext(bp->gl_context);
  if (bp->window)
    SDL_DestroyWindow(bp->window);
  SDL_Quit();
#endif
}

XSCREENSAVER_MODULE ("HexTrail", hextrail)

#endif /* USE_GL */
