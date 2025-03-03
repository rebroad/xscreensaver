/* hextrail, Copyright (c) 2022 Jamie Zawinski <jwz@jwz.org>
 *
 * low-priority TODOs:-
 = Collect all vertices into a dynamic array, then upload to a 1 or more VBOs - also fix glow effects.
 = Don't draw borders when the thickness of the line would be <1 pixel
 = hextrail - the end-points - draw these more slowly, like a decelleration (due to pressure).
 = need an option to make it fill all active displays (wasn't -root supposed to do this?)
 = Measure average tick time (per second) FPS=TPS?
 = Enable ability to click a hexagon and get info
 = Explore using the Z dimension - some undulation or ripple effect perhaps?
 */

#define DEFAULTS "*delay: 30000\n" \
                 "*showFPS: False\n" \
                 "*wireframe: False\n" \
                 "*count: 20\n" \
                 "*suppressRotationAnimation: True\n"

# define release_hextrail 0

#ifdef USE_SDL
#include <SDL3/SDL.h>
//#include <SDL3/SDL_keycode.h>
#define Bool int
#ifdef _Win32
#include <windows.h>
#endif
#else
#endif
#include <GL/gl.h>
#include <GL/glu.h>
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
  int8_t x, y; // N=147 to keep it to 8 bits
  arm arms[6];
  uint8_t ccolor;
  state_t state;
  GLfloat ratio;
  int8_t doing;
  Bool invis;
} hexagon;

#define HEXAGON_CHUNK_SIZE 1000

typedef struct {
  hexagon *chunk[HEXAGON_CHUNK_SIZE];
  int used;
} hex_chunk;

typedef struct {
#ifdef USE_SDL
  SDL_Window *window;
  SDL_GLContext gl_context;
  SDL_Color *colors;
#else
  GLXContext *glx_context;
  XColor *colors;
#endif
  GLdouble model[16], proj[16];
  GLint viewport[4];
  rotator *rot;
  trackball_state *trackball;
  Bool button_down_p;
  time_t now, pause_until, debug;

  hex_chunk **chunks;
  uint16_t *hex_grid;
  int chunk_count;
  int total_hexagons;
  int hexagon_capacity;
  int size;
  enum { FIRST, DRAW, FADE } state;
  GLfloat fade_ratio;

  int ncolors;
} config;

static config *bps = NULL;

static Bool do_spin, do_wander, do_glow, do_neon, do_expand;
static GLfloat speed, thickness;
static int8_t draw_invis = 1;
static Bool pausing = False, pause_fade = False, hexes_on = False;

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

#define MAX_N 147 // To fit within 16bit
#define erm(n) ((n) * ((n) + 1) / 2)
int q = 0, qq = 0, N = 0;

static void init_hex_grid_lookup(void);
void init_hex_grid_lookup() {
	q = (N - 1) / 2;
	qq = q * 2;
}

static uint16_t xy_to_index(int, int);
uint16_t xy_to_index(int x, int y) {
    if (abs(x) > q || abs(y) > q || abs(x-y) > q) return 0;
    int32_t index;
	if (y < 1) index = x + qq + erm(y + N) - erm(q - 1) + 1;
	else index = x + y + qq + erm(y + N) - erm(q - 1) + 1;

	return (uint16_t)index;
}

/*
static uint16_t xy_to_index(int, int);
uint16_t xy_to_index(int x, int y) {
    if (abs(x) > N || abs(y) > N || abs(x + y) > N) return 0;
	int32_t index;
	if (y < 0) {
		int abs_y = -y;
		int x_min = -N + abs_y;
		int num_before = (y + N) * (N - 1 + y) / 2;  // Adjusted cumulative sum
		index = num_before + (x = x_min) + 1;
	} else {
		int neg_rows = erm(N); // 10878 for N=147
		int zero_row = 2 * N + 1; // 295
		int pos_rows = y > 0 ? (y * (2 * N + 1) - erm(y - 1)): 0;
		int x_min = -N;
		index = neg_rows + zero_row + pos_rows + (x - x_min) + 1;
	}

    return (uint16_t)index;
}*/

