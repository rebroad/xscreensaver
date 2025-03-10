/* hextrail, Copyright (c) 2022 Jamie Zawinski <jwz@jwz.org>
 *
 * high-priority TODOs:-
 * low-priority TODOs:-
 = Collect all vertices into a dynamic array, then upload to 1 or more VBOs - also fix glow effects.
 = Don't draw borders when the thickness of the line would be <1 pixel
 = need an option to make it fill all active displays (wasn't -root supposed to do this?)
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
#endif // +Win32
#endif // USE_SDL

/* Define GL_GLEXT_PROTOTYPES before any GL includes to expose function prototypes */
#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES
#endif

#include <GL/gl.h>
#include <GL/glu.h>
#include "xlockmore.h"
#include "fpsI.h"

/* Include glext.h unconditionally to define GL_VERSION_2_0 */
#ifdef HAVE_GL_GLEXT_H
#include <GL/glext.h>
#elif defined(HAVE_OPENGL_GLEXT_H)
#include <OpenGL/glext.h>
#else
#include <GL/glext.h>
#endif

/* Now check for GL_VERSION_2_0 which should be defined in glext.h */
/* GL_VERSION_2_0 should be defined in glext.h */
#include "colors.h"
#include "normals.h"
#include "rotator.h"
#include "gltrackball.h"
#include <ctype.h>
#include <math.h>
#include <time.h>

#ifdef USE_GL /* whole file */

#ifdef GL_VERSION_2_0
/* Vertex shader source */
static const char *vertex_shader_source =
"#version 120\n"
"attribute vec3 position;\n"
"attribute vec4 color;\n"
"varying vec4 v_color;\n"
"void main() {\n"
"    gl_Position = gl_ModelViewProjectionMatrix * vec4(position, 1.0);\n"
"    v_color = color;\n"
"}\n";

/* Fragment shader source with glow effect */
static const char *fragment_shader_source =
"#version 120\n"
"varying vec4 v_color;\n"
"uniform int use_glow;\n"
"uniform sampler2D tex;\n"
"uniform vec2 resolution;\n"
"void main() {\n"
"    vec4 final_color = v_color;\n"
"    if (use_glow == 1) {\n"
"        vec2 uv = gl_FragCoord.xy / resolution;\n"
"        vec4 base_color = texture2D(tex, uv);\n" // Sample from framebuffer
"        if (base_color.a < 0.1) discard;\n" // Skip near-transparent areas
"        float glow = 0.0;\n"
"        float radius = 0.02;\n" // Glow radius in screen space
"        for (int x = -4; x <= 4; x++) for (int y = -4; y <= 4; y++) {\n"
"            vec2 offset = vec2(float(x), float(y)) * radius;\n"
"            float dist = length(offset);\n"
"            float weight = exp(-dist * dist * 4.0);\n" // Sharper falloff
"            glow += texture2D(tex, uv + offset).a * weight;\n"
"        }\n"
"        glow /= 32.0;\n" // Normalize (9x9 kernel)
"        vec3 glow_color = base_color.rgb * 1.5;\n" // Brighten glow
"    final_color = vec4(v_color.rgb + glow_color * glow, base_color.a);\n"
"    }\n"
"    gl_FragColor = final_color;\n"
"}\n";

/* Check if OpenGL version supports shaders */
static int check_gl_version(void) {
	const char *version = (const char *)glGetString(GL_VERSION);
	int major, minor;

	if (version) printf("%s: GL version: %s\n", __func__, version);

	if (!version || sscanf(version, "%d.%d", &major, &minor) != 2)
		return 0;

	if (major < 2)  /* Need at least OpenGL 2.0 for shader support */
		return 0;

	return 1;
}

/* Compile a shader from source */
static GLuint compile_shader(const char *source, GLenum shader_type) {
	GLuint shader = glCreateShader(shader_type);
	if (!shader) {
		fprintf(stderr, "Error creating shader\n");
		return 0;
	}

	glShaderSource(shader, 1, &source, NULL);
	glCompileShader(shader);

	GLint status;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
	if (!status) {
		GLchar info_log[512];
		glGetShaderInfoLog(shader, sizeof(info_log), NULL, info_log);
		fprintf(stderr, "Shader compilation error: %s\n", info_log);
		glDeleteShader(shader);
		return 0;
	}

	return shader;
}

