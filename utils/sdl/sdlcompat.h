/* sdlcompat.h - SDL compatibility layer for xscreensaver
 *
 * Copyright (c) 2023
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 */

#ifndef __SDLCOMPAT_H__
#define __SDLCOMPAT_H__

#ifdef USE_SDL

#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Basic types and constants */
#ifndef Bool
typedef int Bool;
#endif

#ifndef True
#define True 1
#endif

#ifndef False
#define False 0
#endif

/* SDL initialization flags */
#define SDL_DEFAULT_INIT_FLAGS (SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_EVENTS)

/* Window creation flags */
#define SDL_DEFAULT_WINDOW_FLAGS (SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY)

/* Default window dimensions */
#define SDL_DEFAULT_WINDOW_WIDTH 800
#define SDL_DEFAULT_WINDOW_HEIGHT 600

/* Error handling */
#define SDL_ERROR(msg) fprintf(stderr, "SDL Error: %s: %s\n", (msg), SDL_GetError())
#define SDL_RETURN_ERROR(msg, ret) do { SDL_ERROR(msg); return (ret); } while (0)

/**
 * Initialize SDL with the specified flags
 *
 * @param flags Initialization flags (SDL_INIT_VIDEO etc.)
 * @return True on success, False on failure
 */
static inline Bool sdl_init(Uint32 flags)
{
    if (SDL_Init(flags) < 0) {
        SDL_ERROR("SDL_Init failed");
        return False;
    }

    return True;
}

/**
 * Shutdown SDL and clean up resources
 */
static inline void sdl_quit(void)
{
    SDL_Quit();
}

/**
 * Create a new SDL window
 *
 * @param title Window title
 * @param width Window width (in pixels)
 * @param height Window height (in pixels)
 * @param flags Window creation flags
 * @return SDL_Window pointer or NULL on failure
 */
static inline SDL_Window* sdl_create_window(const char *title, int width, int height, Uint32 flags)
{
    SDL_Window *window = SDL_CreateWindow(title, width, height, flags);
    if (!window) {
        SDL_ERROR("Failed to create window");
    }
    return window;
}

/**
 * Get window size in pixels
 *
 * @param window SDL_Window pointer
 * @param width Pointer to store width
 * @param height Pointer to store height
 * @return True on success, False on failure
 */
static inline Bool sdl_get_window_size(SDL_Window *window, int *width, int *height)
{
    if (SDL_GetWindowSizeInPixels(window, width, height) < 0) {
        SDL_ERROR("Failed to get window size");
        return False;
    }
    return True;
}

/**
 * Set window size in pixels
 *
 * @param window SDL_Window pointer
 * @param width New width
 * @param height New height
 */
static inline void sdl_set_window_size(SDL_Window *window, int width, int height)
{
    SDL_SetWindowSize(window, width, height);
}

/**
 * Event handling - process all pending SDL events
 *
 * @param event Pointer to SDL_Event structure to store the event
 * @return True if the event was processed, False otherwise
 */
static inline Bool sdl_process_events(SDL_Event *event)
{
    if (!event) return False;

    return SDL_PollEvent(event);
}

/**
 * Delay execution for specified milliseconds
 *
 * @param ms Milliseconds to delay
 */
static inline void sdl_delay(Uint32 ms)
{
    SDL_Delay(ms);
}

/**
 * Get ticks (milliseconds since SDL initialization)
 *
 * @return Number of milliseconds since SDL_Init was called
 */
static inline Uint64 sdl_get_ticks(void)
{
    return SDL_GetTicks();
}

/**
 * Check if a key is pressed
 *
 * @param scancode SDL_Scancode to check
 * @return True if key is pressed, False otherwise
 */
static inline Bool sdl_is_key_pressed(SDL_Scancode scancode)
{
    const Uint8 *state = SDL_GetKeyboardState(NULL);
    return state[scancode] ? True : False;
}

#endif /* USE_SDL */
#endif /* __SDLCOMPAT_H__ */

