#ifndef JA2_PPC_LAPTOP_H
#define JA2_PPC_LAPTOP_H

#include <Quickdraw.h>
#include <SDL.h>

Boolean JA2LaptopRender(void);
void JA2LaptopHandleClick(short x, short y);
void JA2LaptopHandleKey(SDL_Keycode key);
void JA2LaptopShutdown(void);

#endif