/* Link vertex and fragment shaders into a program */
static GLuint link_program(GLuint vertex_shader, GLuint fragment_shader) {
	GLuint program = glCreateProgram();
	if (!program) {
		fprintf(stderr, "Error creating shader program\n");
		return 0;
	}

	glAttachShader(program, vertex_shader);
	glAttachShader(program, fragment_shader);
	glLinkProgram(program);

	GLint status;
	glGetProgramiv(program, GL_LINK_STATUS, &status);
	if (!status) {
		GLchar info_log[512];
		glGetProgramInfoLog(program, sizeof(info_log), NULL, info_log);
		fprintf(stderr, "Shader program linking error: %s\n", info_log);
		glDeleteProgram(program);
		return 0;
	}

	return program;
}
#endif /* GL_VERSION_2_0 */

#define DEF_SPIN        "True"
#define DEF_WANDER      "True"
#define DEF_GLOW        "False"
#define DEF_NEON        "False"
#define DEF_EXPAND      "False"
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
#ifdef USE_SDL
  SDL_Window *window;
  SDL_GLContext gl_context;
  SDL_Color *colors;
#else // USE_SDL
  GLXContext *glx_context;
  XColor *colors;
#endif // else USE_SDL
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

#ifdef GL_VERSION_2_0
  GLuint shader_program, vertex_shader, fragment_shader;
  GLuint vertex_buffer, vertex_array, fbo, texture;
#endif // GL_VERSION_2_0
} config;

static config *bps = NULL;

#ifdef GL_VERSION_2_0
/* Vertex structure for vertex-based rendering */
typedef struct { GLfloat x, y, z, r, g, b, a; } Vertex;

Vertex *vertices;
int vertex_capacity, vertex_count;

/* Current color tracking for shader mode */
static GLfloat current_color[4] = {1.0, 1.0, 1.0, 1.0};

/* Wrapper function for setting color in both shader and non-shader modes */
static void set_color(GLfloat r, GLfloat g, GLfloat b, GLfloat a) {
	/* Store current color for shader path */
	current_color[0] = r;
	current_color[1] = g;
	current_color[2] = b;
	current_color[3] = a;
}
#endif // GL_VERSION_2_0

/* Wrapper function for setting color from an array */
static void set_color_v(GLfloat *color) {
#ifdef GL_VERSION_2_0
	set_color(color[0], color[1], color[2], color[3]);
#else // GL_VERSION_2_0
	glColor4fv(color);
	glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, color);
#endif // else GL_VERSION_2_0
}

#ifdef GL_VERSION_2_0
static void add_vertex(GLfloat x, GLfloat y, GLfloat z,
					   GLfloat r, GLfloat g, GLfloat b, GLfloat a) {
	/* Check if vertices array exists */
	if (!vertices) {
		/* Fall back to immediate mode */
		glColor4f(r, g, b, a);
		glVertex3f(x, y, z);
		return;
	}

	/* Resize if needed */
	if (vertex_count >= vertex_capacity) {
		vertex_capacity *= 2;
		vertices = (Vertex *)realloc(vertices, vertex_capacity * sizeof(Vertex));
		if (!vertices) {
			/* Fall back to immediate mode */
			glColor4f(r, g, b, a);
			glVertex3f(x, y, z);
			return;
		}
	}

	/* Add the vertex */
	vertices[vertex_count].x = x;
	vertices[vertex_count].y = y;
	vertices[vertex_count].z = z;
	vertices[vertex_count].r = r;
	vertices[vertex_count].g = g;
	vertices[vertex_count].b = b;
	vertices[vertex_count].a = a;
	vertex_count++;
}
#endif // GL_VERSION_2_0

static void do_vertex(GLfloat x, GLfloat y, GLfloat z) {
#ifdef GL_VERSION_2_0
	add_vertex(x, y, z, current_color[0], current_color[1],
			  current_color[2], current_color[3]);
#else // GL_VERSION_2_0
	glVertex3f(x, y, z);
#endif // else GL_VERSION_2_0
}

