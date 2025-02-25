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
#include "xlockmore.h"
#include "colors.h"
#include "normals.h"
#include "rotator.h"
#include "gltrackball.h"
#include <ctype.h>
#include <math.h>
#include <time.h>

#ifdef USE_GL /* whole file */


#define DEF_SPIN        "True"
#define DEF_WANDER      "True"
#define DEF_GLOW        "False"
#define DEF_NEON        "False"
#define DEF_EXPAND      "False"
#define DEF_SPEED       "1.0"
#define DEF_THICKNESS   "0.15"

#define BELLRAND(n) ((frand((n)) + frand((n)) + frand((n))) / 3)

typedef enum { EMPTY, IN, WAIT, OUT, DONE } state_t;

typedef struct {
  state_t state;
  GLfloat ratio;
  GLfloat speed;
} arm;

typedef struct hexagon {
  XYZ pos;		// TODO remove as can be derived from x and y
  int x, y;
  arm arms[6];
  int ccolor;
  state_t state;
  GLfloat ratio;
  int8_t doing;
  Bool invis;
} hexagon;

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
  Bool button_down_p, button_pressed;
  time_t now, pause_until, debug;

  hexagon **hexagons;   // Dynamic array of pointers to hexagons
  hexagon **hex_grid;   // Lookup table of hexagons
  int hexagon_count;    // Number of active hexagons
  int size, grid_w, grid_h;
  int x_offset, y_offset;
  enum { FIRST, DRAW, FADE } state;
  GLfloat fade_ratio;

  int ncolors;
} config;

static config *bps = NULL;

static Bool do_spin;
static GLfloat speed;
static Bool do_wander;
static Bool do_glow;
static Bool do_neon;
static Bool do_expand;
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
  { "-expand", ".expand", XrmoptionNoArg, "True" },
  { "+expand", ".expand", XrmoptionNoArg, "False" },
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
  {&do_expand, "expand", "Expand", DEF_EXPAND, t_Bool},
  {&speed,     "speed",  "Speed",  DEF_SPEED,  t_Float},
  {&thickness, "thickness", "Thickness", DEF_THICKNESS, t_Float},
};

ENTRYPOINT ModeSpecOpt hextrail_opts = {countof(opts), opts, countof(vars), vars, NULL};

static hexagon *do_hexagon(config *bp, int x, int y) {
  // Returns or creates a hexton at co-ords
  int gx = x + bp->grid_w/2 + bp->x_offset;
  int gy = y + bp->grid_h/2 + bp->y_offset;
  if (gx < 0 || gx >= bp->grid_w || gy < 0 || gy >= bp->grid_h) {
	  printf("%s: Out of bounds\n", __func__);
	  // TODO - we could extend_grid here
      return NULL;
  }
  hexagon *h0 = bp->hex_grid[gy * bp->grid_w + gx];
  // We found an existing hexagon, so return it.
  if (h0) return h0;

  // TODO - is this ok to realloc 1 at a time like this?
  hexagon **new_hexagons = (hexagon **)realloc(bp->hexagons, (bp->hexagon_count+1) * sizeof(hexagon *));
  if (!new_hexagons) {
	fprintf(stderr, "%s: Reallocate failed\n", __func__);
	return NULL;
  }

  h0 = (hexagon *)malloc(sizeof(hexagon));
  if (!h0) {
	printf("%s: Malloc failed\n", __func__);
	// TODO - if this fails, do we need to free the memory from the realloc above?
	return NULL;
  }
  memset (h0, 0, sizeof(hexagon));

  bp->hexagons = new_hexagons;
  h0->x = x; h0->y = y;
  GLfloat w = 2.0 / bp->size; // TODO - to delete
  GLfloat h = w * sqrt(3) / 2; //       and below
  h0->pos.x = x * w;
  if (y & 1) h0->pos.x += w / 2; // Stagger into hex arrangement
  h0->pos.y = y * h;
  h0->pos.z = 0;
  h0->state = EMPTY;
  h0->ratio = 0;
  h0->doing = 0;
  //h0->ccolor = random() & bp->ncolors;
  /*for (int i = 0; i < 6; i++) {
    h0->arms[i].state = EMPTY;
    h0->arms[i].ratio = 0;
    h0->arms[i].speed = 0;
    h0->neighbors[i] = NULL;
  }*/

  // Add to lookup table
  // TODO - add code to derive idx for the lookup table entry
  bp->hex_grid[gy * bp->grid_w + gx] = h0;
  bp->hexagons[bp->hexagon_count++] = h0;

  return h0;
}

