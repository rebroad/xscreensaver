/* hextrail, Copyright (c) 2022 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 */

#define DEFAULTS	"*delay:	30000       \n" \
			"*showFPS:      False       \n" \
			"*wireframe:    False       \n" \
			"*count:        20          \n" \
			"*suppressRotationAnimation: True\n" \

# define release_hextrail 0

#ifdef MATRIX_DEBUG
#include "../../matrix_debug.h"
#endif
#ifndef WEB_BUILD
#include "xlockmore.h"
#endif
#include "colors.h"
#include "normals.h"
#include "rotator.h"
#include "gltrackball.h"
#include <ctype.h>

// Rotator constants
static const double SPIN_SPEED = 0.002;
static const double WANDER_SPEED = 0.003;
static const double SPIN_ACCEL = 1.0;

#ifdef USE_GL /* whole file */

#define DEF_SPIN        "True"
#define DEF_WANDER      "True"
#define DEF_SPEED       "1.0"
#define DEF_THICKNESS   "0.15"

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
  GLXContext *glx_context;
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
  GLfloat fade_speed;

  int ncolors;
  XColor *colors;
} hextrail_config;

static hextrail_config *bps = NULL;

#ifdef WEB_BUILD
// Make these accessible to web wrapper
Bool do_spin, do_wander;
GLfloat speed, thickness;
#else
static Bool do_spin, do_wander;
static GLfloat speed, thickness;
#endif
static int8_t draw_invis = 1;
static Bool pausing = False, pause_fade = False, hexes_on = False;

static XrmOptionDescRec opts[] = {
  { "-spin",   ".spin",   XrmoptionNoArg, "True" },
  { "+spin",   ".spin",   XrmoptionNoArg, "False" },
  { "-speed",  ".speed",  XrmoptionSepArg, 0 },
  { "-wander", ".wander", XrmoptionNoArg, "True" },
  { "+wander", ".wander", XrmoptionNoArg, "False" },
  { "-thickness", ".thickness", XrmoptionSepArg, 0 },
};

static argtype vars[] = {
  {&do_spin,   "spin",   "Spin",   DEF_SPIN,   t_Bool},
  {&do_wander, "wander", "Wander", DEF_WANDER, t_Bool},
  {&speed,     "speed",  "Speed",  DEF_SPEED,  t_Float},
  {&thickness, "thickness", "Thickness", DEF_THICKNESS, t_Float},
};

ENTRYPOINT ModeSpecOpt hextrail_opts = {countof(opts), opts, countof(vars), vars, NULL};

#define MAX_N 147 // To fit within 16bit
#define erm(n) ((n) * ((n) + 1) / 2)
int q, qq, N;

static uint16_t xy_to_index(int x, int y) {
	if (abs(x) > q || abs(y) > q || abs(x-y) > q) return 0;
	int32_t index;
	if (y < 1) index = x + qq + erm(y + N) - erm(q - 1) + 1;
	else index = x + y + qq + erm(y + N) - erm(q - 1) + 1;

	return (uint16_t)index;
}

static hexagon *get_hex(hextrail_config *bp, uint16_t id) {
  uint16_t idx = bp->hex_grid[id];
  if (!idx) return NULL;  // Empty cell
  int i = (idx-1) / HEXAGON_CHUNK_SIZE, k = (idx-1) % HEXAGON_CHUNK_SIZE;
  return bp->chunks[i]->chunk[k];
}

static hexagon *do_hexagon(hextrail_config *bp, int8_t px, int8_t py, int8_t x, int8_t y) {
  uint16_t id = xy_to_index(x, y);
  if (!id) {
	static time_t debug = 0;
	if (debug != bp->now) {
	  DL(2, "%s: Out of bounds. id=%d coords=%d,%d\n", __func__, id, x, y);
	  debug = bp->now;
	}
	return NULL;
  }

  hexagon *h0 = get_hex(bp, id);
  if (h0) return h0; // We found an existing hexagon, so return it.

  // Create a new hexagon
  if (bp->total_hexagons >= bp->chunk_count * HEXAGON_CHUNK_SIZE) {
	hex_chunk **new_chunks = realloc(bp->chunks, (bp->chunk_count+1) * sizeof(hex_chunk *));
	if (!new_chunks) {
	  DL(0, "Failed to allocate new chunk array\n");
	  return NULL;
	}
	bp->chunks = new_chunks;
	bp->chunks[bp->chunk_count] = calloc(1, sizeof(hex_chunk));
	if (!bp->chunks[bp->chunk_count]) {
	  DL(0, "Failed to allocate chunk\n");
	  return NULL;
	}
	bp->chunks[bp->chunk_count]->used = 0;
	bp->chunk_count++;
  }

  h0 = calloc(1, sizeof(hexagon));
  if (!h0) {
	DL(0, "%s: Malloc failed\n", __func__);
	return NULL;
  }
  hex_chunk *current = bp->chunks[bp->chunk_count - 1];
  current->chunk[bp->total_hexagons % HEXAGON_CHUNK_SIZE] = h0;
  current->used++;
  bp->total_hexagons++;
  bp->hex_grid[id] = bp->total_hexagons;
  h0->x = x; h0->y = y;
  //DL(2, "New hex at %d,%d (parent %d,%d)\n", x, y, px, py);

  return h0;
}