/* Initialize shader program */
static int init_shaders(config *bp) {
	bp->vertex_shader = compile_shader(vertex_shader_source, GL_VERTEX_SHADER);
	if (!bp->vertex_shader)
		return 0;

	bp->fragment_shader = compile_shader(fragment_shader_source, GL_FRAGMENT_SHADER);
	if (!bp->fragment_shader) {
		glDeleteShader(bp->vertex_shader);
		bp->vertex_shader = 0;
		return 0;
	}

	bp->shader_program = link_program(bp->vertex_shader, bp->fragment_shader);
	if (!bp->shader_program) {
		glDeleteShader(bp->vertex_shader);
		glDeleteShader(bp->fragment_shader);
		bp->vertex_shader = bp->fragment_shader = 0;
		return 0;
	}

	return 1;
}

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
int q, qq, N;

static uint16_t xy_to_index(int x, int y) {
	if (abs(x) > q || abs(y) > q || abs(x-y) > q) return 0;
	int32_t index;
	if (y < 1) index = x + qq + erm(y + N) - erm(q - 1) + 1;
	else index = x + y + qq + erm(y + N) - erm(q - 1) + 1;

	return (uint16_t)index;
}

static hexagon *get_hex(config *bp, uint16_t id) {
  uint16_t idx = bp->hex_grid[id];
  if (!idx) return NULL;  // Empty cell
  int i = (idx-1) / HEXAGON_CHUNK_SIZE, k = (idx-1) % HEXAGON_CHUNK_SIZE;
  return bp->chunks[i]->chunk[k];
}

