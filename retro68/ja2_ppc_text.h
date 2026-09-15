#ifndef JA2_PPC_TEXT_H
#define JA2_PPC_TEXT_H

#include <Quickdraw.h>
#include <QDOffscreen.h>
#include <Fonts.h>
#include <SDL.h>

/*
 * SDL2 on this classic Mac backend has no text rendering API (no SDL_ttf in
 * the vendored fork). QuickDraw's Font Manager is used only as an off-screen
 * glyph rasterizer here -- text is rendered into a private GWorld, never
 * into any SDL-owned window/surface directly, and the resulting glyph
 * pixels are copied into the caller's SDL_Surface. This keeps QuickDraw
 * confined to a small, self-contained platform interface instead of a
 * second window/rendering path (AGENTS.md "Rendering Backend (SDL2)").
 */

void JA2TextInit(void);
void JA2TextShutdown(void);

/* color is a packed SDL_PIXELFORMAT_RGB555 (xRGB1555) value. size is the
   QuickDraw font size in points (systemFont). Returns the pixel width the
   string would occupy, for callers doing their own layout. */
short JA2TextWidth(const char *text, short size);
short JA2TextHeight(short size);
void JA2DrawText(SDL_Surface *destination, short x, short y, const char *text,
                 Uint16 color, short size);

#endif