static uint16_t neighbor(int8_t x, int8_t y, int8_t j, int8_t *nx, int8_t *ny) {
  // First value is arm, 2nd value is X, 3rd is Y
  //   0,0   1,0   2,0   3,0   4,0               5   0
  //      1,1   2,1   3,1   4,1   5,1
  //   1,2   2,2   3,2   4,2   5,2            4  arms   1
  //      2,3   3,3   4,3   5,3   6,3
  //   2,4   3,4   4,4   5,4   6,4               3   2
  const int offset[6][2] = {
	  {0, -1}, {1, 0}, {1, 1}, {0, 1}, {-1, 0}, {-1, -1}
  };

  if (nx && ny) {
	*nx = x + offset[j][0]; *ny = y + offset[j][1];
  }

  return xy_to_index(x + offset[j][0], y + offset[j][1]);
}

static int8_t exits(hextrail_config *bp, hexagon *h0) {
  int8_t exits = 0;
  for (int i = 0; i < 6; i++) {
	arm *a0 = &h0->arms[i];
	if (a0->state != EMPTY) continue;     // Incoming arm
	int id = neighbor(h0->x, h0->y, i, NULL, NULL);
	if (!id) continue;					  // Goes off-grid
	if (bp->hex_grid[id]) continue;		  // Cell occupied
	exits++;
  }

  return exits;
}

static int8_t add_arms(hextrail_config *bp, hexagon *h0) {
  int8_t added = 0, target = 1 + (random() % 4); /* Aim for 1-4 arms */
  int8_t idx[6] = {0, 1, 2, 3, 4, 5};
  for (int8_t i = 0; i < 6; i++) {
	int8_t j = random() % 6;
	int8_t swap = idx[j];
	idx[j] = idx[i];
	idx[i] = swap;
  }

  for (int8_t i = 0; i < 6; i++) {
	int8_t j = idx[i];
	arm *a0 = &h0->arms[j];
	if (a0->state != EMPTY) continue;	/* Incoming arm */
	int8_t nx, ny;
	uint16_t n_id = neighbor(h0->x, h0->y, j, &nx, &ny);
	if (!n_id) continue;				/* Goes off-grid */
	hexagon *h1 = do_hexagon(bp, h0->x, h0->y, nx, ny);
	if (!h1) {							/* Hexagon creation failed */
	  DL(0, "%s: Failed to create hex arm=%d from %d,%d\n", __func__, j, h0->x, h0->y);
	  continue;
	}
	if (h1->state != EMPTY) continue;	/* Occupado */

	arm *a1 = &h1->arms[(j + 3) % 6];	/* Opposite arm */

	if (a1->state != EMPTY) {
	  DL(1, "H1 (%d,%d) empty=%d arm[%d].state=%d\n",
			 h1->x, h1->y, h1->state == EMPTY, (j+3)%6, a1->state);
	  bp->pause_until = bp->now + 3;
	  continue;
	}
	a0->state = OUT;
	a0->speed = 0.05 * (0.8 + frand(1.0));

	if (h1->state == EMPTY) {
	  h1->state = IN;

	  /* Mostly keep the same color */
	  if (! (random() % 5)) h1->ccolor = (h0->ccolor + 1) % bp->ncolors;
	  else h1->ccolor = h0->ccolor;
	} else
	  DL(1, "H0 (%d,%d) arm%d out to H1 (%d,%d)->state=%d\n",
h0->x, h0->y, j, h1->x, h1->y, h1->state);

	added++;
	if (added >= target) break;
  }
  h0->doing = added;

  return added;
}

# define H 0.8660254037844386   /* sqrt(3)/2 */

