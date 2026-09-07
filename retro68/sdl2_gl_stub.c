/*
 * Stub implementation for the OpenGL hookup functions that
 * sdl2macos9's macosclassic video driver always references.
 *
 * SDL-main/include/SDL_config_macosclassic.h unconditionally defines
 * SDL_VIDEO_OPENGL, regardless of any Makefile.r68ppc define. That makes
 * src/video/macosclassic/macvideo.c and macevents.c always reference a
 * fixed set of GL_* hookup functions (see macvideo.c's SDL_VideoDevice
 * assignment and macevents.c's activate/drag handling), even when the
 * game never calls the SDL_GL_* API.
 *
 * The fork ships two real implementations, both intentionally NOT built
 * here: macgl.c needs a real AGL import library, which the Retro68
 * toolchain does not provide (verified: no libAGL/libagl archive exists,
 * and no symbol named aglCreateContext appears in any toolchain library).
 * mactinygl.c is a bundled software OpenGL rasterizer ("TinyGL") that
 * pulls in a much larger dependency than a 2D sprite game needs.
 *
 * JA2 never calls SDL_GL_*; per AGENTS.md's "Rendering Backend (SDL2)"
 * policy it renders through SDL_Surface/SDL_BlitSurface only. This stub
 * exists solely to satisfy the linker for the always-compiled hookup
 * path, and every function fails safe (returns an error / does nothing)
 * so an accidental SDL_GL_* call is a diagnosable no-op, not a crash.
 *
 * Build this alongside any target linking SDL2::Classic
 * (retro68/JA2_PPC_SDL2_MIGRATION_PLAN.md, "Build Integration").
 */

int glLoadLibrary(void *thisDevice, const char *path)
{
    (void)thisDevice;
    (void)path;
    return -1;
}

void *glGetProcAddress(void *thisDevice, const char *proc)
{
    (void)thisDevice;
    (void)proc;
    return (void *)0;
}

void glUnloadLibrary(void *thisDevice)
{
    (void)thisDevice;
}

void *glCreateContext(void *thisDevice, void *window)
{
    (void)thisDevice;
    (void)window;
    return (void *)0;
}

int glMakeCurrent(void *thisDevice, void *window, void *context)
{
    (void)thisDevice;
    (void)window;
    (void)context;
    return -1;
}

int glSetSwapInterval(void *thisDevice, int interval)
{
    (void)thisDevice;
    (void)interval;
    return -1;
}

int glSwapWindow(void *thisDevice, void *window)
{
    (void)thisDevice;
    (void)window;
    return -1;
}

void glDeleteContext(void *thisDevice, void *context)
{
    (void)thisDevice;
    (void)context;
}

void glUpdateWindow(void *thisDevice, void *window)
{
    (void)thisDevice;
    (void)window;
}

/* Not part of the SDL_VideoDevice GL_* table: macevents.c calls these two
   directly on window activate/drag, unconditionally once compiled with
   SDL_VIDEO_OPENGL defined. There is no drawable to (de)activate or
   update without a real GL context, so both are quiet successful no-ops. */
int Mac_GL_SetDrawableActive(int active)
{
    (void)active;
    return 0;
}

void Mac_GL_Update(void)
{
}
