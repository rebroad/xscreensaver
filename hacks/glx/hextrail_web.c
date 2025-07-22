/* hextrail, Copyright (c) 2022 Jamie Zawinski <jwz@jwz.org>
 *
 * Web version with SDL support for emscripten compilation
 */

#define DEFAULTS	"*delay:	30000       \n" \
			"*showFPS:      False       \n" \
			"*wireframe:    False       \n" \
			"*count:        20          \n" \
			"*suppressRotationAnimation: True\n" \

# define release_hextrail 0

#ifdef USE_SDL
#include <SDL3/SDL.h>
#include <GL/gl.h>
#include <GL/glu.h>
#define Bool int
#define True 1
#define False 0
#else
#include "xlockmore.h"
#endif

#include "colors.h"
#include "normals.h"
#include "rotator.h"
#include "gltrackball.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void scale_corners(ModeInfo *mi);

#ifdef USE_GL /* whole file */

#define DEF_SPIN        "True"
#define DEF_WANDER      "True"
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
};

typedef struct {
#ifdef USE_SDL
  SDL_Window *window;
  SDL_GLContext gl_context;
  SDL_Color *colors;
  int window_width;
  int window_height;
#else
  GLXContext *glx_context;
  XColor *colors;
#endif
  rotator *rot;
  trackball_state *trackball;
  Bool button_down_p;

  int grid_w, grid_h;
  hexagon *hexagons;
  int live_count;
  enum { FIRST, DRAW, FADE } state;
  GLfloat fade_ratio;

  int ncolors;
} hextrail_configuration;

static hextrail_configuration *bps = NULL;

static Bool do_spin;
static GLfloat speed;
static Bool do_wander;
static GLfloat thickness;

#ifdef USE_SDL
// Web-specific function exports
EMSCRIPTEN_KEEPALIVE
void set_speed(GLfloat new_speed) {
    speed = new_speed;
}

EMSCRIPTEN_KEEPALIVE
void set_thickness(GLfloat new_thickness) {
    thickness = new_thickness;
}

EMSCRIPTEN_KEEPALIVE
void set_spin(int spin_enabled) {
    do_spin = spin_enabled;
}

EMSCRIPTEN_KEEPALIVE
void set_wander(int wander_enabled) {
    do_wander = wander_enabled;
}

EMSCRIPTEN_KEEPALIVE
void handle_mouse_drag(int delta_x, int delta_y) {
    if (bps && bps->trackball) {
        // Convert screen coordinates to normalized device coordinates
        float x = (float)delta_x / 800.0f;  // Assuming 800px width
        float y = (float)delta_y / 600.0f;  // Assuming 600px height
        gltrackball_rotate(bps->trackball, x, y);
    }
}

EMSCRIPTEN_KEEPALIVE
void handle_mouse_wheel(int delta) {
    if (bps && bps->trackball) {
        // Zoom in/out based on wheel delta
        float zoom = delta > 0 ? 1.1f : 0.9f;
        // This would need to be implemented in the trackball code
    }
}

// Simple random number generator for web
static unsigned int web_seed = 1;
static int web_random() {
    web_seed = web_seed * 1103515245 + 12345;
    return (unsigned int)(web_seed / 65536) % 32768;
}

#define random() web_random()
#define frand(n) ((float)web_random() / 32768.0f * (n))

// Simple color structure for web
typedef struct {
    unsigned char r, g, b;
} WebColor;

static void make_smooth_colormap(int r1, int g1, int b1, WebColor *colors, int *ncolors, int cycle_p, int cycle_len, int reverse_p) {
    // Create a simple color palette
    WebColor palette[] = {
        {255, 0, 0},    // Red
        {255, 165, 0},  // Orange
        {255, 255, 0},  // Yellow
        {0, 255, 0},    // Green
        {0, 0, 255},    // Blue
        {75, 0, 130},   // Indigo
        {238, 130, 238}, // Violet
        {255, 192, 203} // Pink
    };
    
    for (int i = 0; i < *ncolors && i < 8; i++) {
        colors[i] = palette[i];
    }
}

#else
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
#endif

// Initialize default values
static void init_defaults() {
    do_spin = True;
    do_wander = True;
    speed = 1.0f;
    thickness = 0.15f;
}

