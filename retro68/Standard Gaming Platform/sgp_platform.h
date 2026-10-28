#ifndef SGP_PLATFORM_H
#define SGP_PLATFORM_H

/* JA2 PPC: definitions every original source needs on Mac OS that the x86
 * build got from MSVC's runtime. Included from Types.h under JA2_PPC. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The original opens files with Windows paths relative to the process's
 * current directory ("..\\Ja2.ini", "Data\\..."); route them through the
 * same path mapping as CreateFile (sgp_win32.c). */
FILE *SGPFOpen(const char *windowsPath, const char *mode);
#define fopen SGPFOpen

/* MSVC's rand(): RAND_MAX is 0x7fff and the generator is the runtime's
 * LCG. Random() computes rand() * uiRange / RAND_MAX in 32 bits, which
 * overflows with newlib's RAND_MAX, and save games store pregenerated
 * values, so the port uses the same generator as the Windows game. */
int SGPRand(void);
void SGPSrand(unsigned int seed);
#undef RAND_MAX
#define RAND_MAX 0x7fff
#define rand SGPRand
#define srand SGPSrand

int _stricmp(const char *left, const char *right);
int _strnicmp(const char *left, const char *right, size_t count);
char *_strupr(char *text);
char *_strlwr(char *text);
#ifndef stricmp
#define stricmp _stricmp
#define strnicmp _strnicmp
#define strcmpi _stricmp
#define _strcmpi _stricmp
#define strupr _strupr
#define strlwr _strlwr
#endif

#endif