/* Check if a hexagon is within the visible frustum using bounding circle test */
static Bool hex_invis(hextrail_config *bp, int x, int y, GLfloat *rad) {
  GLfloat wid = 2.0 / bp->size;
  GLfloat hgt = wid * H;
  XYZ pos = { x * wid - y * wid / 2, y * hgt, 0 };
  GLdouble winX, winY, winZ;

  /* Project point to screen coordinates */
  gluProject(pos.x, pos.y, pos.z, bp->model, bp->proj, bp->viewport, &winX, &winY, &winZ);

  static time_t debug = 0;
  if (debug != bp->now) {
	DL(2, "%s: winX=%d, winY=%d, winZ=%.1f vp=%d,%d\n", __func__,
	  (int)winX, (int)winY, winZ, bp->viewport[2], bp->viewport[3]);
	debug = bp->now;
  }

  if (winZ <= 0 || winZ >= 1) return 2;  // Far off in Z

  // Calculate bounding circle radius in screen space
  XYZ edge_posx = pos, edge_posy = pos;
  edge_posx.x += wid/2; edge_posy.y += hgt/2;

  GLdouble edge_xx, edge_xy, edge_yx, edge_yy, edge_z;
  gluProject((GLdouble)edge_posx.x, (GLdouble)edge_posx.y, (GLdouble)edge_posx.z,
			bp->model, bp->proj, bp->viewport, &edge_xx, &edge_xy, &edge_z);
  gluProject((GLdouble)edge_posy.x, (GLdouble)edge_posy.y, (GLdouble)edge_posy.z,
			bp->model, bp->proj, bp->viewport, &edge_yx, &edge_yy, &edge_z);

  // Calculate radius in screen space (accounting for perspective projection)
  GLdouble xx_diff = edge_xx - winX, xy_diff = edge_xy - winY;
  GLdouble yx_diff = edge_yx - winX, yy_diff = edge_yy - winY;
  GLdouble radiusx = sqrt(xx_diff*xx_diff + xy_diff*xy_diff);
  GLdouble radiusy = sqrt(yx_diff*yx_diff + yy_diff*yy_diff);

  // And now we take both radiuses and work out the maximum it could in reality.
  GLdouble radius = (radiusx > radiusy) ? radiusx : radiusy;
  if (rad) *rad = radius;

  if (winX + radius < bp->viewport[0] || winX - radius > bp->viewport[0] + bp->viewport[2] ||
	  winY + radius < bp->viewport[1] || winY - radius > bp->viewport[1] + bp->viewport[3])
	return 2; // Fully off-screen

  if (winX < bp->viewport[0] || winX > bp->viewport[0] + bp->viewport[2] ||
	  winY < bp->viewport[1] || winY > bp->viewport[1] + bp->viewport[3])
	return 1; // Center is off-screen

  return 0; // Center is on-screen
}

const XYZ corners[] = {{  0, -1,   0 },       /*      0      */
					   {  H, -0.5, 0 },       /*  5       1  */
					   {  H,  0.5, 0 },       /*             */
					   {  0,  1,   0 },       /*  4       2  */
					   { -H,  0.5, 0 },       /*      3      */
					   { -H, -0.5, 0 }};

static XYZ scaled_corners[6][4];
static GLfloat size, wid, hgt, size1, size2, size3, size4, thick2;

static void scale_corners(ModeInfo *mi) {
  hextrail_config *bp = &bps[MI_SCREEN(mi)];
  size = (H * 2 / 3) / MI_COUNT(mi);
  wid = 2.0 / bp->size;
  hgt = wid * H;
  GLfloat margin = thickness * 0.4;
  size1 = size * (1 - margin * 2);
  size2 = size * (1 - margin * 3);
  thick2 = thickness * bp->fade_ratio;
  size3 = size * thick2 * 0.8;
  size4 = size3 * 2; // when total_arms == 1
  for (int i = 0; i < 6; i++) {
	scaled_corners[i][0].x = corners[i].x * size1;
	scaled_corners[i][0].y = corners[i].y * size1;
	scaled_corners[i][1].x = corners[i].x * size2;
	scaled_corners[i][1].y = corners[i].y * size2;
	scaled_corners[i][2].x = corners[i].x * size3;
	scaled_corners[i][2].y = corners[i].y * size3;
	scaled_corners[i][3].x = corners[i].x * size4;
	scaled_corners[i][3].y = corners[i].y * size4;
  }
}

static void reset_hextrail(ModeInfo *mi) {
  hextrail_config *bp = &bps[MI_SCREEN(mi)];

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

  memset(bp->hex_grid, 0, (N*(N+1)*3+2) * sizeof(uint16_t));
  bp->total_hexagons = 0;
  bp->state = FIRST;
  bp->fade_ratio = 1;
  scale_corners(mi);

  bp->ncolors = 8;
  if (!bp->colors)
	bp->colors = (XColor *) calloc(bp->ncolors, sizeof(XColor));
  make_smooth_colormap (0, 0, 0, bp->colors, &bp->ncolors, False, 0, False);
}

static void handle_arm_out(hextrail_config *bp, hexagon *h0, arm *a0, int j) {
  a0->ratio += a0->speed * speed;
  if (a0->ratio >= 1) {
	/* Just finished growing from center to edge.
	   Pass the baton to this waiting neighbor. */
	hexagon *h1 = get_hex(bp, neighbor(h0->x, h0->y, j, NULL, NULL));
	arm *a1 = &h1->arms[(j + 3) % 6];
	a1->state = IN;
	a1->ratio = 0;
	a1->speed = a0->speed;
	h0->doing--; h1->doing++;
	a0->state = DONE;
	a0->ratio = 1;
  } else if (a0->ratio <= 0) {
	/* Just finished retreating back to center */
	a0->state = EMPTY;
	a0->ratio = 0;
	h0->doing--;
  }
}

