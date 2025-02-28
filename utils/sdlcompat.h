#ifndef __SDLCOMPAT_H__
#define __SDLCOMPAT_H__

#ifdef USE_SDL
#include <SDL3/SDL.h>
#define Bool int
#define False 0
#define True 1
#endif

#endif // __SDLCOMPAT_H___