static hexagon *do_hexagon(config *bp, int px, int py, int x, int y) {
  int id = xy_to_index(x, y);
  if (!id) {
    static time_t debug = 0;
    if (debug != bp->now) {
      printf("%s: Out of bounds. id=%d coords=%d,%d\n", __func__, id, x, y);
      debug = bp->now;
    }
    return NULL;
  }

  int idx = bp->hex_grid[id];
  if (idx) { // Zero means empty
    // We found an existing hexagon, so return it.
    int i = (idx-1) / HEXAGON_CHUNK_SIZE;
    int k = (idx-1) % HEXAGON_CHUNK_SIZE;
	if (i >= bp->chunk_count) {
	  printf("%s: Invalid chunk access (%d,%d)->(%d,%d) id=%d idx=%d ++i=%d/%d k=%d\n",
			 __func__, px, py, x, y, id, idx, i, bp->chunk_count, k);
	  return NULL;
	}
	if (k >= bp->chunks[i]->used) {
	  printf("%s: Invalid chunk access id=%d idx=%d i=%d/%d ++k=%d/%d\n",
			 __func__, id, idx, i, bp->chunk_count, k, bp->chunks[i]->used);
	  return NULL;
	}
	hexagon *h = bp->chunks[i]->chunk[k];
	if (h)
      ;//printf("ID for (%d,%d)=%d hex_grid[%d]=%d (%d,%d)\n", x, y, id, id, idx, h->x, h->y);
	else
      ;//printf("ID for (%d,%d)=%d hex_grid[%d]=%d (NULL)\n", x, y, id, id, idx);
	return h;
  }
  //printf("ID for (%d,%d)=%d hex_grid[%d]=%d\n", x, y, id, id, idx);

  if (bp->total_hexagons >= bp->chunk_count * HEXAGON_CHUNK_SIZE) {
    hex_chunk **new_chunks = realloc(bp->chunks, (bp->chunk_count+1) * sizeof(hex_chunk *));
    if (!new_chunks) {
      fprintf(stderr, "Failed to allocate new chunk array\n");
      return NULL;
    }
    bp->chunks = new_chunks;
    bp->chunks[bp->chunk_count] = calloc(1, sizeof(hex_chunk));
    if (!bp->chunks[bp->chunk_count]) {
      fprintf(stderr, "Failed to allocate chunk\n");
      return NULL;
    }
    bp->chunks[bp->chunk_count]->used = 0;
    bp->chunk_count++;
  }

  hexagon *h0 = calloc(1, sizeof(hexagon));
  if (!h0) {
    printf("%s: Malloc failed\n", __func__);
    return NULL;
  }
  hex_chunk *current = bp->chunks[bp->chunk_count - 1];
  current->chunk[bp->total_hexagons % HEXAGON_CHUNK_SIZE] = h0;
  current->used++;
  bp->total_hexagons++;
  bp->hex_grid[id] = bp->total_hexagons;
  h0->x = x; h0->y = y;
  printf("New hex at %d,%d (parent %d,%d)\n", x, y, px, py);

  return h0;
}

static hexagon *neighbor(config *bp, hexagon *h0, int j) {
  // First value is arm, 2nd value is X, 3rd is Y
  //   0,0   1,0   2,0   3,0   4,0               5   0
  //      1,1   2,1   3,1   4,1   5,1
  //   1,2   2,2   3,2   4,2   5,2            4  arms   1
  //      2,3   3,3   4,3   5,3   6,3
  //   2,4   3,4   4,4   5,4   6,4               3   2
  static const int offset[6][2] = {
      {0, -1}, {1, 0}, {1, 1}, {0, 1}, {-1, 0}, {-1, -1}
  };
  int x = h0->x + offset[j][0], y = h0->y + offset[j][1];
  return do_hexagon(bp, h0->x, h0->y, x, y);
}

static int add_arms(config *bp, hexagon *h0) {
  int i;
  int added = 0;
  int target = (random() % 4); /* Aim for 1-5 arms */

  int idx[6] = {0, 1, 2, 3, 4, 5};
  for (i = 0; i < 6; i++) {
    int j = random() % (6 - i);
    int swap = idx[j];
    idx[j] = idx[i];
    idx[i] = swap;
  }

  for (i = 0; i < 6; i++) {
    int j = idx[i];
    arm *a0 = &h0->arms[j];
    if (a0->state != EMPTY) continue;	/* Arm already exists */
    hexagon *h1 = neighbor(bp, h0, j);
    if (!h1) continue;		            /* No neighboring cell */
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

    added++;
    if (added >= target) break;
  }
  h0->doing = added;

  return added;
}