static void handle_arm_in(hextrail_config *bp, hexagon *h0, arm *a0, int j) {
  a0->ratio += a0->speed * speed;
  if (a0->ratio >= 1) {
	/* Just finished growing from edge to center.  Look for any available exits. */
	h0->doing = 0;
	if (add_arms(bp, h0)) {
	  a0->state = DONE;
	  a0->ratio = 1;
	} else { // nub grow
	  a0->state = WAIT;
	  a0->ratio = ((a0->ratio - 1) * 5) + 1; a0->speed *= 5;
	}
  } else if (a0->ratio >= 0.9 && a0->speed < 0.2 && (a0->speed > 0.1 || !exits(bp, h0))) {
	//DL(2, "%s: %d,%d speed=%f->", __func__, h0->x, h0->y, a0->speed);
	a0->speed *= 1.5;
	//DL(2, "%f\n", a0->speed);
  }
}

static void handle_nub_grow(hextrail_config *bp, hexagon *h0, arm *a0, int j) {
  a0->ratio += a0->speed * (2 - a0->ratio);
  if (a0->ratio >= 1.999) {
	a0->state = DONE;
	a0->ratio = 1;
  }
}

typedef void (*state_handler)(hextrail_config *bp, hexagon *h0, arm *a0, int j);
state_handler arm_handlers[] = {
  [EMPTY] = NULL,
  [OUT] = handle_arm_out,
  [IN] = handle_arm_in,
  [WAIT] = handle_nub_grow,
  [DONE] = NULL
};

static void tick_hexagons (ModeInfo *mi) {
  hextrail_config *bp = &bps[MI_SCREEN(mi)];
  int i, j, k, doinga = 0, doingb = 0, ignorea = 0, ignoreb = 0;
  static time_t tick_report = 0;
  static unsigned int ticks = 0, iters = 0, tps = 0;
  static int min_x = 0, min_y = 0, max_x = 0, max_y = 0;
  static int min_vx = 0, min_vy = 0, max_vx = 0, max_vy = 0;
  int this_min_vx = 0, this_min_vy = 0, this_max_vx = 0, this_max_vy = 0;

  ticks++;
  for(i=0;i<bp->chunk_count;i++) for(k=0;k<bp->chunks[i]->used;k++) {
	hexagon *h0 = bp->chunks[i]->chunk[k];
	if (!(ticks % 4)) h0->invis = hex_invis(bp, h0->x, h0->y, 0);
	// TODO - capture the radius from hex_invis and use for reducing frawing when FPS low

	Bool debug = False;

	if (bp->now != tick_report) {
	  tps = ticks; ticks = 0;
	  tick_report = bp->now;
	}

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
	  DL(3, "pos=%d,%d i=%d vis=(%d-%d,%d-%d) (%d-%d,%d-%d) arms=%d border=%d invis=%d\n",
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

	if (pausing) continue;
	iters++;

	/* Enlarge any still-growing arms if active.  */
	for (j = 0; j < 6; j++) {
	  arm *a0 = &h0->arms[j];
	  if (arm_handlers[a0->state]) arm_handlers[a0->state](bp, h0, a0, j);
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
		if (! (random() % (int)(50.0/speed))) h0->state = OUT;
		break;
	  case EMPTY:
	  case DONE:
		break;
	  default:
		  DL(0, "h0->state = %d\n", h0->state);
		  abort(); break;
	}
  } // Loop through each hexagon

  min_vx = this_min_vx; max_vx = this_max_vx; min_vy = this_min_vy; max_vy = this_max_vy;

  if (bp->now > bp->debug) {
	DL(2, "tps=%d doinga=%d ignorea=%d doingb=%d ignoreb=%d\n", tps, doinga, ignorea, doingb, ignoreb);
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
	  iters = 0;
	  DL(2, "\n\n\n\n\nNew hextrail. capacity=%d\n", bp->hexagon_capacity);
	  h0 = do_hexagon(bp, 0, 0, 0, 0);
	} else {
	  int16_t empty_cells[1000][2]; int empty_count = 0;
	  // TODO - possibly need to change this depending on how grid arranged
	  for (int y = min_vy; y <= max_vy && empty_count < 1000; y++)
		for (int x = min_vx; x <= max_vx && empty_count < 1000; x++) {
		  uint16_t id = xy_to_index(x, y);
		  if (id) {
			int idx = bp->hex_grid[id];
			if (!idx && !hex_invis(bp,x,y,0)) {
			  empty_cells[empty_count][0] = x;
			  empty_cells[empty_count++][1] = y;
			}
		  }
		}
	  if (empty_count > 0) {
		  int pick = random() % empty_count;
		  int x = empty_cells[pick][0], y = empty_cells[pick][1];
		  new_hex = True; DL(2, "Empty_count = %d picked=%d,%d\n", empty_count, x, y);
		  do_hexagon(bp, 0, 0, x, y);
	  }
	}
	if (!h0) {
	  static time_t debug = 0;
	  if (debug != bp->now) {
		DL(2, "%s: !h0 new=%d doinga=%d,%d doingb=%d,%d fails=%d\n", __func__,
				new_hex, doinga, ignorea, doingb, ignoreb, fails);
		debug = bp->now;
	  }
	} else if (h0->state == EMPTY && add_arms(bp, h0)) {
	  DL(2, "hex created. Arms. doing=%d iters=%d fails=%d pos=%d,%d\n",
		h0->doing, iters, fails, h0->x, h0->y);
	  fails = 0;
	  h0->ccolor = random() % bp->ncolors;
	  h0->state = IN;
	  started = True;
	  if (new_hex) bp->pause_until = bp->now + 5;
	} else {
	  fails++;
	  static time_t debug = 0;
	  if (debug != bp->now) {
		DL(2, "hexagon created. doing=%d fails=%d h0=%d,%d empty=%d visible=%d\n",
		  h0->doing, fails, h0->x, h0->y, h0->state == EMPTY, !h0->invis);
		debug = bp->now;
	  }
	}
  }

  if (new_hex && (started || doinga != ignorea))
	DL(2, "New cell: started=%d doinga=%d ignorea=%d doingb=%d ignoreb=%d\n",
		started, doinga, ignorea, doingb, ignoreb);

  if (!started && (doinga - ignorea) < 1 && (doingb - ignoreb) < 1 && bp->state != FADE) {
	DL(2, "Fade started. iters=%d doinga=%d doingb=%d ignorea=%d ignoreb=%d\n",
		iters, doinga, doingb, ignorea, ignoreb);
	bp->state = FADE; bp->fade_speed = 0.01; pause_fade = False;

	for (i=0;i<bp->chunk_count;i++) for (k=0;k<bp->chunks[i]->used;k++) {
	  hexagon *h = bp->chunks[i]->chunk[k];
	  if (h->state == IN || h->state == WAIT) h->state = OUT;
	}
  } else if (bp->state == FADE && !pause_fade) {
	bp->fade_ratio -= bp->fade_speed;
	scale_corners(mi);
	if (bp->fade_ratio <= 0) {
	  DL(2, "Fade ended.\n");
	  reset_hextrail (mi);
	} else if (bp->fade_ratio >= 1) {
	  bp->fade_ratio = 1; bp->fade_speed = 0;
	  bp->state = DRAW; pause_fade = True;
	}
  }
}

