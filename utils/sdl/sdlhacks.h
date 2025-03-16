/* sdlhacks.h - SDL support for xscreensaver hacks
 *
 * Copyright (c) 2023-2024
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 */

#ifndef __SDLHACKS_H__
#define __SDLHACKS_H__

#ifdef USE_SDL

#include "sdlcompat.h"
#include "sdlgl.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef HAVE_RECORD_ANIM
#include <time.h>
#endif

/* Xlockmore compatibility types */
typedef struct XWindowAttributes {
    int width;
    int height;
    int screen;
    unsigned long background_pixel;
    unsigned long white_pixel;
    unsigned long black_pixel;
} XWindowAttributes;

typedef struct record_anim_state record_anim_state;

/* FPS state structure */
typedef struct fps_state {
    double start_time;
    double last_time;
    double current_time;
    double frames;
    double fps;
    int frame_count;
    unsigned long polygon_count;
    char string[40];
} fps_state;

/* ModeInfo structure compatible with xlockmore */
typedef struct ModeInfo {
    void *dpy;                  /* Display pointer (replace with SDL equivalent) */
    SDL_Window *window;     /* SDL window */
    void *gl_context;           /* OpenGL context */
    void *closure;              /* User data */
    int screen_number;                 /* Virtual screen number */
int batchcount;           /* For MI_COUNT compatibility */
    int fps_p;                  /* FPS display enabled */
    int count;                  /* Configuration value */
    int cycles;                 /* Configuration value */
    int recursion_depth;        /* Configuration value */
    int username_p;             /* Show username */
    int mouse_p;                /* Mouse enabled */
    int mousefn;                /* Mouse function */
    int threed;                 /* 3D mode */
    int wireframe_p;            /* Wireframe mode */
    int show_clock;             /* Show clock */
    int use_subproc;            /* Use subprocess */
    int no_clear;               /* No screen clear */
    float cycles_per_second;    /* Animation speed */
    fps_state *fpst;            /* FPS tracking state */
    record_anim_state *record;  /* Animation recording state */
    XWindowAttributes xgwa;     /* Window attributes */
} ModeInfo;

/* Required by xlockmore.h */
typedef int Bool;
#define True 1
#define False 0

/* Common xlockmore macros */
#define MI_DISPLAY(MI)          ((MI)->dpy)
#define MI_WINDOW(MI)           ((MI)->window_sdl)
#define MI_WIN_WIDTH(MI)        ((MI)->xgwa.width)
#define MI_WIN_HEIGHT(MI)       ((MI)->xgwa.height)
#define MI_WIDTH(MI)            (MI_WIN_WIDTH((MI)))
#define MI_HEIGHT(MI)           (MI_WIN_HEIGHT((MI)))
#define MI_SCREEN(MI)           ((MI)->screen)
#define MI_IS_FPS(MI)           ((MI)->fps_p)
#define MI_COUNT(MI)            ((MI)->count)
#define MI_CYCLES(MI)           ((MI)->cycles)
#define MI_CYCLES_PER_SECOND(MI) ((MI)->cycles_per_second)
#define MI_DEPTH(MI)            (32)  /* Just assume 32-bit color depth for now */
#define MI_RECURSION_DEPTH(MI)  ((MI)->recursion_depth)
#define MI_MOUSE_P(MI)          ((MI)->mouse_p)
#define MI_MOUSE_FN(MI)         ((MI)->mousefn)
#define MI_IS_3D(MI)            ((MI)->threed)
#define MI_IS_WIREFRAME(MI)     ((MI)->wireframe_p)
#define MI_CLEAR(MI)            (!(MI)->no_clear)

/* FPS functions */
void sdlhacks_init_fps(ModeInfo *mi);
void sdlhacks_compute_fps(ModeInfo *mi);
void sdlhacks_draw_fps(ModeInfo *mi);
void sdlhacks_free_fps(ModeInfo *mi);

/* Event handling */
Bool sdlhacks_handle_event(ModeInfo *mi, SDL_Event *event);

/* Screen clearing and buffer swapping */
void sdlhacks_clear_screen(ModeInfo *mi);
void sdlhacks_swap_buffers(ModeInfo *mi);

/* ModeInfo initialization */
ModeInfo *sdlhacks_init_modeinfo(SDL_Window *window, SDL_GLContext context);
void sdlhacks_free_modeinfo(ModeInfo *mi);

/* Compatibility functions */
extern Bool glXMakeCurrent(void **dpy, unsigned long drawable, void *ctx);
extern void glXSwapBuffers(void **dpy, unsigned long drawable);

#ifdef SDLHACKS_IMPLEMENTATION

/* FPS implementation */
void sdlhacks_init_fps(ModeInfo *mi)
{
    if (!mi || !MI_IS_FPS(mi)) return;

    fps_state *fps = (fps_state *)calloc(1, sizeof(fps_state));
    if (!fps) return;

    fps->start_time = SDL_GetTicks() / 1000.0;
    fps->last_time = fps->start_time;
    fps->frames = 0;
    fps->fps = 0;
    fps->frame_count = 0;
    fps->polygon_count = 0;
    fps->string[0] = '\0';

    mi->fpst = fps;
}

