# SDL Support in XScreenSaver

This document tracks the SDL support implementation for XScreenSaver.

## File Structure

The SDL support is organized into three main files under `utils/sdl/`:

1. `sdlcompat.h` - Basic SDL compatibility layer
   - Basic SDL initialization and cleanup
   - Window management
   - Event handling basics
   - Common utilities and types
   - Error handling

2. `sdlgl.h` - OpenGL support for SDL
   - OpenGL context management
   - Context creation and configuration
   - Shader utilities (compilation, linking)
   - GL error handling
   - Extension support checking

3. `sdlhacks.h` - Screensaver-specific functionality
   - XLockmore compatibility layer
   - ModeInfo structure and macros
   - FPS tracking and display
   - Event handling for screensavers
   - Screen clearing and buffer swapping

## Implementation Details

### SDL Compatibility Layer
- Provides basic types (Bool, True, False)
- Window management functions
- Error handling macros
- Event processing utilities
- Timing functions

### OpenGL Integration
- GL context configuration structure
- Default OpenGL settings
- Shader management utilities
- Error checking and reporting
- Extension checking

### Screenhack Support
- Complete ModeInfo structure matching xlockmore
- FPS tracking and display system
- Event handling specific to screensavers
- Buffer management and swapping
- Backward compatibility functions

## Usage Notes

1. All SDL support requires USE_SDL to be defined
2. Implementation is included using *_IMPLEMENTATION defines
3. Files are organized in dependency order:
   - sdlcompat.h (base layer)
   - sdlgl.h (depends on sdlcompat.h)
   - sdlhacks.h (depends on both above)

## Future Improvements

1. Proper FPS display using OpenGL text rendering
2. More robust error handling
3. Additional xlockmore compatibility functions
4. Testing with various screensavers