# define HEXAGON_COLOR(V,H) do { \
  (V)[0] = bp->colors[(H)->ccolor].red   / 65535.0 * bp->fade_ratio; \
  (V)[1] = bp->colors[(H)->ccolor].green / 65535.0 * bp->fade_ratio; \
  (V)[2] = bp->colors[(H)->ccolor].blue  / 65535.0 * bp->fade_ratio; \
  (V)[3] = 1; \
} while (0)

static void
draw_hexagons (ModeInfo *mi)
{
  hextrail_config *bp = &bps[MI_SCREEN(mi)];
  int wire = MI_IS_WIREFRAME(mi);

  glFrontFace (GL_CCW);
  glBegin (wire ? GL_LINES : GL_TRIANGLES);
  glNormal3f (0, 0, 1);

  int i, k;
  for (i=0;i<bp->chunk_count;i++) for (k=0;k<bp->chunks[i]->used;k++) {
	hexagon *h = bp->chunks[i]->chunk[k];
	if (draw_invis < h->invis) continue;
	XYZ pos = { h->x * wid - h->y * wid / 2, h->y * hgt, 0 }; // TODO - better stored within hexagon struct?
	GLfloat color[4]; HEXAGON_COLOR (color, h);

	int j, total_arms = 0;
	GLfloat nub_ratio = 0;
	for (j = 0; j < 6; j++) {
	  arm *a = &h->arms[j];
	  if (a->state == OUT || a->state == DONE || a->state == WAIT) total_arms++;
	  if (a->state == WAIT) nub_ratio = a->ratio;
	} // loop through arms

	// Debug: Log arm states for first few hexagons (limit to first 5 hexagons)
	static int debug_hex_count = 0;
	if (debug_hex_count < 5) {
	  DL(1, "DEBUG: Hex %d at (%d,%d) arm states: ", debug_hex_count, h->x, h->y);
	  for (j = 0; j < 6; j++) {
		arm *a = &h->arms[j];
		DL(1, "arm%d=%s(%.2f) ", j,
		   a->state == EMPTY ? "EMPTY" :
		   a->state == IN ? "IN" :
		   a->state == OUT ? "OUT" :
		   a->state == WAIT ? "WAIT" : "DONE", a->ratio);
	  }
	  DL(1, "total_arms=%d\n", total_arms);
	  debug_hex_count++;
	}

	for (j = 0; j < 6; j++) {
	  arm *a = &h->arms[j];
	  int k = (j + 1) % 6;
	  XYZ p[6];

	  if (hexes_on || (h->state != EMPTY && h->state != DONE)) {
		GLfloat color1[4], ratio;
		ratio = (hexes_on && h->state != IN) ? 1 : h->ratio;
		memcpy(color1, color, sizeof(color1));
		color1[0] *= ratio; color1[1] *= ratio; color1[2] *= ratio;

		glColor4fv (color1);
		glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, color1);

		/* Outer edge of hexagon border */
		p[0].x = pos.x + scaled_corners[j][0].x;
		p[0].y = pos.y + scaled_corners[j][0].y;
		p[0].z = pos.z;

		p[1].x = pos.x + scaled_corners[k][0].x;
		p[1].y = pos.y + scaled_corners[k][0].y;
		p[1].z = pos.z;

		/* Inner edge of hexagon border */
		p[2].x = pos.x + scaled_corners[k][1].x;
		p[2].y = pos.y + scaled_corners[k][1].y;
		p[2].z = pos.z;
		p[3].x = pos.x + scaled_corners[j][1].x;
		p[3].y = pos.y + scaled_corners[j][1].y;
		p[3].z = pos.z;

		glVertex3f (p[0].x, p[0].y, p[0].z);
		glVertex3f (p[1].x, p[1].y, p[1].z);
		if (!wire) glVertex3f (p[2].x, p[2].y, p[2].z);
		mi->polygon_count++;

		glVertex3f (p[2].x, p[2].y, p[2].z);
		glVertex3f (p[3].x, p[3].y, p[3].z);
		if (!wire) glVertex3f (p[0].x, p[0].y, p[0].z);
		mi->polygon_count++;
	  } // hexes_on || (h->state != EMPTY && h->state != DONE)

	  /* Line from center to edge, or edge to center.  */
	  if (a->state == IN || a->state == OUT || a->state == DONE || a->state == WAIT) {
		GLfloat x   = (corners[j].x + corners[k].x) / 2;
		GLfloat y   = (corners[j].y + corners[k].y) / 2;
		GLfloat xoff = corners[k].x - corners[j].x;
		GLfloat yoff = corners[k].y - corners[j].y;
		
		// Debug: Log thickness values for first few arms
		static int debug_arm_count = 0;
		if (debug_arm_count < 5) {
		  DL(1, "DEBUG: Arm rendering - size2=%.3f, thick2=%.3f, xoff=%.3f, yoff=%.3f\n", 
		     size2, thick2, xoff, yoff);
		  debug_arm_count++;
		}
		GLfloat line_length = (a->state == WAIT) ? 1 : a->ratio;
		GLfloat start, end;
		GLfloat ncolor[4];
		GLfloat color1[4];
		GLfloat color2[4];

		/* Color of the outer point of the line is average color of
		   this and the neighbor. */
		hexagon *hn = get_hex(bp, neighbor(h->x, h->y, j, NULL, NULL));
		if (!hn) {
		  DL(2, "%s: h=%d,%d j=%d BAD NEIGHBOR\n", __func__, h->x, h->y, j);
		  continue;
		}
		HEXAGON_COLOR (ncolor, hn);
		ncolor[0] = (ncolor[0] + color[0]) / 2;
		ncolor[1] = (ncolor[1] + color[1]) / 2;
		ncolor[2] = (ncolor[2] + color[2]) / 2;
		//ncolor[3] = (ncolor[3] + color[3]) / 2;

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

		glColor4fv (color2);
		glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, color2);
		glVertex3f (p[3].x, p[3].y, p[3].z);
		glColor4fv (color1);
		glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, color1);
		glVertex3f (p[0].x, p[0].y, p[0].z);
		if (!wire) glVertex3f (p[1].x, p[1].y, p[1].z);
		mi->polygon_count++;

		glVertex3f (p[1].x, p[1].y, p[1].z);

		glColor4fv (color2);
		glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, color2);
		glVertex3f (p[2].x, p[2].y, p[2].z);
		if (!wire) glVertex3f (p[3].x, p[3].y, p[3].z);
		mi->polygon_count++;
	  } // arm is IN, OUT, DONE, or WAIT

	  /* Hexagon (one triangle of) in center to hide line miter/bevels.  */
	  if (total_arms && a->state != DONE && a->state != OUT) {
		p[0] = pos; p[1].z = pos.z; p[2].z = pos.z;

		if (nub_ratio) {
		  p[1].x = pos.x + scaled_corners[j][2].x * nub_ratio;
		  p[1].y = pos.y + scaled_corners[j][2].y * nub_ratio;
		  p[2].x = pos.x + scaled_corners[k][2].x * nub_ratio;
		  p[2].y = pos.y + scaled_corners[k][2].y * nub_ratio;
		} else {
		  int8_t s = (total_arms == 1) ? 3 : 2;
		  p[1].x = pos.x + scaled_corners[j][s].x;
		  p[1].y = pos.y + scaled_corners[j][s].y;
		  /* Inner edge of hexagon border */
		  p[2].x = pos.x + scaled_corners[k][s].x;
		  p[2].y = pos.y + scaled_corners[k][s].y;
		}

		glColor4fv (color);
		glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, color);
		if (!wire) glVertex3f (p[0].x, p[0].y, p[0].z);
		glVertex3f (p[1].x, p[1].y, p[1].z);
		glVertex3f (p[2].x, p[2].y, p[2].z);
		mi->polygon_count++;
	  } // total_arms && a->state != DONE && a->state != OUT
	} // loop through arms
  } // loop through hexagons

  glEnd();
}