static hexagon *do_hexagon(config *bp, int8_t px, int8_t py, int8_t x, int8_t y) {
  uint16_t id = xy_to_index(x, y);
  if (!id) {
	static time_t debug = 0;
	if (debug != bp->now) {
	  printf("%s: Out of bounds. id=%d coords=%d,%d\n", __func__, id, x, y);
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

  h0 = calloc(1, sizeof(hexagon));
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
  //printf("New hex at %d,%d (parent %d,%d)\n", x, y, px, py);

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

static int8_t exits(config *bp, hexagon *h0) {
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

static int8_t add_arms(config *bp, hexagon *h0) {
  int8_t added = 0, target = (random() % 4); /* Aim for 1-5 arms */
  int8_t idx[6] = {0, 1, 2, 3, 4, 5};
  for (int8_t i = 0; i < 6; i++) {
	int8_t j = random() % (6 - i);
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
		fprintf(stderr, "%s: Failed to create hex arm=%d from %d,%d\n", __func__, j, h0->x, h0->y);
		continue;
	}
	if (h1->state != EMPTY) continue;	/* Occupado */

	arm *a1 = &h1->arms[(j + 3) % 6];	/* Opposite arm */

	if (a1->state != EMPTY) {
	  printf("H1 (%d,%d) empty=%d arm[%d].state=%d\n",
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
	  printf("H0 (%d,%d) arm%d out to H1 (%d,%d)->state=%d\n",
h0->x, h0->y, j, h1->x, h1->y, h1->state);

	added++;
	if (added >= target) break;
  }
  h0->doing = added;

  return added;
}

# undef H
# define H 0.8660254037844386   /* sqrt(3)/2 */

/* Check if a hexagon is within the visible frustum using bounding circle test */
static Bool hex_invis(config *bp, int x, int y, GLfloat *rad) {
  GLfloat wid = 2.0 / bp->size;
  GLfloat hgt = wid * H;
  XYZ pos = { x * wid - y * wid / 2, y * hgt, 0 };
  GLdouble winX, winY, winZ;

  /* Project point to screen coordinates */
  gluProject((GLdouble)pos.x, (GLdouble)pos.y, (GLdouble)pos.z,
			 bp->model, bp->proj, bp->viewport, &winX, &winY, &winZ);

  static time_t debug = 0;
  if (debug != bp->now) {
	  printf("%s: winX=%d, winY=%d, winZ=%.1f vp=%d,%d\n", __func__,
			  (int)winX, (int)winY, winZ, bp->viewport[2], bp->viewport[3]);
	  debug = bp->now;
  }

  if (winZ <= 0 || winZ >= 1) return 2;  // Far off in Z

  /* Calculate bounding circle radius in screen space */
  XYZ edge_posx = pos, edge_posy = pos;
  edge_posx.x += wid/2; edge_posy.y += hgt/2;

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
GLfloat size, wid, hgt, size1, size2, size3, size4, thick2;

static void scale_corners(ModeInfo *mi) {
  config *bp = &bps[MI_SCREEN(mi)];
  size = (H * 2 / 3) / MI_COUNT(mi);
  wid = 2.0 / bp->size;
  hgt = wid * H;
  GLfloat margin = thickness * 0.4;
  size1 = size * (1 - margin * 2);
  size2 = size * (1 - margin * 3);
  thick2 = thickness * bp->fade_ratio;
  size3 = size * thick2 * 0.8;
  size4 = size3 * 2; // When total_arms == 1
  for (int j = 0; j < 6; j++) {
	scaled_corners[j][0].x = corners[j].x * size1;
	scaled_corners[j][0].y = corners[j].y * size1;
	scaled_corners[j][1].x = corners[j].x * size2;
	scaled_corners[j][1].y = corners[j].y * size2;
	scaled_corners[j][2].x = corners[j].x * size3;
	scaled_corners[j][2].y = corners[j].y * size3;
	scaled_corners[j][3].x = corners[j].x * size4;
	scaled_corners[j][3].y = corners[j].y * size4;
  }
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

  memset(bp->hex_grid, 0, (N*(N+1)*3+2) * sizeof(uint16_t));
  bp->total_hexagons = 0;
  bp->state = FIRST;
  bp->fade_ratio = 1;
  scale_corners(mi);

  bp->ncolors = 8;
  if (!bp->colors)
#ifdef USE_SDL
	bp->colors = (SDL_Color *) calloc(bp->ncolors, sizeof(SDL_Color));
  make_smooth_colormap(bp->colors, &bp->ncolors, False, 0, False);
#else // USE_SDL
	bp->colors = (XColor *) calloc(bp->ncolors, sizeof(XColor));
  make_smooth_colormap (0, 0, 0, bp->colors, &bp->ncolors, False, 0, False);
#endif // else USE_SDL
}

static void handle_arm_out(config *bp, hexagon *h0, arm *a0, int j) {
  a0->ratio += a0->speed * speed;
  if (a0->ratio >= 1) {
	/* Just finished growing from center to edge.
	   Pass the baton to this waiting neighbor. */
	hexagon *h1 = get_hex(bp, neighbor(h0->x, h0->y, j, NULL, NULL));
	arm *a1 = &h1->arms[(j + 3) % 6];
	a1->state = IN;
	a1->ratio = a0->ratio - 1;
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

static void handle_arm_in(config *bp, hexagon *h0, arm *a0, int j) {
  a0->ratio += a0->speed * speed;
  if (a0->ratio >= 1) {
	h0->doing = 0;
	/* Just finished growing from edge to center.  Look for any available exits. */
	if (add_arms(bp, h0)) {
	  a0->state = DONE;
	  a0->ratio = 1;
	} else { // nub grow
	  a0->state = WAIT;
	  a0->ratio = ((a0->ratio - 1) * 5) + 1; a0->speed *= 5;
	  h0->doing = 1;
	}
  } else if (a0->ratio >= 0.9 && a0->speed < 0.2 && (a0->speed > 0.1 || !exits(bp, h0))) {
	//printf("%s: %d,%d speed=%f->", __func__, h0->x, h0->y, a0->speed);
	a0->speed *= 1.5;
	//printf("%f\n", a0->speed);
  }
}

static void handle_nub_grow(config *bp, hexagon *h0, arm *a0, int j) {
  a0->ratio += a0->speed * (2 - a0->ratio);
  if (a0->ratio >= 1.999) {
	a0->state = DONE;
	h0->doing = 0;
	a0->ratio = 1;
  }
}

typedef void (*state_handler)(config *bp, hexagon *h0, arm *a0, int j);
state_handler arm_handlers[] = {
  [EMPTY] = NULL,
  [OUT] = handle_arm_out,
  [IN] = handle_arm_in,
  [WAIT] = handle_nub_grow,
  [DONE] = NULL
};

static void tick_hexagons (ModeInfo *mi) {
  config *bp = &bps[MI_SCREEN(mi)];
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
		printf("h0->state = %d\n", h0->state);
		abort(); break;
	}
  } // Loop through each hexagon

  min_vx = this_min_vx; max_vx = this_max_vx; min_vy = this_min_vy; max_vy = this_max_vy;

  if (bp->now > bp->debug) {
	printf("tps=%d doinga=%d ignorea=%d doingb=%d ignoreb=%d\n", tps, doinga, ignorea, doingb, ignoreb);
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
	  printf("\n\n\n\n\nNew hextrail. capacity=%d\n", bp->hexagon_capacity);
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
		new_hex = True; printf("Empty_count = %d picked=%d,%d\n", empty_count, x, y);
		do_hexagon(bp, 0, 0, x, y);
	  }
	}
	if (!h0) {
	  static time_t debug = 0;
	  if (debug != bp->now) {
		printf("%s: !h0 new=%d doinga=%d,%d doingb=%d,%d fails=%d\n", __func__,
				new_hex, doinga, ignorea, doingb, ignoreb, fails);
		debug = bp->now;
	  }
	} else if (h0->state == EMPTY && add_arms(bp, h0)) {
	  printf("hex created. Arms. doing=%d iters=%d fails=%d pos=%d,%d\n",
		  h0->doing, iters, fails, h0->x, h0->y);
	  fails = 0;
	  h0->ccolor = random() % bp->ncolors;
	  h0->state = DONE;
	  started = True;
	  if (new_hex) bp->pause_until = bp->now + 5;
	} else {
	  fails++;
	  static time_t debug = 0;
	  if (debug != bp->now) {
		printf("hexagon created. doing=%d fails=%d h0=%d,%d empty=%d visible=%d\n",
			h0->doing, fails, h0->x, h0->y, h0->state == EMPTY, !h0->invis);
		debug = bp->now;
	  }
	}
  }

  if (new_hex && (started || doinga != ignorea))
	printf("New cell: started=%d doinga=%d ignorea=%d doingb=%d ignoreb=%d\n",
			started, doinga, ignorea, doingb, ignoreb);

  if (!started && (doinga - ignorea) < 1 && (doingb - ignoreb) < 1 && bp->state != FADE) {
	printf("Fade started. iters=%d doinga=%d doingb=%d ignorea=%d ignoreb=%d\n",
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
	  printf("Fade ended.\n");
	  reset_hextrail (mi);
	} else if (bp->fade_ratio >= 1) {
	  bp->fade_ratio = 1; bp->fade_speed = 0;
	  bp->state = DRAW; pause_fade = True;
	}
  }
}

#ifdef USE_SDL
  # define HEXAGON_COLOR(V,H) do { \
	int idx = (H)->ccolor; \
	(V)[0] = bp->colors[idx].r / 255.0f * bp->fade_ratio; \
	(V)[1] = bp->colors[idx].g / 255.0f * bp->fade_ratio; \
	(V)[2] = bp->colors[idx].b / 255.0f * bp->fade_ratio; \
	(V)[3] = bp->colors[idx].a / 255.0f; \
  } while (0)
#else // USE_SDL
  # define HEXAGON_COLOR(V,H) do { \
	(V)[0] = bp->colors[(H)->ccolor].red   / 65535.0 * bp->fade_ratio; \
	(V)[1] = bp->colors[(H)->ccolor].green / 65535.0 * bp->fade_ratio; \
	(V)[2] = bp->colors[(H)->ccolor].blue  / 65535.0 * bp->fade_ratio; \
	(V)[3] = 1; \
  } while (0)
#endif // else USE_SDL

#ifdef GL_VERSION_2_0
/* Render accumulated vertices using shaders */
static void render_vertices(ModeInfo *mi, config *bp, int wire) {
	if (vertex_count == 0) return;

	/* Use shader program */
	glUseProgram(bp->shader_program);

	/* Set uniform variables */
	GLint resolution_loc = glGetUniformLocation(bp->shader_program, "resolution");
	GLint use_glow_loc = glGetUniformLocation(bp->shader_program, "use_glow");
	GLint tex_loc = glGetUniformLocation(bp->shader_program, "tex");

	if (resolution_loc != -1) glUniform2f(resolution_loc, MI_WIDTH(mi), MI_HEIGHT(mi));
	if (use_glow_loc != -1) glUniform1i(use_glow_loc, (do_glow || do_neon) ? 1 : 0);
	if (tex_loc != -1) glUniform1i(tex_loc, 0); // Use texture unit 0

	/* Update vertex buffer with accumulated vertices */
	glBindBuffer(GL_ARRAY_BUFFER, bp->vertex_buffer);
	glBufferData(GL_ARRAY_BUFFER, vertex_count * sizeof(Vertex),
				 vertices, GL_STREAM_DRAW);

	/* Set up vertex attributes */
#ifdef GL_VERSION_3_0
	if (bp->vertex_array) glBindVertexArray(bp->vertex_array);
#endif // GL_VERSION_3_0

	/* Position attribute (x, y, z) */
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);

	/* Color attribute (r, g, b, a) */
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(3 * sizeof(GLfloat)));

	/* Draw the vertices */
	glDrawArrays(wire ? GL_LINES : GL_TRIANGLES, 0, vertex_count);

	/* Clean up */
	glDisableVertexAttribArray(0);
	glDisableVertexAttribArray(1);
#ifdef GL_VERSION_3_0
	if (bp->vertex_array) glBindVertexArray(0);
#endif // GL_VERSION_3_0
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glUseProgram(0);
}
#endif // GL_VERSION_2_0

