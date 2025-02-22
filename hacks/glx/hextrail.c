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
  int x, y;
  hexagon *neighbors[6]; // not technically needed...
  arm arms[6];
  int ccolor;
  state_t border_state;
  GLfloat border_ratio;
  Bool empty;
  int doing;
  Bool invis;
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
  Bool bug_found;

  int grid_w, grid_h;
  int x_offset, y_offset;
  hexagon *hexagons;
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

  for (y = 0; y < bp->grid_h; y++) for (x = 0; x < bp->grid_w; x++) {
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
  bp->x_offset = 0; bp->y_offset = 0;
  if (bp->button_pressed)
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
      h0->x = x; h0->y = y;
      h0->pos.x = (x - bp->grid_w/2) * w;
      if (y & 1) h0->pos.x += w / 2; // Stagger into hex arrangement
      h0->pos.y = (y - bp->grid_h/2) * h;
      h0->pos.z = 0;
      h0->border_state = EMPTY;
      h0->border_ratio = 0;
	  h0->empty = True;
      h0->doing = 0;

      h0->ccolor = random() % bp->ncolors;
    }
  }

  update_neighbors(mi);
}


static int add_arms (ModeInfo *mi, hexagon *h0) {
  hextrail_configuration *bp = &bps[MI_SCREEN(mi)];
  int i;
  int added = 0;
  int target = (random() % 4);

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
    hexagon *h1 = h0->neighbors[j];
    arm *a0 = &h0->arms[j];
    arm *a1;
    if (!h1) {
      //printf("pos=%d,%d No neighbour on arm %d\n", h0->x, h0->y, j);
      continue;			/* No neighboring cell */
    }
    if (!h1->empty) continue;	/* Occupado */
    if (a0->state != EMPTY) continue;		/* Arm already exists */

    a1 = &h1->arms[(j + 3) % 6];		/* Opposite arm */

    if (a1->state != EMPTY) abort();
    a0->state = OUT;
    a1->state = WAIT;
    a0->ratio = 0;
    a1->ratio = 0;
    a0->speed = 0.05 * speed * (0.8 + frand(1.0));
    a1->speed = a0->speed;

    if (h1->border_state == EMPTY) {
      h1->doing = -1;
      h1->border_state = IN;
	  h1->empty = False; h0->empty = False;

      /* Mostly keep the same color */
      h1->ccolor = h0->ccolor;
      if (! (random() % 5)) h1->ccolor = (h0->ccolor + 1) % bp->ncolors;
    }

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
             model, proj, viewport,
             &winX, &winY, &winZ);

  Bool is_visible = (winX >= viewport[0] && winX <= viewport[0] + viewport[2] &&
          winY >= viewport[1] && winY <= viewport[1] + viewport[3] &&
          winZ > 0 && winZ < 1);

  return !is_visible;
}

static void expand_plane(ModeInfo *mi, int direction) {
  hextrail_configuration *bp = &bps[MI_SCREEN(mi)];
  //if (bp->x_offset || bp->y_offset) return; // TODO - temp test
  printf("Expanding plane - count=%ld - before grid = %dx%d\n", MI_COUNT(mi), bp->grid_w, bp->grid_h);
  int old_grid_w = bp->grid_w; int old_grid_h = bp->grid_h;
  int new_grid_w = old_grid_w; int new_grid_h = old_grid_h;
  hexagon *old_hexagons = bp->hexagons;
  const int expansion = 1;
  int x, y;

  /* Increase grid size */
  if (direction & 8 || direction & 2) new_grid_w += expansion;
  if (direction & 4 || direction & 1) new_grid_h += expansion;

  /* Allocate new grid */
  hexagon *new_hexagons = (hexagon *)calloc(new_grid_w * new_grid_h, sizeof(hexagon));
  if (!new_hexagons) {
    fprintf(stderr, "Failed to allocate memory for expanded grid\n");
    return;
  }

  /* Calculate copy offsets */
  int x_offset = (direction & 8) ? expansion : 0;
  int y_offset = (direction & 4) ? expansion : 0;
  bp->x_offset += x_offset; bp->y_offset += y_offset;

  /* Copy existing hexagons with position adjustment */
  for (y = 0; y < old_grid_h; y++) for (x = 0; x < old_grid_w; x++) {
    hexagon *new_hex = &new_hexagons[(y + y_offset) * new_grid_w + (x + x_offset)];
    *new_hex = old_hexagons[y * old_grid_w + x];
  }

  /* Initialize new hexagons */
  GLfloat w = 1.0 / MI_COUNT(mi); GLfloat h = w * sqrt(3) / 2;

  for (y = 0; y < new_grid_h; y++) for (x = 0; x < new_grid_w; x++) {
    hexagon *h0 = &new_hexagons[y * new_grid_w + x];

    /* Skip existing hexagons */
    if (x >= x_offset && x < old_grid_w + x_offset &&
        y >= y_offset && y < old_grid_h + y_offset)
      continue;

    h0->pos.x = (x - new_grid_w/2) * w;
    h0->pos.y = (y - new_grid_h/2) * h;
    if (y & 1) h0->pos.x += w / 2;
    h0->pos.z = 0;
	h0->x = x - x_offset; h0->y = y - y_offset;
    h0->border_state = EMPTY;
    h0->border_ratio = 0;
	h0->empty = True;
    h0->doing = 0;
    h0->ccolor = random() % bp->ncolors;
    /*for (int i = 0; i < 6; i++) {
      h0->arms[i].state = EMPTY;
      h0->arms[i].ratio = 0;
      h0->arms[i].speed = 0;
      h0->neighbors[i] = NULL;
    }*/
  }

  bp->grid_w = new_grid_w; bp->grid_h = new_grid_h;
  printf("Expanding plane - after grid = %dx%d\n", bp->grid_w, bp->grid_h);
  bp->hexagons = new_hexagons;
  update_neighbors(mi);
  free(old_hexagons);
}