static hexagon *neighbor(config *bp, hexagon *h0, int j) {
  // First value is arm, 2nd value is XE, XO, Y
  //   0,0   1,0   2,0   3,0   4,0               5   0
  //      0,1   1,1   2,1   3,1   4,1
  //   0,2   1,2   2,2   3,2   4,             4  arms   1
  //      0,3   1,3   2,3   3,3   4,3
  //   0,4   1,4   2,4   3,4   4,4               3   2
  const int offset[6][3] = {
	  {0, 1, -1}, {1, 1, 0}, {0, 1, 1}, {-1, 0, 1}, {-1, -1, 0}, {-1, 0, -1}
  };
  int x = h0->x + offset[j][h0->y & 1], y = h0->y + offset[j][2];
  return do_hexagon(bp, x, y);
}

static int add_arms (config *bp, hexagon *h0) {
  int i;
  int added = 0;
  int target = 1 + (random() % 5); /* Aim for 1-5 arms */

  int idx[6];				/* Traverse in random order */
  for (i = 0; i < 6; i++) idx[i] = i;
  for (i = 0; i < 6; i++) {
    int j = random() % 6;
    int swap = idx[j];
    idx[j] = idx[i];
    idx[i] = swap;
  }

  for (i = 0; i < 6; i++) {
    int j = idx[i];
    arm *a0 = &h0->arms[j];
    if (a0->state != EMPTY) continue;	/* Arm already exists */
    hexagon *h1 = neighbor(bp, h0, j);
    if (!h1) {
      //printf("pos=%d,%d No neighbour on arm %d\n", h0->x, h0->y, j);
	  // TODO - need to create this offsets array
      continue;		/* No neighboring cell */
    }
    if (h1->state != EMPTY) continue;	/* Occupado */
    if (a0->state != EMPTY) continue;   /* Arm already exists */

    arm *a1 = &h1->arms[(j + 3) % 6];	/* Opposite arm */

    if (a1->state != EMPTY) {
      printf("H1 (%d,%d) empty=%d arm[%d].state=%d\n",
             h1->x, h1->y, h1->state == EMPTY, (j+3)%6, a1->state);
      bp->pause_until = bp->now + 3;
      continue;
    }
    a0->state = OUT;
    a1->state = WAIT;
    a0->ratio = 0;
    a1->ratio = 0;
    a0->speed = 0.05 * speed * (0.8 + frand(1.0));
    a1->speed = a0->speed;

    if (h1->state == EMPTY) {
      h1->state = IN;

      /* Mostly keep the same color */
      if (! (random() % 5)) h1->ccolor = (h0->ccolor + 1) % bp->ncolors;
      else h1->ccolor = h0->ccolor;
    } else
      printf("H0 (%d,%d) arm%d out to H1 (%d,%d)->state=%d\n",
h0->x, h0->y, j, h1->x, h1->y, h1->state);
      // TODO - find out which arm of H1 is already WAITing and the ratio of the OUT arm heading to it.

    added++;
    if (added >= target) break;
  }
  h0->doing = added;

  return added;
}

/* Check if a point is within the visible frustum */
static Bool point_invis(hexagon *h0) {
  XYZ point = h0->pos;
  GLdouble model[16], proj[16];
  GLint viewport[4];
  GLdouble winX, winY, winZ;

  /* Get current matrices and viewport */
  glGetDoublev(GL_MODELVIEW_MATRIX, model);
  glGetDoublev(GL_PROJECTION_MATRIX, proj);
  glGetIntegerv(GL_VIEWPORT, viewport);

  /* Project point to screen coordinates */
  gluProject((GLdouble)point.x, (GLdouble)point.y, (GLdouble)point.z,
             model, proj, viewport, &winX, &winY, &winZ);

  Bool is_visible = (winX >= viewport[0] && winX <= viewport[0] + viewport[2] &&
          winY >= viewport[1] && winY <= viewport[1] + viewport[3] &&
          winZ > 0 && winZ < 1);

  /*if (h0->state != EMPTY && h0->invis == is_visible && is_visible)
    printf("Visibility flip at (%d,%d): win=(%.1f, %.1f, %.1f), viewport=(%d,%d,%d,%d)\n",
             h0->x, h0->y, winX, winY, winZ, viewport[0], viewport[1], viewport[2], viewport[3]);*/

  return !is_visible;
}