/* Window management, etc */
ENTRYPOINT void
reshape_hextrail (ModeInfo *mi, int width, int height)
{
  hextrail_config *bp = &bps[MI_SCREEN(mi)];
  GLfloat h = (GLfloat) height / (GLfloat) width;
  int y = 0;

  if (width > height * 3) {   /* tiny window: show middle */
	height = width * 9/16;
	y = -height/2;
	h = height / (GLfloat) width;
  }

  glViewport (0, y, (GLint) width, (GLint) height);
  DL(2, "%s: width=%d height=%d\n", __func__, width, height);
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

  {
	GLfloat s = (MI_WIDTH(mi) < MI_HEIGHT(mi)
				 ? (MI_WIDTH(mi) / (GLfloat) MI_HEIGHT(mi))
				 : 1);
	glScalef (s, s, s);
  }

  glClear(GL_COLOR_BUFFER_BIT);
}

ENTRYPOINT Bool
hextrail_handle_event (ModeInfo *mi, XEvent *event)
{
  debug_level = mi->debug_level;  /* Set global debug_level for DL() calls */
  hextrail_config *bp = &bps[MI_SCREEN(mi)];

  if (gltrackball_event_handler (event, bp->trackball,
			  MI_WIDTH(mi), MI_HEIGHT(mi), &bp->button_down_p)) {
	if (bp->fade_speed > 0) bp->fade_speed = -bp->fade_speed;
	return True;
  } else if (event->xany.type == KeyPress) {
	KeySym keysym;
	char c = 0;
	XLookupString (&event->xkey, &c, 1, &keysym, 0);

	if (c == '\t' || c == '\r' || c == '\n') ;
	else if (c == '>' || c == '.' || keysym == XK_Up || keysym == XK_Next) {
	  MI_COUNT(mi)--;
	  if (MI_COUNT(mi) < 1) MI_COUNT(mi) = 1;
	  bp->size = MI_COUNT(mi) * 2;
	  scale_corners(mi);
	} else if (keysym == XK_Right) {
	  speed *= 2;
	  if (speed > 20) speed = 20;
	  DL(1, "%s: speed = %f -> %f\n", __func__, speed/2, speed);
	} else if (keysym == XK_Left) {
	  speed /= 2;
	  if (speed < 0.0001) speed = 0.0001;
	  DL(1, "%s: speed = %f -> %f\n", __func__, speed*2, speed);
	} else if (c == '<' || c == ',' || c == '_' || c == '\010' || c == '\177' ||
		keysym == XK_Down || keysym == XK_Prior) {
	  MI_COUNT(mi)++;
	  bp->size = MI_COUNT(mi) * 2;
	  scale_corners(mi);
	} else if (c == '-') {
	  MI_COUNT(mi)--;
	  if (MI_COUNT(mi) < 1) MI_COUNT(mi) = 1;
	  scale_corners(mi);
	} else if (c == '=' || c == '+') {
	  MI_COUNT(mi)++;
	  scale_corners(mi);
	} else if (c == 'I') {
	  draw_invis = (int8_t)(draw_invis - 1) % 4;
	  DL(1, "%s: draw_invis = %d\n", __func__, draw_invis);
	} else if (c == 'i') {
	  draw_invis = (int8_t)(draw_invis + 1) % 4;
	  DL(1, "%s: draw_invis = %d\n", __func__, draw_invis);
	} else if (c == 'D') {
	  mi->debug_level = (int8_t)(mi->debug_level - 1) % 4;
	  DL(1, "%s: debug_level = %d\n", __func__, mi->debug_level);
	} else if (c == 'd') {
	  mi->debug_level = (int8_t)(mi->debug_level + 1) % 4;
	  DL(1, "%s: debug_level = %d\n", __func__, mi->debug_level);
	} else if (c == 'h') {
	  hexes_on = !hexes_on;
	  DL(1, "%s: hexes_on = %d\n", __func__, hexes_on);
	} else if (c == 'p' || c == ' ') {
	  if (pause_fade) {
		pause_fade = False;
		DL(1, "%s: pause_fade = %d hexagons=%d\n", __func__, pause_fade, bp->total_hexagons);
	  } else if (bp->fade_speed > 0) {
		bp->fade_speed = -bp->fade_speed;
		DL(1, "%s: pause_fade = %d hexagons=%d\n", __func__, pause_fade, bp->total_hexagons);
	  } else {
		pausing = !pausing;
		DL(1, "%s: pausing = %d hexagons=%d\n", __func__, pausing, bp->total_hexagons);
	  }
	} else if (screenhack_event_helper (MI_DISPLAY(mi), MI_WINDOW(mi), event))
	  return True;
	else return False;

	return True;
  }

  return False;
}