/* Check if a hexagon is within the visible frustum using bounding circle test */
static Bool hex_invis(config *bp, int x, int y, int *sx, int *sy) {
  GLfloat wid = 2.0 / bp->size;
  GLfloat hgt = wid * sqrt(3) / 2;
  XYZ pos = { x * wid - y * wid / 2, y * hgt, 0 };
  GLdouble winX, winY, winZ;

  /* Project point to screen coordinates */
  gluProject((GLdouble)pos.x, (GLdouble)pos.y, (GLdouble)pos.z,
             bp->model, bp->proj, bp->viewport, &winX, &winY, &winZ);

  if (sx) *sx = winX;
  if (sy) *sy = winY;

  static time_t debug = 0;
  if (debug != bp->now) {
      printf("%s: winX=%d, winY=%d, winZ=%.1f vp=%d,%d\n", __func__,
              (int)winX, (int)winY, winZ, bp->viewport[2], bp->viewport[3]);
      debug = bp->now;
  }

  if (winZ <= 0 || winZ >= 1) return 2;  // Far off in Z

  /* Calculate bounding circle radius in screen space */
  /* Use hexagon's width (wid) as the diameter, project a point that distance away */
  XYZ edge_posx = pos;
  XYZ edge_posy = pos;
  edge_posx.x += wid/2;
  edge_posy.y += hgt/2;

  GLdouble edge_xx, edge_xy, edge_yx, edge_yy, edge_z;
  gluProject((GLdouble)edge_posx.x, (GLdouble)edge_posx.y, (GLdouble)edge_posx.z,
             bp->model, bp->proj, bp->viewport, &edge_xx, &edge_xy, &edge_z);
  gluProject((GLdouble)edge_posy.x, (GLdouble)edge_posy.y, (GLdouble)edge_posy.z,
             bp->model, bp->proj, bp->viewport, &edge_yx, &edge_yy, &edge_z);

  /* Calculate radius in screen space (accounting for perspective projection) */
  GLdouble xx_diff = edge_xx - winX, xy_diff = edge_xy - winY;
  GLdouble yx_diff = edge_yx - winX, yy_diff = edge_yy - winY;
  GLdouble radiusx = sqrt(xx_diff*xx_diff + xy_diff*xy_diff);
  GLdouble radiusy = sqrt(yx_diff*yx_diff + yy_diff*yy_diff);

  // And now we take both radiuses and work out the maximum it could in reality.
  GLdouble radius = (radiusx > radiusy) ? radiusx : radiusy;

  if (winX + radius < bp->viewport[0] || winX - radius > bp->viewport[0] + bp->viewport[2] ||
      winY + radius < bp->viewport[1] || winY - radius > bp->viewport[1] + bp->viewport[3])
      return 2; // Fully off-screen

  if (winX < bp->viewport[0] || winX > bp->viewport[0] + bp->viewport[2] ||
      winY < bp->viewport[1] || winY > bp->viewport[1] + bp->viewport[3])
      return 1; // Center is off-screen

  return 0; // Center is on-screen
}

static void reset_hextrail(ModeInfo *mi) {
  config *bp = &bps[MI_SCREEN(mi)];

  if (bp->chunks) {
    for (int i = 0; i < bp->chunk_count; i++) {
      if (bp->chunks[i]) {
        for (int k = 0; k < bp->chunks[i]->used; k++)
          if (bp->chunks[i]->chunk[k]) {
            free(bp->chunks[i]->chunk[k]);
            bp->chunks[i]->chunk[k] = NULL;
          }
        bp->chunks[i]->used = 0;
      }
      free(bp->chunks[i]);
    }
    bp->chunks = NULL;
    bp->chunk_count = 0;
  }

  memset(bp->hex_grid, 0, 65269+1 * sizeof(uint16_t));
  bp->total_hexagons = 0;
  bp->state = FIRST;
  bp->fade_ratio = 1;

  bp->ncolors = 8;
  if (!bp->colors)
#ifdef USE_SDL
    bp->colors = (SDL_Color *) calloc(bp->ncolors, sizeof(SDL_Color));
  make_smooth_colormap(bp->colors, &bp->ncolors, False, 0, False);
#else
    bp->colors = (XColor *) calloc(bp->ncolors, sizeof(XColor));
  make_smooth_colormap (0, 0, 0, bp->colors, &bp->ncolors, False, 0, False);
#endif
}

