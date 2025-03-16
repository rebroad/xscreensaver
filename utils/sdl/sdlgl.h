/* sdlgl.h - OpenGL support for SDL integration in xscreensaver
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

#ifndef __SDLGL_H__
#define __SDLGL_H__

#include "sdlcompat.h"

#ifdef USE_SDL

#include <SDL3/SDL.h>
#include <GL/gl.h>

/* ========================================================================= */
/* OpenGL Context Management                                                 */
/* ========================================================================= */

/* GL context configuration */
typedef struct {
    int major_version;
    int minor_version;
    Bool core_profile;
    Bool debug_context;
    int depth_bits;
    int stencil_bits;
    Bool double_buffer;
    Bool vsync;
} SDL_GL_Config;

/* Default OpenGL configuration */
#define SDL_GL_DEFAULT_CONFIG { \
    .major_version = 2, \
    .minor_version = 0, \
    .core_profile = False, \
    .debug_context = False, \
    .depth_bits = 16, \
    .stencil_bits = 0, \
    .double_buffer = True, \
    .vsync = True \
}

/* Initialize OpenGL context with the given configuration */
Bool sdlgl_init_context(SDL_Window *window, const SDL_GL_Config *config);

/* Destroy OpenGL context */
void sdlgl_destroy_context(SDL_GLContext context);

/* Make OpenGL context current */
Bool sdlgl_make_current(SDL_Window *window, SDL_GLContext context);

/* Swap buffers (for double-buffered contexts) */
void sdlgl_swap_buffers(SDL_Window *window);

/* Check if GL extensions are supported */
Bool sdlgl_have_extension(const char *extension);

/* Check OpenGL version */
Bool sdlgl_check_version(int major, int minor);

/* ========================================================================= */
/* Shader Utilities                                                          */
/* ========================================================================= */

/* Compile shader from source */
GLuint sdlgl_compile_shader(GLenum shader_type, const char *source);

/* Link program from vertex and fragment shaders */
GLuint sdlgl_link_program(GLuint vertex_shader, GLuint fragment_shader);

/* Load shader from file */
GLuint sdlgl_load_shader(GLenum shader_type, const char *filename);

/* Create program from vertex and fragment shader files */
GLuint sdlgl_create_program(const char *vertex_file, const char *fragment_file);

/* Delete shader */
void sdlgl_delete_shader(GLuint shader);

/* Delete program */
void sdlgl_delete_program(GLuint program);

/* Get shader info log */
char* sdlgl_get_shader_info_log(GLuint shader);

/* Get program info log */
char* sdlgl_get_program_info_log(GLuint program);

/* ========================================================================= */
/* GL Error Handling                                                         */
/* ========================================================================= */

/* Check for GL errors and print to stderr */
void sdlgl_check_error(const char *op);

/* Clear all pending GL errors */
void sdlgl_clear_error(void);

/* Get GL error string */
const char* sdlgl_error_string(GLenum error);

#ifdef SDLGL_IMPLEMENTATION

/* ========================================================================= */
/* OpenGL Context Management Implementation                                  */
/* ========================================================================= */

Bool sdlgl_init_context(SDL_Window *window, const SDL_GL_Config *config)
{
    if (!window) return False;
    if (!config) {
        SDL_GL_Config default_config = SDL_GL_DEFAULT_CONFIG;
        return sdlgl_init_context(window, &default_config);
    }

    /* Set GL attributes */
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, config->major_version);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, config->minor_version);

    if (config->core_profile) {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                           SDL_GL_CONTEXT_PROFILE_CORE);
    }

    if (config->debug_context) {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
    }

    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, config->depth_bits);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, config->stencil_bits);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, config->double_buffer ? 1 : 0);

    /* Create GL context */
    SDL_GLContext context = SDL_GL_CreateContext(window);
    if (!context) {
        fprintf(stderr, "sdlgl: failed to create GL context: %s\n",
                SDL_GetError());
        return False;
    }

    /* Set VSync */
    if (SDL_GL_SetSwapInterval(config->vsync ? 1 : 0) < 0) {
        fprintf(stderr, "sdlgl: failed to set swap interval: %s\n",
                SDL_GetError());
        /* Not a fatal error, continue */
    }

    /* Make context current */
    if (!sdlgl_make_current(window, context)) {
        SDL_GL_DeleteContext(context);
        return False;
    }

    return True;
}

void sdlgl_destroy_context(SDL_GLContext context)
{
    if (context) {
        SDL_GL_DeleteContext(context);
    }
}

Bool sdlgl_make_current(SDL_Window *window, SDL_GLContext context)
{
    if (!window || !context) return False;

    if (SDL_GL_MakeCurrent(window, context) < 0) {
        fprintf(stderr, "sdlgl: failed to make context current: %s\n",
                SDL_GetError());
        return False;
    }

    return True;
}

void sdlgl_swap_buffers(SDL_Window *window)
{
    if (window) {
        SDL_GL_SwapWindow(window);
    }
}

Bool sdlgl_have_extension(const char *extension)
{
    if (!extension) return False;

    /* Get extensions string */
    const char *extensions = (const char *)glGetString(GL_EXTENSIONS);
    if (!extensions) return False;

    /* Check if extension is in the string */
    const char *start = extensions;
    const char *where, *terminator;

    /* Extension names should not have spaces */
    where = strstr(start, extension);
    if (!where) return False;

    terminator = where + strlen(extension);
    return *terminator == ' ' || *terminator == '\0';
}

Bool sdlgl_check_version(int major, int minor)
{
    GLint major_version, minor_version;

    glGetIntegerv(GL_MAJOR_VERSION, &major_version);
    glGetIntegerv(GL_MINOR_VERSION, &minor_version);

    return (major_version > major ||
           (major_version == major && minor_version >= minor));
}