// TODO - is it possible to use 20-bit or 24-bit or 32-bit pointers rather than wasting 64-bits for each one?
static void expand_grid(config *bp, int direction) {
  int new_grid_w = bp->grid_w, new_grid_h = bp->grid_h;

  /* Increase grid size */
  if (direction & 8) new_grid_w += 2;
  if (direction & 2) new_grid_w++;
  if (direction & 4 || direction & 1) new_grid_h++;
  /* Calculate copy offsets */
  int x_offset = (direction & 8) ? 2 : 0;
  int y_offset = (direction & 4) ? 1 : 0;

  bp->debug = bp->now;
  printf("Expanding grid: before = %d-%d,%d-%d after = %d-%d,%d-%d\n",
          -bp->x_offset, -bp->y_offset, bp->grid_w - bp->x_offset, bp->grid_h - bp->y_offset,
          -bp->x_offset - x_offset, -bp->y_offset - y_offset,
          new_grid_w - bp->x_offset - x_offset, new_grid_h - bp->y_offset - y_offset);

  bp->x_offset += x_offset; bp->y_offset += y_offset;

  /* Allocate new grid */
  hexagon **new_grid = (hexagon **)calloc(new_grid_w * new_grid_h, sizeof(hexagon *));
  // TODO - do we need to memset to ensure zeros?
  if (!new_grid) {
    fprintf(stderr, "Failed to allocate memory for expanded grid\n");
    return;
  }

  /* Copy existing hexagon pointers with position adjustment */
  int x, y;
  for (y = 0; y < bp->grid_h; y++) for (x = 0; x < bp->grid_w; x++) {
	// TODO below should I be using hexagon ** ?
    hexagon *old_idx = bp->hex_grid[y * bp->grid_w + x];
    hexagon *new_idx = new_grid[(y + y_offset) * new_grid_w + x + x_offset];
    *new_idx = *old_idx;
  }

  bp->grid_w = new_grid_w; bp->grid_h = new_grid_h;
  free(bp->hex_grid);
  bp->hex_grid = new_grid;
}

static void reset_hextrail(config *bp) {
  // TODO - is this a good idea each time? Or better to zero the entries?
  for (int i = 0; i < bp->hexagon_count; i++) free(bp->hexagons[i]);
  // TODO - do we need to free(bp->hexagons)) also?
  //free (bp->hexagons);
  //bp->hexagons = NULL;
  // TODO - should we perhaps free (bp->hex_grid) also? Or zero it?
  bp->hexagon_count = 0;
  bp->state = FIRST;
  bp->fade_ratio = 1;
  bp->ncolors = 8;
  bp->x_offset = 0; bp->y_offset = 0;
  if (do_expand && bp->button_pressed) printf("Setting button_pressed to False\n");
  bp->button_pressed = False;
  if (!bp->colors) {
#ifdef USE_SDL
    bp->colors = (SDL_Color *) calloc(bp->ncolors, sizeof(SDL_Color));
    make_smooth_colormap(bp->colors, &bp->ncolors, False, 0, False);
#else
    bp->colors = (XColor *) calloc(bp->ncolors, sizeof(XColor));
    make_smooth_colormap (0, 0, 0, bp->colors, &bp->ncolors, False, 0, False);
#endif
  } else
    printf("Didn't smooth. ncolors = %d\n", bp->ncolors);

  bp->grid_w = bp->size; bp->grid_h = bp->size;
  //make_plane (bp);
}