static void tick_hexagons (ModeInfo *mi) {
  config *bp = &bps[MI_SCREEN(mi)];
  int i, j, k, doinga = 0, doingb = 0, ignorea = 0, ignoreb = 0;
  static int ticks = 0, iters = 0;
  static int min_x = 0, min_y = 0, max_x = 0, max_y = 0;
  static int min_vx = 0, min_vy = 0, max_vx = 0, max_vy = 0;
  int this_min_vx = 0, this_min_vy = 0, this_max_vx = 0, this_max_vy = 0;

  ticks++;
  for(i=0;i<bp->chunk_count;i++) for(k=0;k<bp->chunks[i]->used;k++) {
    hexagon *h0 = bp->chunks[i]->chunk[k];
    h0->invis = hex_invis(bp, h0->x, h0->y, 0, 0);

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

    if (h0->x > max_x) {
      max_x = h0->x; debug = True;
    } else if (h0->x < min_x) {
      min_x = h0->x; debug = True;
    }
    if (h0->y > max_y) {
      max_y = h0->y; debug = True;
    } else if (h0->y < min_y) {
      min_y = h0->y; debug = True;
    }

    if (debug) {
      printf("pos=%d,%d i=%d vis=(%d-%d,%d-%d) (%d-%d,%d-%d) arms=%d border=%d invis=%d\n",
              h0->x, h0->y, i, min_vx, max_vx, min_vy, max_vy,
              min_x, max_x, min_y, max_y, h0->doing, h0->state, h0->invis);
      bp->debug = bp->now;
    }

    if (h0->doing) {
      doinga++;
      if (h0->invis) ignorea++;
    }

    if (h0->state != EMPTY && h0->state != DONE) {
      doingb++;
      if (h0->invis) ignoreb++;
    }

    if (pausing || (do_expand && h0->invis > 1)) {
      h0->state = DONE;
      continue;
    }
    iters++;

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
              a1->state = IN;
              a1->ratio = a0->ratio - 1;
              a1->speed = a0->speed;
              a0->state = DONE;
              a0->ratio = 1;
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

  if (bp->now > bp->debug) {
    printf("doinga=%d ignorea=%d doingb=%d ignoreb=%d\n", doinga, ignorea, doingb, ignoreb);
    bp->debug = bp->now;
  }

  /* Start a new cell growing.  */
  Bool new_hex = False, started = False;
  static int fails = 0;
  if ((doinga - ignorea) <= 0) {
    hexagon *h0 = NULL;
    if (bp->state == FIRST) {
      fails = 0;
      bp->state = DRAW;
      bp->fade_ratio = 1;
      min_vx = 0; max_vx = 0; min_vy = 0; max_vy = 0;
      min_x = 0; max_x = 0; min_y = 0; max_y = 0;
      ticks = 0; iters = 0;
      printf("\n\n\n\n\nNew hextrail. capacity=%d\n", bp->hexagon_capacity);
      h0 = do_hexagon(bp, 0, 0, 0, 0);
    } else {
      int16_t empty_cells[1000][2]; int empty_count = 0;
	  // TODO - possibly need to change this depending on how grid arranged
      for (int y = min_vy; y <= max_vy && empty_count < 1000; y++)
        for (int x = min_vx; x <= max_vx && empty_count < 1000; x++) {
          int id = xy_to_index(x, y);
          if (id) {
            int idx = bp->hex_grid[id];
            if (!idx && !hex_invis(bp,x,y,0,0)) {
              empty_cells[empty_count][0] = x;
              empty_cells[empty_count++][1] = y;
            }
          }
        }
      if (empty_count > 0) {
        int pick = random() % empty_count;
        int x = empty_cells[pick][0], y = empty_cells[pick][1];
        new_hex = True; printf("Empty_count = %d picked=%d,%d\n", empty_count, x, y);
        do_hexagon(bp, 0, 0, x, y);
      }
    }
    if (!h0) {
      static time_t debug = 0;
      if (debug != bp->now) {
        printf("%s: !h0 new=%d doinga=%d,%d doingb=%d,%d ticks=%d fails=%d\n", __func__,
                new_hex, doinga, ignorea, doingb, ignoreb, ticks, fails);
        debug = bp->now;
      }
    } else if (h0->state == EMPTY && add_arms(bp, h0)) {
      printf("hex created. Arms. doing=%d ticks=%d iters=%d fails=%d pos=%d,%d\n",
          h0->doing, ticks, iters, fails, h0->x, h0->y);
      fails = 0;
      h0->ccolor = random() % bp->ncolors;
      h0->state = DONE;
      started = True;
      if (new_hex) bp->pause_until = bp->now + 5;
    } else {
      fails++;
      static time_t debug = 0;
      if (debug != bp->now) {
        printf("hexagon created. doing=%d ticks=%d fails=%d h0=%d,%d empty=%d visible=%d\n",
            h0->doing, ticks, fails, h0->x, h0->y, h0->state == EMPTY, !h0->invis);
        debug = bp->now;
      }
    }
  }

  if (new_hex && (started || doinga != ignorea))
    printf("New cell: started=%d doinga=%d ignorea=%d doingb=%d ignoreb=%d\n",
            started, doinga, ignorea, doingb, ignoreb);

  if (!started && (doinga - ignorea) < 1 && (doingb - ignoreb) < 1 && bp->state != FADE) {
    printf("Fade started. ticks=%d iters=%d doinga=%d doingb=%d ignorea=%d ignoreb=%d\n",
            ticks, iters, doinga, doingb, ignorea, ignoreb);
    bp->state = FADE;

    for (i=0;i<bp->chunk_count;i++) for (k=0;k<bp->chunks[i]->used;k++) {
      hexagon *h = bp->chunks[i]->chunk[k];
      if (h->state == IN || h->state == WAIT) h->state = OUT;
    }
  } else if (bp->state == FADE && !pause_fade) {
    bp->fade_ratio -= 0.01 * speed;
    if (bp->fade_ratio <= 0) {
      printf("Fade ended.\n");
      reset_hextrail (mi);
	}
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

static void draw_hexagons (ModeInfo *mi) {
  config *bp = &bps[MI_SCREEN(mi)];
  int wire = MI_IS_WIREFRAME(mi);
  GLfloat length = sqrt(3) / 3;
  GLfloat size = length / MI_COUNT(mi);
  GLfloat thick2 = thickness * bp->fade_ratio;
  GLfloat wid = 2.0 / bp->size;
  GLfloat hgt = wid * sqrt(3) / 2;

  // Dynamic array for vertices
  /*Vertex *vertices = NULL;
  int vertex_count = 0;
  int vertex_capacity = 10000; // Initial guess
  vertices = malloc(vertex_capacity * sizeof(Vertex));*/

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

  int i, k;
  for (i=0;i<bp->chunk_count;i++) for (k=0;k<bp->chunks[i]->used;k++) {
    hexagon *h = bp->chunks[i]->chunk[k];
    if (draw_invis < h->invis) continue;

    XYZ pos = { h->x * wid - h->y * wid / 2, h->y * hgt, 0 };
    GLfloat color[4];
    HEXAGON_COLOR (color, h);

    int j, total_arms = 0;
    for (j = 0; j < 6; j++) {
      arm *a = &h->arms[j];
      if (a->state == OUT || a->state == DONE) total_arms++;
    }

    for (j = 0; j < 6; j++) {
      arm *a = &h->arms[j];
      GLfloat margin = thickness * 0.4;
      GLfloat size1 = size * (1 - margin * 2);
      GLfloat size2 = size * (1 - margin * 3);
      int k = (j + 1) % 6;
      XYZ p[6];

      if (hexes_on || (h->state != EMPTY && h->state != DONE)) {
        GLfloat color1[3], ratio;;
		ratio = (hexes_on && h->state != IN) ? 1 : h->ratio;
        memcpy (color1, color, sizeof(color1));
        color1[0] *= ratio;
        color1[1] *= ratio;
        color1[2] *= ratio;

        glColor4fv (color1);
        glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, color1);

        /* Outer edge of hexagon border */
        p[0].x = pos.x + corners[j].x * size1;
        p[0].y = pos.y + corners[j].y * size1;
        p[0].z = pos.z;

        p[1].x = pos.x + corners[k].x * size1;
        p[1].y = pos.y + corners[k].y * size1;
        p[1].z = pos.z;

        /* Inner edge of hexagon border */
        p[2].x = pos.x + corners[k].x * size2;
        p[2].y = pos.y + corners[k].y * size2;
        p[2].z = pos.z;
        p[3].x = pos.x + corners[j].x * size2;
        p[3].y = pos.y + corners[j].y * size2;
        p[3].z = pos.z;

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
        hexagon *hn = neighbor(bp, h, j);
        if (!hn) {
            printf("%s: h=%d,%d h=%d BAD NEIGHBOR\n", __func__, h->x, h->y, j);
            continue;
        }
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
        p[0].x = pos.x + xoff * size2 * thick2 + x * start;
        p[0].y = pos.y + yoff * size2 * thick2 + y * start;
        p[0].z = pos.z;
        p[1].x = pos.x - xoff * size2 * thick2 + x * start;
        p[1].y = pos.y - yoff * size2 * thick2 + y * start;
        p[1].z = pos.z;

        /* Edge */
        p[2].x = pos.x - xoff * size2 * thick2 + x * end;
        p[2].y = pos.y - yoff * size2 * thick2 + y * end;
        p[2].z = pos.z;
        p[3].x = pos.x + xoff * size2 * thick2 + x * end;
        p[3].y = pos.y + yoff * size2 * thick2 + y * end;
        p[3].z = pos.z;

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

        p[0] = pos;

        p[1].x = pos.x + corners[j].x * size3;
        p[1].y = pos.y + corners[j].y * size3;
        p[1].z = pos.z;

        /* Inner edge of hexagon border */
        p[2].x = pos.x + corners[k].x * size3;
        p[2].y = pos.y + corners[k].y * size3;
        p[2].z = pos.z;

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
ENTRYPOINT void reshape_hextrail (ModeInfo *mi, int width, int height) {
  config *bp = &bps[MI_SCREEN(mi)];
  GLfloat h = (GLfloat)height / (GLfloat)width;
  int y = 0;

  if (width > height * 3) {   /* tiny window: show middle */
    height = width * 9 / 16;
    y = -height / 2;
    h = height / (GLfloat)width;
  }

  glViewport(0, y, (GLint)width, (GLint)height);
  printf("%s: width=%d height=%d\n", __func__, width, height);
  bp->viewport[0] = 0;
  bp->viewport[1] = y;
  bp->viewport[2] = width;
  bp->viewport[3] = height;
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective (30.0, 1/h, 1.0, 100.0);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  gluLookAt( 0.0, 0.0, 30.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0);

  GLfloat s = (MI_WIDTH(mi) < MI_HEIGHT(mi) ? (MI_WIDTH(mi) / (GLfloat) MI_HEIGHT(mi)) : 1);
  glScalef (s, s, s); // TODO - what does this do?
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
              MI_WIDTH(mi), MI_HEIGHT(mi), &bp->button_down_p)) {
    if (bp->state == FADE) {
      bp->state = DRAW;
      bp->fade_ratio = 1;
    }
    return True;
  }
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

    if (c == ' ' || c == '\t' || c == '\r' || c == '\n') ;
    else if (c == '>' || c == '.' || c == '+' || c == '=' ||
#ifdef USE_SDL
            keysym == SDLK_UP || keysym == SDLK_PAGEDOWN
#else
            keysym == XK_Up || keysym == XK_Next
#endif
            ) {
      MI_COUNT(mi)++;
      bp->size = MI_COUNT(mi) * 2;
    } else if (
#ifdef USE_SDL
            keysym == SDLK_RIGHT
#else
            keysym == XK_Right
#endif
            )
      MI_COUNT(mi)--;
    else if (
#ifdef USE_SDL
            keysym == SDLK_LEFT
#else
            keysym == XK_Left
#endif
            ) {
      MI_COUNT(mi)--;
	if (bp->size < 1) bp->size = 1;
	} else if (c == '<' || c == ',' || c == '-' || c == '_' ||
               c == '\010' || c == '\177' ||
#ifdef USE_SDL
            keysym == SDLK_DOWN || keysym == SDLK_PAGEUP
#else
            keysym == XK_Down || keysym == XK_Prior
#endif
            ) {
      MI_COUNT(mi)++;
      if (MI_COUNT(mi) < 1) MI_COUNT(mi) = 1;
      bp->size = MI_COUNT(mi) * 2;
    } else if (c == 's') {
      draw_invis = (int8_t)(draw_invis - 1) % 4;
      printf("%s: draw_invis = %d\n", __func__, draw_invis);
    } else if (c == 'S') {
      draw_invis = (int8_t)(draw_invis + 1) % 4;
      printf("%s: draw_invis = %d\n", __func__, draw_invis);
    } else if (c == 'e') {
      do_expand = !do_expand;
      printf("%s: do_expand = %d\n", __func__, do_expand);
    } else if (c == 'h') {
      hexes_on = !hexes_on;
      printf("%s: hexes_on = %d\n", __func__, hexes_on);
    } else if (c == 'p') {
      if (bp->state == FADE) {
		pause_fade = !pause_fade;
        printf("%s: pause_fade = %d hexagons=%d\n", __func__, pause_fade, bp->total_hexagons);
	  } else {
        pausing = !pausing;
        printf("%s: pausing = %d hexagons=%d\n", __func__, pausing, bp->total_hexagons);
	  }
    }
#ifdef USE_SDL
    else if (event->type == SDL_EVENT_QUIT)
#else
    else if (screenhack_event_helper (MI_DISPLAY(mi), MI_WINDOW(mi), event))
#endif
      return True;
    else return False;

    return True;
  }

  return False;
}

ENTRYPOINT void init_hextrail (ModeInfo *mi) {
  MI_INIT (mi, bps);
  config *bp = &bps[MI_SCREEN(mi)];

#ifdef USE_SDL
  // SDL_GLContext is already created in main; store window for reference
  bp->window = mi->window;
  bp->gl_context = mi->gl_context;
  if (!bp->gl_context || !bp->window) {
    fprintf(stderr, "%s: Invalid SDL GL context or window\n", progname);
    exit(1);
  }
#else
  bp->glx_context = init_GL(mi);
#endif

  reshape_hextrail(mi, MI_WIDTH(mi), MI_HEIGHT(mi));

  bp->rot = make_rotator(do_spin ? 0.002 : 0, do_spin ? 0.002 : 0,
                         do_spin ? 0.002 : 0, 1.0, // spin_accel
                         do_wander ? 0.003 : 0, False);
  bp->trackball = gltrackball_init(True);
  gltrackball_reset(bp->trackball, -0.4 + frand(0.8),
                     -0.4 + frand(0.8));

  if (thickness < 0.05) thickness = 0.05;
  if (thickness > 0.5) thickness = 0.5;

  bp->chunk_count = 0;

  bp->size = MI_COUNT(mi) * 2; N = bp->size;
  bp->hex_grid = (uint16_t *)calloc(65269+1, sizeof(uint16_t));
  init_hex_grid_lookup();
  reset_hextrail (mi);
}


ENTRYPOINT void draw_hextrail (ModeInfo *mi) {
  config *bp = &bps[MI_SCREEN(mi)];

#ifdef USE_SDL
  if (SDL_GL_MakeCurrent(bp->window, bp->gl_context) < 0) {
    fprintf(stderr, "%s: SDL_GL_MakeCurrent failed: %s\n", progname, SDL_GetError());
    return;
  }
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

  glPushMatrix();
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
  if (bp->pause_until < bp->now && !pausing) {
    glGetDoublev(GL_MODELVIEW_MATRIX, bp->model);
    glGetDoublev(GL_PROJECTION_MATRIX, bp->proj);
    tick_hexagons (mi);
  }
  draw_hexagons (mi);

  glPopMatrix ();

  if (mi->fps_p) do_fps(mi);
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
  if (!bp->gl_context || !bp->window) return;
#else
  if (!bp->glx_context) return;
  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *bp->glx_context);
#endif

  if (bp->trackball) gltrackball_free (bp->trackball);
  if (bp->rot) free_rotator (bp->rot);
  if (bp->colors) free (bp->colors);

  if (bp->chunks) {
    for (int i = 0; i < bp->chunk_count; i++) if (bp->chunks[i]) {
      for (int k = 0; k < bp->chunks[i]->used; k++)
        if (bp->chunks[i]->chunk[k]) free(bp->chunks[i]->chunk[k]);
      free(bp->chunks[i]);
    }
    free(bp->chunks);
  }
}

XSCREENSAVER_MODULE ("HexTrail", hextrail)

#endif /* USE_GL */