static void reset_hextrail(ModeInfo *mi) {
  hextrail_configuration *bp = &bps[MI_SCREEN(mi)];
  if (MI_COUNT(mi) < 1) MI_COUNT(mi) = 1;
  free (bp->hexagons);
  bp->hexagons = NULL;
  bp->state = FIRST;
  bp->fade_ratio = 1;
  make_plane (mi);
}

static void tick_hexagons (ModeInfo *mi) {
  hextrail_configuration *bp = &bps[MI_SCREEN(mi)];
  int i, j, doinga = 0, doingb = 0, ignorea = 0, ignoreb = 0;
  int8_t dir = 0;
  static int max_x = 0, max_y = 0, min_x = 0, min_y = 0;
  static int last_min_vx = 0, last_min_vy = 0, last_max_vx = 0, last_max_vy = 0;
  int min_vx = bp->grid_w, min_vy = bp->grid_h, max_vx = 0, max_vy = 0;

  if (!bp->button_pressed) {
    for (i = 0; i < bp->grid_w * bp->grid_h; i++) {
      hexagon *h0 = &bp->hexagons[i];
      Bool is_edge = (h0->x == 0 || h0->x == bp->grid_w - 1 ||
                     h0->y == 0 || h0->y == bp->grid_h - 1 );
      Bool invis = point_invis(h0);
      Bool debug = False;

      h0->invis = invis;

	  // Measure the drawn part we can see
	  if (!invis && (h0->border_state != EMPTY || !h0->empty)) {
        if (h0->x > max_vx) {
          max_vx = h0->x;
          if (h0->x > last_max_vx) {
            debug = True; last_max_vx = h0->x;
          }
        } else if (h0->x < min_vx) {
          min_vx = h0->x;
          if (h0->x < last_min_vx) {
            debug = True; last_min_vx = h0->x;
          }
        }
        if (h0->y > max_vy) {
          max_vy = h0->y;
          if (h0->y > last_max_vy) {
            debug = True; last_max_vy = h0->y;
          }
        } else if (h0->y < min_vy) {
          min_vy = h0->y;
          if (h0->y < last_min_vy) {
            debug = True; last_min_vy = h0->y;
          }
        }
	  } // Visible and something there

      // Try to debug why expansion not working
	  if (!h0->empty) {
		int posx = (int)(h0->pos.x * 1000); int posy = (int)(h0->pos.y * 1000);
        if (posx > max_x) {
          max_x = posx; debug = True;
        } else if (posx < min_x) {
          min_x = posx; debug = True;
        }
        if (posy > max_y) {
          max_y = posy; debug = True;
        } else if (posy < min_y) {
          min_y = posy; debug = True;
        }
	  }

	  if (h0->doing && is_edge && !invis) {
        // 1=vmax++, 2=hmax++, 4=vmin--, 8=hmin--
        if (h0->x == 0) dir |= 8;
        else if (h0->x == bp->grid_w - 1) dir |= 2;
        if (h0->y == 0) dir |= 4;
        else if (h0->y == bp->grid_h - 1) dir |= 1;
        // TODO - test if we can shift instead of expand
        //printf("pos=%d,%d Expanding plane %d edge=%d visible=%d arms=%d border=%d\n", h0->x, h0->y, dir, is_edge, !invis, h0->doing, h0->border_state != EMPTY);
        break;
      }

      if (debug)
        printf("pos=%d,%d vis=(%d-%d,%d-%d) (%d-%d,%d-%d) arms=%d border=%d edge=%d, visible=%d\n",
                h0->x, h0->y, last_min_vx, last_max_vx, last_min_vy, last_max_vy,
                min_x, max_x, min_y, max_y, h0->doing, h0->border_state != EMPTY, is_edge, !invis);
      // TODO use above values to work out if we can shift instead of expand plane

    } // For all hexagons
      //
    last_min_vx = min_vx; last_max_vx = max_vx; last_min_vy = min_vy; last_max_vy = max_vy;

    if (dir > 0) {
      expand_plane(mi, dir);
    }
  } // Button never pressed

  for (i = 0; i < bp->grid_w * bp->grid_h; i++) {
    hexagon *h0 = &bp->hexagons[i];

    Bool is_edge = (h0->x == 0 || h0->x == bp->grid_w - 1 ||
                   h0->y == 0 || h0->y == bp->grid_h - 1 );

    if (h0->doing) {
      doinga++;
      if (h0->invis) ignorea++;
    }

    if (h0->border_state != EMPTY) {
      doingb++;
      if (h0->invis) ignoreb++;
    }

    if (is_edge && h0->invis) continue;

    /* Enlarge any still-growing arms if active.  */
    for (j = 0; j < 6; j++) {
      arm *a0 = &h0->arms[j];
      switch (a0->state) {
        case OUT:
          if (a0->speed <= 0) {
            printf("a0->speed = %f\n", a0->speed);
            abort();
          }
          a0->ratio += a0->speed;
          if (a0->ratio >= 1) {
            /* Just finished growing from center to edge.
               Pass the baton to this waiting neighbor. */
            hexagon *h1 = h0->neighbors[j];
            arm *a1 = &h1->arms[(j + 3) % 6];
            if (a1->state != WAIT) {
              printf("H0 (%d,%d)'s arm=%d connecting to H1 (%d,%d)'s arm_state=%d\n", h0->x, h0->y, j, h1->x, h1->y, a1->state);
              abort();
            }
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
          if (a0->ratio >= 1) {
            /* Just finished growing from edge to center.
               Look for any available exits. */
            a0->state = DONE;
			//hexagon *h1 = h0->neighbors[(j + 3) % 6];
			hexagon *h1 = h0->neighbors[j];
			h1->doing--;
            a0->ratio = 1;
            add_arms(mi, h0);
          }
          break;
        case EMPTY: case WAIT: case DONE:
          break;
        default:
          printf("a0->state = %d\n", a0->state);
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
        printf("h0->border_state = %d\n", h0->border_state);
        abort(); break;
    }
  } // Loop through each hexagon

  /* Start a new cell growing.  */
  Bool try_new = False, started = False;
  if ((doinga - ignorea) <= 0) {
    for (i = 0; i < (bp->grid_w * bp->grid_h) / 3; i++) {
      hexagon *h0;
      int x, y;
      if (bp->state == FIRST) {
        x = bp->grid_w / 2; y = bp->grid_h / 2;
        bp->state = DRAW;
        bp->fade_ratio = 1;
        max_x = 0; max_y = 0; min_x = 0; min_y = 0;
        last_max_vx = 0; last_max_vy = 0; last_min_vx = bp->grid_w; last_min_vy = bp->grid_h;
        printf("New hextrail. vis=(%d-%d,%d-%d) (%d-%d,%d-%d)\n",
                last_min_vx, last_max_vx, last_min_vy, last_max_vy,
                min_x, max_x, min_y, max_y);
      } else {
        try_new = True;
        x = random() % bp->grid_w; // TODO - don't use random?
        y = random() % bp->grid_h;
      }
      h0 = &bp->hexagons[y * bp->grid_w + x];
      if (h0->empty && !h0->invis && add_arms(mi, h0)) {
        started = True;
        break;
      }
    } // Look for a suitable cell
  }

  if (try_new && (started || doinga != ignorea))
    printf("New cell: started=%d doing=%d ignored=%d\n", started, doinga, ignorea);

  if (!started && (doinga - ignorea) <= 0 && (doingb - ignoreb) < 2 && bp->state != FADE) {
    printf("Fade started. doinga=%d doingb=%d\n", doinga, doingb);
    bp->state = FADE;
    bp->fade_ratio = 1;

    for (i = 0; i < bp->grid_w * bp->grid_h; i++) {
      hexagon *h = &bp->hexagons[i];
      if (h->border_state == IN || h->border_state == WAIT)
        h->border_state = OUT;
    }
  } else if (bp->state == FADE) {
    bp->fade_ratio -= 0.01 * speed;
    if (bp->fade_ratio <= 0)
      reset_hextrail (mi);
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

  reset_hextrail (mi);
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
    get_position (bp->rot, &x, &y, &z, !bp->button_down_p && !bp->bug_found);
    glTranslatef((x - 0.5) * 6,
                 (y - 0.5) * 6,
                 (z - 0.5) * 12);

    gltrackball_rotate (bp->trackball);

    get_rotation (bp->rot, &x, &y, &z, !bp->button_down_p && !bp->bug_found);
    glRotatef (z * 360, 0.0, 0.0, 1.0);
  }

  mi->polygon_count = 0;

  {
    GLfloat s = 18;
    glScalef (s, s, s);
  }

  if (!bp->button_down_p && !bp->bug_found) tick_hexagons (mi);
  else if (bp->button_down_p) {
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