static void tick_hexagons (config *bp) {
  int i, j, doinga = 0, doingb = 0, ignorea = 0, ignoreb = 0;
  int empty = 0, vempty = 0; // TODO use this prior to fade
  int8_t dir = 0;
  static int min_x = 0, min_y = 0, max_x = 0, max_y = 0;
  static int min_vx = 0, min_vy = 0, max_vx = 0, max_vy = 0;
  int this_min_vx = 0, this_min_vy = 0, this_max_vx = 0, this_max_vy = 0;

  for (i = 0; i < bp->hexagon_count; i++) {
    hexagon *h0 = bp->hexagons[i];
    h0->invis = (do_expand && !bp->button_pressed && point_invis(h0));

    int adj_x = h0->x + bp->size/2 + bp->x_offset;
    int adj_y = h0->y + bp->size/2 + bp->y_offset;

    Bool debug = False;

    // Measure the drawn part we can see
    if (!h0->invis && h0->state != EMPTY) {
      if (h0->x > this_max_vx) {
        this_max_vx = h0->x;
        if (h0->x > max_vx) {
          debug = True; max_vx = h0->x;
        }
      }
      if (h0->x < this_min_vx) {
        this_min_vx = h0->x;
        if (h0->x < min_vx) {
        debug = True; min_vx = h0->x;
        }
      }
      if (h0->y > this_max_vy) {
        this_max_vy = h0->y;
        if (h0->y > max_vy) {
          debug = True; max_vy = h0->y;
        }
      }
      if (h0->y < this_min_vy) {
        this_min_vy = h0->y;
        if (h0->y < min_vy) {
          debug = True; min_vy = h0->y;
        }
      }
    } // Visible and non-empty

    int8_t edge = 0;

    // Logic to expand the X,Y lookup table
    if (h0->doing && !h0->invis) {
      // 1=vmax++, 2=hmax++, 4=vmin--, 8=hmin--
      if (adj_x == 0) edge |= 8;
      else if (adj_x == bp->grid_w - 1) edge |= 2;
      if (adj_y == 0) edge |= 4;
      else if (adj_y == bp->grid_h - 1) edge |= 1;
      dir |= edge;
      // TODO - test if we can shift instead of expand
    }

    if (debug) {
      printf("pos=%d,%d vis=(%d-%d,%d-%d) (%d-%d,%d-%d) arms=%d border=%d edge=%d, invis=%d\n",
              h0->x, h0->y, min_vx, max_vx, min_vy, max_vy,
              min_x, max_x, min_y, max_y, h0->doing, h0->state, edge, h0->invis);
      bp->debug = bp->now;
    }
    // TODO use above values to work out if we can shift instead of expand plane

    if (h0->doing) {
      doinga++;
      if (h0->invis) ignorea++;
    }

    if (h0->state == EMPTY) {
      empty++;
      if (!h0->invis) vempty++;
    } else if (h0->state != DONE) {
      doingb++;
      if (h0->invis) ignoreb++;
    }

    // TODO - change point_invis to return a value based on how
    // far off-screen. 1 for just-off. 2 for +5% off, 3 for +10%
    // e.g. if (h0->invis > 1) continue;
    if (do_expand && edge && h0->invis) continue;

    /* Enlarge any still-growing arms if active.  */
    for (j = 0; j < 6; j++) {
      arm *a0 = &h0->arms[j];
      switch (a0->state) {
        case OUT:
          a0->ratio += a0->speed;
          if (a0->ratio >= 1) {
            /* Just finished growing from center to edge.
               Pass the baton to this waiting neighbor. */
            hexagon *h1 = neighbor(bp, h0, j);
            arm *a1 = &h1->arms[(j + 3) % 6];
            if (a1->state != WAIT) {
              printf("H0 (%d,%d)'s arm=%d connecting to H1 (%d,%d)'s arm_state=%d arm_ratio=%.1f\n", h0->x, h0->y, j, h1->x, h1->y, a1->state, a1->ratio);
              bp->pause_until = bp->now + 3;
              a0->speed = -a0->speed;
              a1->state = OUT;
              if (a1->speed > 0) a1->speed = -a1->speed;
            } else {
              a0->state = DONE;
              a0->ratio = 1;
              a1->state = IN;
              a1->ratio = 0;
              a1->speed = a0->speed;
            }
          } else if (a0->ratio <= 0) {
            /* Just finished retreating back to center */
            a0->state = DONE;
            a0->ratio = 0;
          }
          break;
        case IN:
          if (a0->speed <= 0) abort();
          a0->ratio += a0->speed;
          if (a0->ratio >= 1) {
            /* Just finished growing from edge to center.
               Look for any available exits. */
            a0->state = DONE;
            //hexagon *h1 = h0->neighbors[(j + 3) % 6];
            hexagon *h1 = neighbor(bp, h0, j);
            h1->doing--;
            a0->ratio = 1;
            add_arms(bp, h0);
          }
          break;
        case EMPTY: case WAIT: case DONE:
          break;
        default:
          printf("a0->state = %d\n", a0->state);
          abort(); break;
      }
    } // 6 arms

    switch (h0->state) {
      case IN:
        h0->ratio += 0.05 * speed;
        if (h0->ratio >= 1) {
          h0->ratio = 1;
          h0->state = WAIT;
        }
        break;
      case OUT:
        h0->ratio -= 0.05 * speed;
        if (h0->ratio <= 0) {
          h0->ratio = 0;
          h0->state = DONE;
        }
      case WAIT:
        if (! (random() % 50)) h0->state = OUT;
        break;
      case EMPTY:
      case DONE:
        break;
      default:
        printf("h0->state = %d\n", h0->state);
        abort(); break;
    }
  } // Loop through each hexagon

  min_vx = this_min_vx; max_vx = this_max_vx; min_vy = this_min_vy; max_vy = this_max_vy;

  if (dir && do_expand) expand_grid(bp, dir);

  if (bp->now > bp->debug) {
    printf("doinga=%d ignorea=%d doingb=%d ignoreb=%d vempty=%d empty=%d\n",
        doinga, ignorea, doingb, ignoreb, vempty, empty);
    bp->debug = bp->now;
  }

  /* Start a new cell growing.  */
  Bool try_new = False, started = False;
  if ((doinga - ignorea) <= 0) {
    for (i = 0; i < (bp->grid_w * bp->grid_h) / 3; i++) {
      int x = 0, y = 0;
      if (bp->state == FIRST) {
        bp->state = DRAW;
        bp->fade_ratio = 1; // TODO what is this?
        min_vx = 0; max_vx = 0; min_vy = 0; max_vy = 0;
        min_x = 0; max_x = 0; min_y = 0; max_y = 0;
        printf("New hextrail. vis=(%d-%d,%d-%d) (%d-%d,%d-%d)\n",
                min_vx, max_vx, min_vy, max_vy,
                min_x, max_x, min_y, max_y);
      } else {
        try_new = True;
        x = (random() % bp->grid_w) - bp->size;
        y = (random() % bp->grid_h) - bp->size;
      }
      hexagon *h0 = do_hexagon(bp, x, y);
      if (h0->state == EMPTY && !h0->invis && add_arms(bp, h0)) {
        h0->ccolor = random() % bp->ncolors;
        h0->state = DONE;
        started = True;
        if (try_new) bp->pause_until = bp->now + 5;
        break;
      }
    } // Look for a suitable cell
  }

  if (try_new && (started || doinga != ignorea))
    printf("New cell: started=%d doinga=%d ignorea=%d doingb=%d ignoreb=%d\n",
            started, doinga, ignorea, doingb, ignoreb);

  if (!started && (doinga - ignorea) < 1 && (doingb - ignoreb) < 1 && bp->state != FADE) {
    printf("Fade started. doinga=%d doingb=%d ignorea=%d ignoreb=%d\n",
            doinga, doingb, ignorea, ignoreb);
    bp->state = FADE;
    bp->fade_ratio = 1;

    for (i = 0; i < bp->hexagon_count; i++) {
      hexagon *h = bp->hexagons[i];
      if (h->state == IN || h->state == WAIT)
        h->state = OUT;
    }
  } else if (bp->state == FADE) {
    bp->fade_ratio -= 0.01 * speed;
    if (bp->fade_ratio <= 0)
      reset_hextrail (bp);
  }
}