// Function to create rotator with current spin/wander settings
static rotator* create_hextrail_rotator(void) {
#ifdef WEB_BUILD
	printf("DEBUG: Creating rotator - do_spin=%d, do_wander=%d\n", do_spin, do_wander);
#endif

	return make_rotator(do_spin ? SPIN_SPEED : 0,
					   do_spin ? SPIN_SPEED : 0,
					   do_spin ? SPIN_SPEED : 0,
					   SPIN_ACCEL,
					   do_wander ? WANDER_SPEED : 0,
					   False);
}

ENTRYPOINT void
init_hextrail(ModeInfo *mi)
{
  debug_level = mi->debug_level;  /* Set global debug_level for DL() calls */
  DL(1, "DEBUG: init_hextrail called\n");
  hextrail_config *bp;
  MI_INIT (mi, bps);
  bp = &bps[MI_SCREEN(mi)];

  bp->glx_context = init_GL(mi);

  reshape_hextrail (mi, MI_WIDTH(mi), MI_HEIGHT(mi));

  bp->rot = create_hextrail_rotator();
  bp->trackball = gltrackball_init (True);

  /* Let's tilt the scene a little. */
  gltrackball_reset (bp->trackball,
					 -0.4 + frand(0.8),
					 -0.4 + frand(0.8));

  if (thickness < 0.05) thickness = 0.05;
  if (thickness > 0.5) thickness = 0.5;
  DL(1, "%s: Thickness value = %.3f, Speed value = %.3f\n", __func__, thickness, speed);

  bp->chunk_count = 0;

  bp->size = MI_COUNT(mi) * 2; N = bp->size * 2 + 1; // N should be odd
  if (N > MAX_N) {
	DL(1, "%s: Limiting N (%d) to MAX_N (%d)\n", __func__, N, MAX_N);
	N = MAX_N;
  }
  bp->hex_grid = (uint16_t *)calloc(N*(N+1)*3+2, sizeof(uint16_t));
  q = (N + 1) / 2;
  qq = q * 2;
  DL(1, "%s: N=%d q=%d qq=%d\n", __func__, N, q, qq);
  reset_hextrail(mi);
}


