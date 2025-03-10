# XScreenSaver Development Guide

## Build Commands
- Build all: `make`
- Build a specific hack: `make <hackname>` (e.g., `make hextrail`)
- Clean: `make clean`
- Install: `make install`
- Test a specific hack: `./<hackname>` (e.g., `./hextrail`)

## Code Style
- C99 standard with GNU extensions (`-std=gnu99`)
- Keep functions and variable names descriptive
- Use comments to explain complex logic

## Project Structure
- `/hacks/glx/` - OpenGL screensavers
- `/hacks/` - Regular X11 screensavers
- `/driver/` - XScreenSaver core program
- `/utils/` - Shared utility functions

## Debugging
- Add temporary debug prints with `printf()`
- Check the `verbose` flag in screensavers for debug output

## Common Libraries/Utils
- `rotator.h` - For rotation animations
- `gltrackball.h` - For trackball UI interaction
- `normals.h` - For calculating normals
- `colors.h` - For color management