static void draw_glow_point(XYZ p, GLfloat size, GLfloat scale,
        GLfloat *color, GLfloat alpha, Bool neon) {
  glBegin(GL_TRIANGLE_FAN);
  if (!neon) glColor4f(color[0], color[1], color[2], alpha);
  glVertex3f(p.x, p.y, p.z);
  for (int g = 0; g <= 16; g++) {
  //for (int g = 0; g <= 8; g++) {
    float angle = g * M_PI / 8;
    //float angle = g * M_PI / 4;
    float x = p.x + cos(angle) * size * scale;
    float y = p.y + sin(angle) * size * scale;
    glVertex3f(x, y, p.z);
  }
  glEnd();
}

static void draw_hexagons (ModeInfo *mi) {
  config *bp = &bps[MI_SCREEN(mi)];
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

  for (i = 0; i < bp->hexagon_count; i++) {
    hexagon *h = bp->hexagons[i];
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

      if (h->state != EMPTY && h->state != DONE) {
        GLfloat color1[3];
        memcpy (color1, color, sizeof(color1));
        color1[0] *= h->ratio;
        color1[1] *= h->ratio;
        color1[2] *= h->ratio;

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
        HEXAGON_COLOR (ncolor, neighbor(bp, h, j));
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

        //if (! h->neighbors[j]) abort();  /* arm/neighbor mismatch */

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
            draw_glow_point(p[0], size, glow_scale, glow_color, glow_alpha, do_neon);

            /* End point glow */
            draw_glow_point(p[3], size, glow_scale, glow_color, glow_alpha, do_neon);

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
        } // Glow or neon

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
      } // arm is IN, OUT or DONE

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


/* Window management, etc */
ENTRYPOINT void
reshape_hextrail (ModeInfo *mi, int width, int height) {
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
  gluLookAt( 0.0, 0.0, 30.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0);

  {
    GLfloat s = (MI_WIDTH(mi) < MI_HEIGHT(mi)
                 ? (MI_WIDTH(mi) / (GLfloat) MI_HEIGHT(mi))
                 : 1);
    glScalef (s, s, s);
  }

  glClear(GL_COLOR_BUFFER_BIT);
}

ENTRYPOINT Bool hextrail_handle_event (ModeInfo *mi,
#ifdef USE_SDL
        SDL_Event *event
#else
        XEvent *event
#endif
        ) {
  config *bp = &bps[MI_SCREEN(mi)];

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
            ) {
      MI_COUNT(mi)--;
      if (MI_COUNT(mi) < 1) MI_COUNT(mi) = 1;
    }
#ifdef USE_SDL
    else if (event->type == SDL_EVENT_QUIT) ;
#else
    else if (screenhack_event_helper (MI_DISPLAY(mi), MI_WINDOW(mi), event)) ;
#endif
    else return False;

#ifndef USE_SDL
  RESET:
    reset_hextrail(bp);
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
  config *bp;
  MI_INIT (mi, bps);
  bp = &bps[MI_SCREEN(mi)];

#ifndef USE_SDL
  bp->glx_context = init_GL(mi);
#endif

  reshape_hextrail (mi, MI_WIDTH(mi), MI_HEIGHT(mi));

  bp->size = MI_COUNT(mi) * 2 + 1;
  bp->rot = make_rotator(do_spin ? 0.002 : 0,
                         do_spin ? 0.002 : 0,
                         do_spin ? 0.002 : 0,
                         1.0, // spin_accel
                         do_wander ? 0.003 : 0,
                         False);
  bp->trackball = gltrackball_init(True);
  gltrackball_reset(bp->trackball, -0.4 + frand(0.8),
                     -0.4 + frand(0.8));

  if (thickness < 0.05) thickness = 0.05;
  if (thickness > 0.5) thickness = 0.5;

  bp->hex_grid = (hexagon **)calloc(bp->size * bp->size, sizeof(hexagon *));

  reset_hextrail (bp);
}