void sdlhacks_compute_fps(ModeInfo *mi)
{
    if (!mi || !mi->fpst) return;

    fps_state *fps = (fps_state *)mi->fpst;
    fps->current_time = SDL_GetTicks() / 1000.0;

    /* Update FPS once per second */
    if (fps->current_time - fps->last_time >= 1.0) {
        fps->fps = fps->frames / (fps->current_time - fps->last_time);
        fps->last_time = fps->current_time;
        fps->frames = 0;

        /* Format FPS string */
        snprintf(fps->string, sizeof(fps->string), "%.1f FPS", fps->fps);
        fps->frame_count = 0;
    }

    fps->frames++;
    fps->frame_count++;
}

void sdlhacks_draw_fps(ModeInfo *mi)
{
    if (!mi || !mi->fpst) return;

    fps_state *fps = (fps_state *)mi->fpst;
    if (!fps->string[0]) return;

    /* For now, simple FPS display in console */
    if (fps->frame_count == 0) {
        fprintf(stderr, "%s\n", fps->string);
    }

    /* TODO: Implement proper FPS display using OpenGL text rendering */
}

void sdlhacks_free_fps(ModeInfo *mi)
{
    if (!mi || !mi->fpst) return;

    free(mi->fpst);
    mi->fpst = NULL;
}

/* Event handling */
Bool sdlhacks_handle_event(ModeInfo *mi, SDL_Event *event)
{
    if (!mi || !event) return False;

    switch (event->type) {
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
            MI_WIN_WIDTH(mi) = event->window.data1;
            MI_WIN_HEIGHT(mi) = event->window.data2;
            glViewport(0, 0, MI_WIDTH(mi), MI_HEIGHT(mi));
            return True;

        case SDL_EVENT_KEY_DOWN:
            /* Handle screenhack-specific keys */
            switch (event->key.keysym.sym) {
                case SDLK_ESCAPE:
                    return True;

                default:
                    break;
            }
            break;

        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if (MI_MOUSE_FN(mi)) {
                /* Handle mouse interaction based on MI_MOUSE_FN */
                return True;
            }
            break;

        default:
            break;
    }

    return False;
}

/* Screen clearing and buffer swapping */
void sdlhacks_clear_screen(ModeInfo *mi)
{
    if (!mi) return;

    if (MI_CLEAR(mi)) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }
}

void sdlhacks_swap_buffers(ModeInfo *mi)
{
    if (!mi || !mi->window_sdl) return;

    SDL_GL_SwapWindow(mi->window_sdl);

    /* Compute FPS after swapping buffers */
    if (MI_IS_FPS(mi)) {
        sdlhacks_compute_fps(mi);
        sdlhacks_draw_fps(mi);
    }
}

/* ModeInfo initialization */
ModeInfo *sdlhacks_init_modeinfo(SDL_Window *window, SDL_GLContext context)
{
    if (!window) return NULL;

    ModeInfo *mi = (ModeInfo *)calloc(1, sizeof(ModeInfo));
    if (!mi) return NULL;

    mi->window_sdl = window;
    mi->gl_context = context;
    mi->dpy = NULL;  /* We don't use this with SDL */
    mi->screen = 0;
    mi->fps_p = True;  /* Enable FPS display by default */
    mi->count = 1;
    mi->cycles = 1;
    mi->cycles_per_second = 60.0;  /* Default to 60 fps */
    mi->recursion_depth = 1;
    mi->mouse_p = False;
    mi->mousefn = 0;
    mi->threed = True;  /* Most screensavers are 3D */
    mi->wireframe_p = False;
    mi->show_clock = False;
    mi->use_subproc = False;
    mi->no_clear = False;

    /* Initialize window attributes */
    int width, height;
    SDL_GetWindowSizeInPixels(window, &width, &height);
    mi->xgwa.width = width;
    mi->xgwa.height = height;
    mi->xgwa.screen = 0;
    mi->xgwa.background_pixel = 0;
    mi->xgwa.white_pixel = 0xFFFFFF;
    mi->xgwa.black_pixel = 0;

    /* Initialize FPS tracking */
    sdlhacks_init_fps(mi);

    return mi;
}

void sdlhacks_free_modeinfo(ModeInfo *mi)
{
    if (!mi) return;

    /* Free FPS state */
    if (mi->fpst) {
        sdlhacks_free_fps(mi);
    }

    /* Free the ModeInfo structure */
    free(mi);
}

/* Compatibility functions for xlockmore */
Bool glXMakeCurrent(void **dpy, unsigned long drawable, void *ctx)
{
    /* This is a stub for SDL compatibility */
    return SDL_GL_MakeCurrent((SDL_Window *)drawable, (SDL_GLContext)ctx) == 0;
}

void glXSwapBuffers(void **dpy, unsigned long drawable)
{
    /* This is a stub for SDL compatibility */
    SDL_GL_SwapWindow((SDL_Window *)drawable);
}

extern Bool screenhack_event_helper(void **dpy, unsigned long w, void *event)
{
    /* Stub function for compatibility */
    return False;
}

#endif /* SDLHACKS_IMPLEMENTATION */

#endif /* USE_SDL */
#endif /* __SDLHACKS_H__ */