/* ========================================================================= */
/* Shader Utilities Implementation                                           */
/* ========================================================================= */

GLuint sdlgl_compile_shader(GLenum shader_type, const char *source)
{
    if (!source) return 0;

    /* Create shader object */
    GLuint shader = glCreateShader(shader_type);
    if (!shader) {
        fprintf(stderr, "sdlgl: failed to create shader\n");
        return 0;
    }

    /* Set shader source */
    glShaderSource(shader, 1, &source, NULL);

    /* Compile shader */
    glCompileShader(shader);

    /* Check compile status */
    GLint status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status == GL_FALSE) {
        char *log = sdlgl_get_shader_info_log(shader);
        fprintf(stderr, "sdlgl: shader compilation failed: %s\n", log);
        free(log);

        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

GLuint sdlgl_link_program(GLuint vertex_shader, GLuint fragment_shader)
{
    /* Create program object */
    GLuint program = glCreateProgram();
    if (!program) {
        fprintf(stderr, "sdlgl: failed to create program\n");
        return 0;
    }

    /* Attach shaders */
    if (vertex_shader)
        glAttachShader(program, vertex_shader);
    if (fragment_shader)
        glAttachShader(program, fragment_shader);

    /* Link program */
    glLinkProgram(program);

    /* Check link status */
    GLint status;
    glGetProgramiv(program, GL_LINK_STATUS, &status);
    if (status == GL_FALSE) {
        char *log = sdlgl_get_program_info_log(program);
        fprintf(stderr, "sdlgl: program linking failed: %s\n", log);
        free(log);

        glDeleteProgram(program);
        return 0;
    }

    return program;
}

GLuint sdlgl_load_shader(GLenum shader_type, const char *filename)
{
    if (!filename) return 0;

    /* Open file */
    FILE *file = fopen(filename, "rb");
    if (!file) {
        fprintf(stderr, "sdlgl: failed to open shader file '%s'\n", filename);
        return 0;
    }

    /* Get file size */
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    /* Read file content */
    char *source = (char *)malloc(size + 1);
    if (!source) {
        fprintf(stderr, "sdlgl: failed to allocate memory for shader source\n");
        fclose(file);
        return 0;
    }

    size_t read_size = fread(source, 1, size, file);
    fclose(file);

    if (read_size != size) {
        fprintf(stderr, "sdlgl: failed to read shader file '%s'\n", filename);
        free(source);
        return 0;
    }

    source[size] = '\0';

    /* Compile shader */
    GLuint shader = sdlgl_compile_shader(shader_type, source);
    free(source);

    return shader;
}

GLuint sdlgl_create_program(const char *vertex_file, const char *fragment_file)
{
    GLuint vertex_shader = 0, fragment_shader = 0, program = 0;

    /* Load vertex shader */
    if (vertex_file) {
        vertex_shader = sdlgl_load_shader(GL_VERTEX_SHADER, vertex_file);
        if (!vertex_shader) goto cleanup;
    }

    /* Load fragment shader */
    if (fragment_file) {
        fragment_shader = sdlgl_load_shader(GL_FRAGMENT_SHADER, fragment_file);
        if (!fragment_shader) goto cleanup;
    }

    /* Link program */
    program = sdlgl_link_program(vertex_shader, fragment_shader);

cleanup:
    /* Clean up shaders */
    if (vertex_shader)
        glDeleteShader(vertex_shader);
    if (fragment_shader)
        glDeleteShader(fragment_shader);

    return program;
}

void sdlgl_delete_shader(GLuint shader)
{
    if (shader) {
        glDeleteShader(shader);
    }
}

void sdlgl_delete_program(GLuint program)
{
    if (program) {
        glDeleteProgram(program);
    }
}

char* sdlgl_get_shader_info_log(GLuint shader)
{
    GLint length;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);

    if (length <= 0) return strdup("");

    char *log = (char *)malloc(length);
    if (!log) return strdup("(memory allocation failed)");

    glGetShaderInfoLog(shader, length, NULL, log);
    return log;
}

char* sdlgl_get_program_info_log(GLuint program)
{
    GLint length;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);

    if (length <= 0) return strdup("");

    char *log = (char *)malloc(length);
    if (!log) return strdup("(memory allocation failed)");

    glGetProgramInfoLog(program, length, NULL, log);
    return log;
}

/* ========================================================================= */
/* GL Error Handling Implementation                                          */
/* ========================================================================= */

void sdlgl_check_error(const char *op)
{
    GLenum error;

    while ((error = glGetError()) != GL_NO_ERROR) {
        fprintf(stderr, "sdlgl: GL error after %s: %s (0x%x)\n",
                op ? op : "operation",
                sdlgl_error_string(error),
                error);
    }
}

void sdlgl_clear_error(void)
{
    while (glGetError() != GL_NO_ERROR);
}

const char* sdlgl_error_string(GLenum error)
{
    switch (error) {
        case GL_NO_ERROR:
            return "No error";
        case GL_INVALID_ENUM:
            return "Invalid enum";
        case GL_INVALID_VALUE:
            return "Invalid value";
        case GL_INVALID_OPERATION:
            return "Invalid operation";
        case GL_STACK_OVERFLOW:
            return "Stack overflow";
        case GL_STACK_UNDERFLOW:
            return "Stack underflow";
        case GL_OUT_OF_MEMORY:
            return "Out of memory";
        case GL_INVALID_FRAMEBUFFER_OPERATION:
            return "Invalid framebuffer operation";
        default:
            return "Unknown error";
    }
}

#endif /* SDLGL_IMPLEMENTATION */

#endif /* USE_SDL */
#endif /* __SDLGL_H__ */

