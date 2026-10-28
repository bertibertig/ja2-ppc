#ifndef JA2_OPTIONS_H
#define JA2_OPTIONS_H

#include <Quickdraw.h>

void JA2OptionsLoad(void);
Boolean JA2OptionsSkipIntros(void);
Boolean JA2OptionsShowFPS(void);
unsigned long JA2OptionsProfileFrames(void);
Boolean JA2OptionsProfileUncapped(void);
const char *JA2OptionsProfileLabel(void);

#endif