static void draw_hexagons(ModeInfo *mi) {
  config *bp = &bps[MI_SCREEN(mi)];
  int wire = MI_IS_WIREFRAME(mi);

  mi->polygon_count = 0;

#ifdef GL_VERSION_2_0
  /* Reset vertex count */
  vertex_count = 0;
#else // GL_VERSION_2_0
  glFrontFace(GL_CCW);
  glBegin(wire ? GL_LINES : GL_TRIANGLES);
  glNormal3f(0, 0, 1);
#endif // else GL_VERSION_2_0

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
	  if (a->state == OUT || a->state == DONE || a->state == WAIT) {
		total_arms++;
		if (a->state == WAIT)
		  nub_ratio = a->ratio;
	  }
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

		set_color_v(color1);
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

		do_vertex(p[0].x, p[0].y, p[0].z);
		do_vertex(p[1].x, p[1].y, p[1].z);
		if (!wire) do_vertex(p[2].x, p[2].y, p[2].z);
		mi->polygon_count++;

		do_vertex(p[2].x, p[2].y, p[2].z);
		do_vertex(p[3].x, p[3].y, p[3].z);
		if (!wire) do_vertex(p[0].x, p[0].y, p[0].z);
		mi->polygon_count++;
	  }

	  /* Line from center to edge, or edge to center.  */
	  if (a->state == IN || a->state == OUT || a->state == DONE || a->state == WAIT) {
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
		hexagon *hn = get_hex(bp, neighbor(h->x, h->y, j, NULL, NULL));
		if (!hn) {
			printf("%s: h=%d,%d j=%d BAD NEIGHBOR\n", __func__, h->x, h->y, j);
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

		set_color_v(color2);
		do_vertex(p[3].x, p[3].y, p[3].z);
		set_color_v(color1);
		do_vertex(p[0].x, p[0].y, p[0].z);
		if (!wire) do_vertex(p[1].x, p[1].y, p[1].z);
		mi->polygon_count++;

		do_vertex(p[1].x, p[1].y, p[1].z);

		set_color_v(color2);
		do_vertex(p[2].x, p[2].y, p[2].z);
		if (!wire) do_vertex(p[3].x, p[3].y, p[3].z);
		mi->polygon_count++;
	  } // arm is IN, OUT or DONE

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

		set_color_v(color);
		if (!wire) do_vertex(p[0].x, p[0].y, p[0].z);
		do_vertex(p[1].x, p[1].y, p[1].z);
		do_vertex(p[2].x, p[2].y, p[2].z);
		mi->polygon_count++;
	  }
	} // loop through arms
  }