ENTRYPOINT void
draw_hextrail (ModeInfo *mi)
{
  debug_level = mi->debug_level;  /* Set global debug_level for DL() calls */
  hextrail_config *bp = &bps[MI_SCREEN(mi)];

  if (!bp->glx_context) return;
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);

  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *bp->glx_context);

  glShadeModel(GL_SMOOTH);

  glDisable(GL_DEPTH_TEST);
  glEnable(GL_NORMALIZE);
  glDisable(GL_CULL_FACE);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glPushMatrix();
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

  bp->now = time(NULL);
  if (bp->pause_until < bp->now && !pausing) {
	glGetDoublev(GL_MODELVIEW_MATRIX, bp->model);
	glGetDoublev(GL_PROJECTION_MATRIX, bp->proj);
	tick_hexagons (mi);
  }
  draw_hexagons (mi);

  glPopMatrix ();

  if (mi->fps_p) do_fps (mi);
  glFinish();

  glXSwapBuffers(dpy, window);
}


ENTRYPOINT void
free_hextrail (ModeInfo *mi)
{
  hextrail_config *bp = &bps[MI_SCREEN(mi)];

  if (!bp->glx_context) return;
  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *bp->glx_context);

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

#ifdef WEB_BUILD
// Function to update rotator when spin/wander settings change
void update_hextrail_rotator(void) {
	extern hextrail_config *bps;
	extern ModeInfo web_mi;

	hextrail_config *bp = &bps[web_mi.screen];
	if (!bp->rot) return;

	// Free old rotator and create new one with updated settings
	if (bp->rot) {
		free_rotator(bp->rot);
	}

	bp->rot = create_hextrail_rotator();
}
#endif

XSCREENSAVER_MODULE ("HexTrail", hextrail)

#endif /* USE_GL */