ENTRYPOINT void draw_hextrail (ModeInfo *mi) {
  config *bp = &bps[MI_SCREEN(mi)];

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
    glTranslatef((x - 0.5) * 6, (y - 0.5) * 6, (z - 0.5) * 12);

    gltrackball_rotate (bp->trackball);

    get_rotation (bp->rot, &x, &y, &z, !bp->button_down_p);
    glRotatef (z * 360, 0.0, 0.0, 1.0);
  }

  mi->polygon_count = 0;

  {
    GLfloat s = 18;
    glScalef (s, s, s);
  }

  bp->now = time(NULL);
  if (bp->pause_until < bp->now) tick_hexagons (bp);
  if (bp->button_down_p) {
    if (do_expand && !bp->button_pressed) printf("Button pressed\n");
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
  config *bp = &bps[MI_SCREEN(mi)];

#ifdef USE_SDL
  if (!bp->gl_context) return;
#else
  if (!bp->glx_context) return;
  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *bp->glx_context);
  if (bp->trackball) gltrackball_free (bp->trackball);
  if (bp->rot) free_rotator (bp->rot);
#endif
  if (bp->colors) free (bp->colors);
  for (int i = 0; i < bp->hexagon_count; i++) free(bp->hexagons[i]);
  free (bp->hexagons);

#ifdef USE_SDL
  if (bp->gl_context) SDL_GL_DestroyContext(bp->gl_context);
  if (bp->window) SDL_DestroyWindow(bp->window);
  SDL_Quit();
#endif
}

XSCREENSAVER_MODULE ("HexTrail", hextrail)

#endif /* USE_GL */