static void
make_plane (ModeInfo *mi)
{
  hextrail_configuration *bp = &bps[MI_SCREEN(mi)];
  int x, y;
  GLfloat size, w, h;
  hexagon *grid;

#ifdef USE_SDL
  int count = 20; // Default count for web
#else
  int count = MI_COUNT(mi);
#endif

  bp->grid_w = count * 2;
  bp->grid_h = bp->grid_w;

  grid = (bp->hexagons
          ? bp->hexagons
          : (hexagon *) malloc (bp->grid_w * bp->grid_h * sizeof(*grid)));
  memset (grid, 0, bp->grid_w * bp->grid_h * sizeof(*grid));

  bp->ncolors = 8;
  if (! bp->colors)
#ifdef USE_SDL
    bp->colors = (SDL_Color *) calloc(bp->ncolors, sizeof(SDL_Color));
#else
    bp->colors = (XColor *) calloc(bp->ncolors, sizeof(XColor));
#endif

#ifdef USE_SDL
  WebColor web_colors[8];
  make_smooth_colormap (0, 0, 0, web_colors, &bp->ncolors, False, 0, False);
  for (int i = 0; i < bp->ncolors; i++) {
    bp->colors[i].r = web_colors[i].r;
    bp->colors[i].g = web_colors[i].g;
    bp->colors[i].b = web_colors[i].b;
    bp->colors[i].a = 255;
  }
#else
  make_smooth_colormap (0, 0, 0,
                        bp->colors, &bp->ncolors,
                        False, 0, False);
#endif

  size = 2.0 / bp->grid_w;
  w = size;
  h = size * sqrt(3) / 2;

  bp->hexagons = grid;

  for (y = 0; y < bp->grid_h; y++)
    for (x = 0; x < bp->grid_w; x++)
      {
        hexagon *h0 = &grid[y * bp->grid_w + x];
        h0->pos.x = (x - bp->grid_w/2) * w;
        h0->pos.y = (y - bp->grid_h/2) * h;
        h0->border_state = EMPTY;
        h0->border_ratio = 0;

        if (y & 1)
          h0->pos.x += w / 2;

        h0->ccolor = random() % bp->ncolors;
      }

  for (y = 0; y < bp->grid_h; y++)
    for (x = 0; x < bp->grid_w; x++)
      {
        hexagon *h0 = &grid[y * bp->grid_w + x];
# undef NEIGHBOR
# define NEIGHBOR(I,XE,XO,Y) do {                                        \
          int x1 = x + (y & 1 ? (XO) : (XE));                            \
          int y1 = y + (Y);                                              \
          if (x1 >= 0 && x1 < bp->grid_w && y1 >= 0 && y1 < bp->grid_h)  \
            h0->neighbors[(I)] = &grid [y1 * bp->grid_w + x1];           \
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

static Bool
empty_hexagon_p (hexagon *h)
{
  int i;
  for (i = 0; i < 6; i++)
    if (h->arms[i].state != EMPTY)
      return False;
  return True;
}

static int
add_arms (ModeInfo *mi, hexagon *h0)
{
  hextrail_configuration *bp = &bps[MI_SCREEN(mi)];
  int i;
  int added = 0;
  int target = 1 + (random() % 4);	/* Aim for 1-4 arms */

  int idx[6] = {0, 1, 2, 3, 4, 5};	/* Traverse in random order */
  for (i = 0; i < 6; i++)
    {
      int j = random() % 6;
      int swap = idx[j];
      idx[j] = idx[i];
      idx[i] = swap;
    }

  for (i = 0; i < 6; i++)
    {
      int j = idx[i];
      hexagon *h1 = h0->neighbors[j];
      arm *a0 = &h0->arms[j];
      arm *a1;
      if (!h1) continue;			/* No neighboring cell */
      if (h1->border_state != EMPTY) continue;	/* Occupado */
      if (a0->state != EMPTY) continue;		/* Incoming arm */

      a1 = &h1->arms[(j + 3) % 6];		/* Opposite arm */

      if (a1->state != EMPTY) continue;
      a0->state = OUT;
      a0->ratio = 0;
      a1->ratio = 0;
      a0->speed = 0.05 * speed * (0.8 + frand(1.0));
      a1->speed = a0->speed;

      h1->border_state = IN;

      /* Mostly keep the same color */
      h1->ccolor = h0->ccolor;
      if (! (random() % 5))
        h1->ccolor = (h0->ccolor + 1) % bp->ncolors;

      bp->live_count++;
      added++;
      if (added >= target)
        break;
    }
  return added;
}

static void
tick_hexagons (ModeInfo *mi)
{
  hextrail_configuration *bp = &bps[MI_SCREEN(mi)];
  int i, j;

  /* Enlarge any still-growing arms.
   */
  for (i = 0; i < bp->grid_w * bp->grid_h; i++)
    {
      hexagon *h0 = &bp->hexagons[i];
      for (j = 0; j < 6; j++)
        {
          arm *a0 = &h0->arms[j];
          switch (a0->state) {
          case OUT:
            a0->ratio += a0->speed * speed;
            if (a0->ratio > 1)
              {
                /* Just finished growing from center to edge.
                   Pass the baton to this waiting neighbor. */
                hexagon *h1 = h0->neighbors[j];
                arm *a1 = &h1->arms[(j + 3) % 6];
                a0->state = DONE;
                a0->ratio = 1;
                a1->state = IN;
                a1->ratio = 0;
                a1->speed = a0->speed;
              }
            break;
          case IN:
            a0->ratio += a0->speed * speed;
            if (a0->ratio > 1)
              {
                /* Just finished growing from edge to center.
                   Look for any available exits. */
                if (add_arms (mi, h0)) {
                  bp->live_count--;
                  a0->state = DONE;
                  a0->ratio = 1;
                } else { // nub grow
                  a0->state = WAIT;
                  a0->ratio = ((a0->ratio - 1) * 5) + 1; a0->speed *= 5;
                }
              }
            break;
          case WAIT:
            a0->ratio += a0->speed * speed * (2 - a0->ratio);
            if (a0->ratio >= 1.999) {
              a0->state = DONE;
              a0->ratio = 1;
              bp->live_count--;
            }
          case EMPTY: case DONE:
            break;
          }
        }

      switch (h0->border_state) {
      case IN:
        h0->border_ratio += 0.05 * speed;
        if (h0->border_ratio >= 1)
          {
            h0->border_ratio = 1;
            h0->border_state = WAIT;
          }
        break;
      case OUT:
        h0->border_ratio -= 0.05 * speed;
        if (h0->border_ratio <= 0)
          {
            h0->border_ratio = 0;
            h0->border_state = DONE;
          }
      case WAIT:
        if (! (random() % 50))
          h0->border_state = OUT;
        break;
      case EMPTY:
      case DONE:
        break;
      }
    }

  /* Start a new cell growing.
   */
  if (bp->live_count <= 0)
    for (i = 0; i < (bp->grid_w * bp->grid_h) / 3; i++)
      {
        hexagon *h0;
        int x, y;
        if (bp->state == FIRST)
          {
            x = bp->grid_w / 2;
            y = bp->grid_h / 2;
            bp->state = DRAW;
            bp->fade_ratio = 1;
            scale_corners(mi);
          }
        else
          {
            x = random() % bp->grid_w;
            y = random() % bp->grid_h;
          }
        h0 = &bp->hexagons[y * bp->grid_w + x];
        if (empty_hexagon_p (h0) &&
            add_arms (mi, h0)) 
          break;
      }

  if (bp->live_count <= 0 && bp->state != FADE)
    {
      bp->state = FADE;
      bp->fade_ratio = 1;

      for (i = 0; i < bp->grid_w * bp->grid_h; i++)
        {
          hexagon *h = &bp->hexagons[i];
          if (h->border_state == IN || h->border_state == WAIT)
            h->border_state = OUT;
        }
    }
  else if (bp->state == FADE)
    {
      bp->fade_ratio -= 0.01 * speed;
      scale_corners(mi);
      if (bp->fade_ratio <= 0)
        {
          make_plane (mi);
          bp->state = FIRST;
          bp->fade_ratio = 1;
        }
    }
}

# define H 0.8660254037844386   /* sqrt(3)/2 */

static const XYZ corners[] = {{  0, -1,   0 },       /*      0      */
                              {  H, -0.5, 0 },       /*  5       1  */
                              {  H,  0.5, 0 },       /*             */
                              {  0,  1,   0 },       /*  4       2  */
                              { -H,  0.5, 0 },       /*      3      */
                              { -H, -0.5, 0 }};

static XYZ scaled_corners[6][4];

static void scale_corners(ModeInfo *mi) {
  hextrail_configuration *bp = &bps[MI_SCREEN(mi)];
#ifdef USE_SDL
  int count = 20; // Default count for web
#else
  int count = MI_COUNT(mi);
#endif
  GLfloat size = sqrt(3) / 3 / count;
  GLfloat margin = thickness * 0.4;
  GLfloat size1 = size * (1 - margin * 2);
  GLfloat size2 = size * (1 - margin * 3);
  GLfloat thick2 = thickness * bp->fade_ratio;
  GLfloat size3 = size * thick2 * 0.8;
  GLfloat size4 = size3 * 2; // when total_arms == 1
  
  int i;
  for (i = 0; i < 6; i++) {
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

static void
draw_hexagons (ModeInfo *mi)
{
  hextrail_configuration *bp = &bps[MI_SCREEN(mi)];
  int wire = 0; // No wireframe for web
  GLfloat length = sqrt(3) / 3;
#ifdef USE_SDL
  int count = 20; // Default count for web
#else
  int count = MI_COUNT(mi);
#endif
  GLfloat size = length / count;
  GLfloat margin = thickness * 0.4;
  GLfloat size2 = size * (1 - margin * 3);
  GLfloat thick2 = thickness * bp->fade_ratio;
  int i;

  glFrontFace (GL_CCW);
  glBegin (wire ? GL_LINES : GL_TRIANGLES);
  glNormal3f (0, 0, 1);

  for (i = 0; i < bp->grid_w * bp->grid_h; i++)
    {
      hexagon *h = &bp->hexagons[i];
      int total_arms = 0;
      GLfloat color[4];
      GLfloat nub_ratio = 0;
      int j;

      for (j = 0; j < 6; j++)
        {
          arm *a = &h->arms[j];
          if (a->state == OUT || a->state == DONE || a->state == WAIT) {
            total_arms++;
            if (a->state == WAIT)
              nub_ratio = a->ratio;
          }
        }

#ifdef USE_SDL
      color[0] = bp->colors[h->ccolor].r / 255.0f * bp->fade_ratio;
      color[1] = bp->colors[h->ccolor].g / 255.0f * bp->fade_ratio;
      color[2] = bp->colors[h->ccolor].b / 255.0f * bp->fade_ratio;
      color[3] = 1.0f;
#else
      color[0] = bp->colors[h->ccolor].red   / 65535.0 * bp->fade_ratio;
      color[1] = bp->colors[h->ccolor].green / 65535.0 * bp->fade_ratio;
      color[2] = bp->colors[h->ccolor].blue  / 65535.0 * bp->fade_ratio;
      color[3] = 1;
#endif

      for (j = 0; j < 6; j++)
        {
          arm *a = &h->arms[j];
          int k = (j + 1) % 6;
          XYZ p[6];

          if (h->border_state != EMPTY && h->border_state != DONE)
            {
              GLfloat color1[4];
              memcpy (color1, color, sizeof(color1));
              color1[0] *= h->border_ratio;
              color1[1] *= h->border_ratio;
              color1[2] *= h->border_ratio;

              glColor4fv (color1);
              glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, color1);

              /* Outer edge of hexagon border */
              p[0].x = h->pos.x + scaled_corners[j][0].x;
              p[0].y = h->pos.y + scaled_corners[j][0].y;
              p[0].z = h->pos.z;

              p[1].x = h->pos.x + scaled_corners[k][0].x;
              p[1].y = h->pos.y + scaled_corners[k][0].y;
              p[1].z = h->pos.z;

              /* Inner edge of hexagon border */
              p[2].x = h->pos.x + scaled_corners[k][1].x;
              p[2].y = h->pos.y + scaled_corners[k][1].y;
              p[2].z = h->pos.z;
              p[3].x = h->pos.x + scaled_corners[j][1].x;
              p[3].y = h->pos.y + scaled_corners[j][1].y;
              p[3].z = h->pos.z;

              glVertex3f (p[0].x, p[0].y, p[0].z);
              glVertex3f (p[1].x, p[1].y, p[1].z);
              if (! wire)
                glVertex3f (p[2].x, p[2].y, p[2].z);

              glVertex3f (p[2].x, p[2].y, p[2].z);
              glVertex3f (p[3].x, p[3].y, p[3].z);
              if (! wire)
                glVertex3f (p[0].x, p[0].y, p[0].z);
            }

          /* Line from center to edge, or edge to center.
           */
          if (a->state == IN || a->state == OUT || a->state == DONE || a->state == WAIT)
            {
              GLfloat x   = (corners[j].x + corners[k].x) / 2;
              GLfloat y   = (corners[j].y + corners[k].y) / 2;
              GLfloat xoff = corners[k].x - corners[j].x;
              GLfloat yoff = corners[k].y - corners[j].y;
              GLfloat line_length = (a->state == WAIT) ? 1 : a->ratio;
              GLfloat start, end;
              GLfloat ncolor[4];
              GLfloat color1[4];
              GLfloat color2[4];

              /* Color of the outer point of the line is average color of
                 this and the neighbor. */
#ifdef USE_SDL
              ncolor[0] = bp->colors[h->neighbors[j]->ccolor].r / 255.0f * bp->fade_ratio;
              ncolor[1] = bp->colors[h->neighbors[j]->ccolor].g / 255.0f * bp->fade_ratio;
              ncolor[2] = bp->colors[h->neighbors[j]->ccolor].b / 255.0f * bp->fade_ratio;
              ncolor[3] = 1.0f;
#else
              ncolor[0] = bp->colors[h->neighbors[j]->ccolor].red   / 65535.0 * bp->fade_ratio;
              ncolor[1] = bp->colors[h->neighbors[j]->ccolor].green / 65535.0 * bp->fade_ratio;
              ncolor[2] = bp->colors[h->neighbors[j]->ccolor].blue  / 65535.0 * bp->fade_ratio;
              ncolor[3] = 1;
#endif
              ncolor[0] = (ncolor[0] + color[0]) / 2;
              ncolor[1] = (ncolor[1] + color[1]) / 2;
              ncolor[2] = (ncolor[2] + color[2]) / 2;
              ncolor[3] = (ncolor[3] + color[3]) / 2;

              if (a->state == OUT)
                {
                  start = 0;
                  end = size * line_length;
                  memcpy (color1, color,  sizeof(color1));
                  memcpy (color2, ncolor, sizeof(color1));
                }
              else
                {
                  start = size;
                  end = size * (1 - line_length);
                  memcpy (color1, ncolor, sizeof(color1));
                  memcpy (color2, color,  sizeof(color1));
                }

              if (! h->neighbors[j]) continue;  /* arm/neighbor mismatch */

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

              glColor4fv (color2);
              glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, color2);
              glVertex3f (p[3].x, p[3].y, p[3].z);
              glColor4fv (color1);
              glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, color1);
              glVertex3f (p[0].x, p[0].y, p[0].z);
              if (! wire)
                glVertex3f (p[1].x, p[1].y, p[1].z);

              glVertex3f (p[1].x, p[1].y, p[1].z);

              glColor4fv (color2);
              glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, color2);
              glVertex3f (p[2].x, p[2].y, p[2].z);
              if (! wire)
                glVertex3f (p[3].x, p[3].y, p[3].z);
            }

          /* Hexagon (one triangle of) in center to hide line miter/bevels.
           */
          if (total_arms && a->state != DONE && a->state != OUT)
            {
              p[0] = h->pos; p[1].z = h->pos.z; p[2].z = h->pos.z;

              if (nub_ratio) {
                p[1].x = h->pos.x + scaled_corners[j][2].x * nub_ratio;
                p[1].y = h->pos.y + scaled_corners[j][2].y * nub_ratio;
                p[2].x = h->pos.x + scaled_corners[k][2].x * nub_ratio;
                p[2].y = h->pos.y + scaled_corners[k][2].y * nub_ratio;
              } else {
                int8_t s = (total_arms == 1) ? 3 : 2;
                p[1].x = h->pos.x + scaled_corners[j][s].x;
                p[1].y = h->pos.y + scaled_corners[j][s].y;
                p[2].x = h->pos.x + scaled_corners[k][s].x;
                p[2].y = h->pos.y + scaled_corners[k][s].y;
              }

              glColor4fv (color);
              glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, color);
              if (! wire)
                glVertex3f (p[0].x, p[0].y, p[0].z);
              glVertex3f (p[1].x, p[1].y, p[1].z);
              glVertex3f (p[2].x, p[2].y, p[2].z);
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

#ifdef USE_SDL
  GLfloat s = 1.0; // Default scale for web
#else
  GLfloat s = (MI_WIDTH(mi) < MI_HEIGHT(mi)
               ? (MI_WIDTH(mi) / (GLfloat) MI_HEIGHT(mi))
               : 1);
#endif
  glScalef (s, s, s);

  glClear(GL_COLOR_BUFFER_BIT);
}

#ifdef USE_SDL
// Web-specific event handling
EMSCRIPTEN_KEEPALIVE
void hextrail_handle_event(int event_type, int key_code) {
    // Handle web events here if needed
}
#else
ENTRYPOINT Bool
hextrail_handle_event (ModeInfo *mi, XEvent *event)
{
  hextrail_configuration *bp = &bps[MI_SCREEN(mi)];
  if (gltrackball_event_handler (event, bp->trackball,
                                 MI_WIDTH (mi), MI_HEIGHT (mi),
                                 &bp->button_down_p))
    return True;
  else if (event->xany.type == KeyPress)
    {
      KeySym keysym;
      char c = 0;
      XLookupString (&event->xkey, &c, 1, &keysym, 0);

      if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
        ;
      else if (c == '>' || c == '.' || c == '+' || c == '=' ||
               keysym == XK_Right || keysym == XK_Up || keysym == XK_Next)
        {
          MI_COUNT(mi)++;
          scale_corners(mi);
        }
      else if (c == '<' || c == ',' || c == '-' || c == '_' ||
               c == '\010' || c == '\177' ||
               keysym == XK_Left || keysym == XK_Down || keysym == XK_Prior)
        {
          MI_COUNT(mi)--;
          scale_corners(mi);
        }
      else if (screenhack_event_helper (MI_DISPLAY(mi), MI_WINDOW(mi), event))
        ;
      else
        return False;

    RESET:
      if (MI_COUNT(mi) < 1) MI_COUNT(mi) = 1;
      scale_corners(mi);
      free (bp->hexagons);
      bp->hexagons = 0;
      bp->state = FIRST;
      bp->fade_ratio = 1;
      bp->live_count = 0;
      make_plane (mi);
      return True;
    }
  else if (screenhack_event_helper (MI_DISPLAY(mi), MI_WINDOW(mi), event))
    goto RESET;

  return False;
}
#endif

ENTRYPOINT void 
init_hextrail (ModeInfo *mi)
{
  hextrail_configuration *bp;

  MI_INIT (mi, bps);

  bp = &bps[MI_SCREEN(mi)];

#ifdef USE_SDL
  // Initialize SDL if not already done
  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
    return;
  }

  // Set OpenGL attributes
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);

  // Create window
  bp->window = SDL_CreateWindow("HexTrail", 
                               SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                               800, 600, 
                               SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
  if (!bp->window) {
    printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
    return;
  }

  // Create OpenGL context
  bp->gl_context = SDL_GL_CreateContext(bp->window);
  if (!bp->gl_context) {
    printf("OpenGL context could not be created! SDL_Error: %s\n", SDL_GetError());
    return;
  }

  bp->window_width = 800;
  bp->window_height = 600;
#else
  bp->glx_context = init_GL(mi);
#endif

  reshape_hextrail (mi, 800, 600);

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

  init_defaults();
  make_plane (mi);
  bp->state = FIRST;
  bp->fade_ratio = 1;
}

ENTRYPOINT void
draw_hextrail (ModeInfo *mi)
{
  hextrail_configuration *bp = &bps[MI_SCREEN(mi)];

#ifdef USE_SDL
  if (!bp->window || !bp->gl_context)
    return;

  SDL_GL_MakeCurrent(bp->window, bp->gl_context);
#else
  if (!bp->glx_context)
    return;

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

  {
    GLfloat s = 18;
    glScalef (s, s, s);
  }

  if (! bp->button_down_p)
    tick_hexagons (mi);
  draw_hexagons (mi);

  glPopMatrix ();

#ifdef USE_SDL
  SDL_GL_SwapWindow(bp->window);
#else
  if (mi->fps_p) do_fps (mi);
  glFinish();
  glXSwapBuffers(MI_DISPLAY(mi), MI_WINDOW(mi));
#endif
}

ENTRYPOINT void
free_hextrail (ModeInfo *mi)
{
  hextrail_configuration *bp = &bps[MI_SCREEN(mi)];

#ifdef USE_SDL
  if (bp->gl_context) SDL_GL_DeleteContext(bp->gl_context);
  if (bp->window) SDL_DestroyWindow(bp->window);
  SDL_Quit();
#else
  if (!bp->glx_context) return;
  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *bp->glx_context);
#endif

  if (bp->trackball) gltrackball_free (bp->trackball);
  if (bp->rot) free_rotator (bp->rot);
  if (bp->colors) free (bp->colors);
  free (bp->hexagons);
}

#ifdef USE_SDL
// Web-specific main function
EMSCRIPTEN_KEEPALIVE
int main() {
    ModeInfo mi;
    memset(&mi, 0, sizeof(mi));
    mi.screen = 0;
    
    init_hextrail(&mi);
    return 0;
}
#else
XSCREENSAVER_MODULE ("HexTrail", hextrail)
#endif

#endif /* USE_GL */ 