#ifdef GL_VERSION_2_0
  /* Render accumulated vertices with shaders */
  render_vertices(mi, bp, wire);
#else
  glEnd();
#endif // GL_VERSION_2_0
}


/* Window management, etc */
ENTRYPOINT void reshape_hextrail(ModeInfo *mi, int width, int height) {
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

  GLfloat s = (MI_WIDTH(mi) < MI_HEIGHT(mi) ? (MI_WIDTH(mi) / (GLfloat)MI_HEIGHT(mi)) : 1);
  glScalef (s, s, s); // TODO - what does this do?
  glClear(GL_COLOR_BUFFER_BIT);
}

ENTRYPOINT Bool hextrail_handle_event(ModeInfo *mi,
#ifdef USE_SDL
		SDL_Event *event
#else // USE_SDL
		XEvent *event
#endif // else USE_SDL
		) {
  config *bp = &bps[MI_SCREEN(mi)];

  if (gltrackball_event_handler (event, bp->trackball,
			  MI_WIDTH(mi), MI_HEIGHT(mi), &bp->button_down_p)) {
	if (bp->fade_speed > 0) {
	  bp->fade_speed = -bp->fade_speed;
	}
	return True;
  }
#ifdef USE_SDL
  else if (event->type == SDL_EVENT_KEY_DOWN) {
	SDL_Keycode keysym = event->key.key;
	char c = (char)event->key.key;
#else // USE_SDL
  else if (event->xany.type == KeyPress) {
	KeySym keysym;
	char c = 0;
	XLookupString (&event->xkey, &c, 1, &keysym, 0);
#endif // else USE_SDL

	if (c == '\t' || c == '\r' || c == '\n') ;
	else if (c == '>' || c == '.' ||
#ifdef USE_SDL
			keysym == SDLK_UP || keysym == SDLK_PAGEDOWN
#else // USE_SDL
			keysym == XK_Up || keysym == XK_Next
#endif // else USE_SDL
			) {
	  MI_COUNT(mi)--;
	  if (MI_COUNT(mi) < 1) MI_COUNT(mi) = 1;
	  bp->size = MI_COUNT(mi) * 2;
	  scale_corners(mi);
	} else if (
#ifdef USE_SDL
			keysym == SDLK_RIGHT
#else
			keysym == XK_Right
#endif
			) {
	  speed *= 2;
	  if (speed > 20) speed = 20;
	  printf("%s: speed = %f -> %f\n", __func__, speed/2, speed);
	} else if (
#ifdef USE_SDL
			keysym == SDLK_LEFT
#else
			keysym == XK_Left
#endif
	  ) {
	  speed /= 2;
	  if (speed < 0.0001) speed = 0.0001;
	  printf("%s: speed = %f -> %f\n", __func__, speed*2, speed);
	} else if (c == '<' || c == ',' || c == '_' ||
			   c == '\010' || c == '\177' ||
#ifdef USE_SDL
			keysym == SDLK_DOWN || keysym == SDLK_PAGEUP
#else
			keysym == XK_Down || keysym == XK_Prior
#endif
			) {
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
	} else if (c == 'p' || c == ' ') {
	  if (pause_fade) {
		pause_fade = False;
		printf("%s: pause_fade = %d hexagons=%d\n", __func__, pause_fade, bp->total_hexagons);
	  } else if (bp->fade_speed > 0) {
		bp->fade_speed = -bp->fade_speed;
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

ENTRYPOINT void init_hextrail(ModeInfo *mi) {
  MI_INIT (mi, bps);
  config *bp = &bps[MI_SCREEN(mi)];

#ifdef GL_VERSION_2_0
  /* Initialize vertex buffer */
  vertex_capacity = 10000;  /* Initial vertex capacity */
  vertex_count = 0;
  vertices = (Vertex *)malloc(vertex_capacity * sizeof(Vertex));

  if (!vertices) fprintf(stderr, "Failed to allocate vertices array\n");
#endif

#ifdef USE_SDL
  // SDL_GLContext is already created in main; store window for reference
  bp->window = mi->window;
  bp->gl_context = mi->gl_context;
  if (!bp->gl_context || !bp->window) {
	fprintf(stderr, "%s: Invalid SDL GL context or window\n", progname);
	exit(1);
  }
#else // USE_SDL
  bp->glx_context = init_GL(mi);
#endif // else USE_SDL

  /* Check if we can use shaders on this system */
  int use_shaders = check_gl_version();
  printf("%s: use_shaders = %d\n", __func__, use_shaders);

  reshape_hextrail(mi, MI_WIDTH(mi), MI_HEIGHT(mi));

  bp->rot = make_rotator(do_spin ? 0.002 : 0, do_spin ? 0.002 : 0,
						 do_spin ? 0.002 : 0, 1.0, // spin_accel
						 do_wander ? 0.003 : 0, False);
  bp->trackball = gltrackball_init(True);
  gltrackball_reset(bp->trackball, -0.4 + frand(0.8),
					 -0.4 + frand(0.8)); // TODO - half-reset each hextrail_reset

  if (thickness < 0.05) thickness = 0.05;
  if (thickness > 0.5) thickness = 0.5;

  bp->chunk_count = 0;

#ifdef GL_VERSION_2_0
  /* Initialize shaders if supported */
  if (!init_shaders(bp)) {
	fprintf(stderr, "Failed to initialize shaders, falling back to immediate mode\n");
	//bp->use_shaders = 0;
  } else {
	/* Set up vertex attributes */
	glGenBuffers(1, &bp->vertex_buffer);
	glGenFramebuffers(1, &bp->fbo);
	glGenTextures(1, &bp->texture);
	glBindTexture(GL_TEXTURE_2D, bp->texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, MI_WIDTH(mi), MI_HEIGHT(mi), 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glBindFramebuffer(GL_FRAMEBUFFER, bp->fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, bp->texture, 0);
#ifdef GL_VERSION_3_0
	glGenVertexArrays(1, &bp->vertex_array);
#else
	/* Fallback for systems without VAO support */
	bp->vertex_array = 0;
#endif

	/* Bind attributes */
	glBindAttribLocation(bp->shader_program, 0, "position");
	glBindAttribLocation(bp->shader_program, 1, "color");
  }
#endif

  bp->size = MI_COUNT(mi) * 2; N = bp->size * 2 + 1; // N should be odd
  if (N > MAX_N) {
	printf("%s: Limiting N (%d) to MAX_N (%d)\n", __func__, N, MAX_N);
	N = MAX_N;
  }
  bp->hex_grid = (uint16_t *)calloc(N*(N+1)*3+2, sizeof(uint16_t));
  q = (N + 1) / 2;
  qq = q * 2;
  printf("%s: N=%d q=%d qq=%d\n", __func__, N, q, qq);
  reset_hextrail(mi);
}


ENTRYPOINT void draw_hextrail (ModeInfo *mi) {
  config *bp = &bps[MI_SCREEN(mi)];

#ifdef USE_SDL
  if (SDL_GL_MakeCurrent(bp->window, bp->gl_context) < 0) {
	fprintf(stderr, "%s: SDL_GL_MakeCurrent failed: %s\n", progname, SDL_GetError());
	return;
  }
#else // USE_SDL
  if (!bp->glx_context) return;
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);
  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *bp->glx_context);
#endif // else USE_SDL

  glShadeModel(GL_SMOOTH);
  glDisable(GL_DEPTH_TEST);
  glEnable(GL_NORMALIZE);
  glDisable(GL_CULL_FACE);

#ifdef GL_VERSION_2_0
  if (do_glow || do_neon) {
	// First pass: Render scene to texture
	glBindFramebuffer(GL_FRAMEBUFFER, bp->fbo);
  }
#endif

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

  {
	GLfloat s = 18;
	glScalef (s, s, s);
  }

  bp->now = time(NULL);
  if (bp->pause_until < bp->now && !pausing) {
	glGetDoublev(GL_MODELVIEW_MATRIX, bp->model);
	glGetDoublev(GL_PROJECTION_MATRIX, bp->proj);
	tick_hexagons(mi);
  }

  draw_hexagons(mi);
  glPopMatrix();

#ifdef GL_VERSION_2_0
  if (do_glow || do_neon) {
	// Second pass: Render to screen with glow effect
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Activate texture with the rendered scene
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, bp->texture);

	// Draw scene again with glow effect
	glPushMatrix();
	draw_hexagons(mi);
	glPopMatrix();

	// Cleanup
	glBindTexture(GL_TEXTURE_2D, 0);
  }
#endif

  if (mi->fps_p) {
	/* If we're using shaders, add shader statistics to the FPS display */
#ifdef GL_VERSION_2_0
	fps_state *fps = mi->fpst;
	if (fps && fps->frame_count == 0) {
	  char shader_stats[100];

	  snprintf(shader_stats, sizeof(shader_stats), "\nVerts: %u", vertex_count);

	  /* Make sure we don't overflow the buffer */
	  if (strlen(fps->string) + strlen(shader_stats) < sizeof(fps->string)) {
		strcat(fps->string, shader_stats);
	  }
	}
#endif // GL_VERSION_2_0
	do_fps(mi);
  }
  glFinish(); // TODO do if using shaders?

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

#ifdef GL_VERSION_2_0
  /* Clean up shader resources */
  glDeleteBuffers(1, &bp->vertex_buffer);
#ifdef GL_VERSION_3_0
  if (bp->vertex_array) glDeleteVertexArrays(1, &bp->vertex_array);
#endif

  if (bp->shader_program) glDeleteProgram(bp->shader_program);
  if (bp->vertex_shader) glDeleteShader(bp->vertex_shader);
  if (bp->fragment_shader) glDeleteShader(bp->fragment_shader);

  /* Free vertex array */
  if (vertices) free(vertices);
#endif
}

XSCREENSAVER_MODULE ("HexTrail", hextrail)

#endif /* USE_GL */
