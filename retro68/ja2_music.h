#ifndef JA2_MUSIC_H
#define JA2_MUSIC_H

/* Mirrors the original main-menu music mode using the System 9 Sound Manager. */
typedef void (*JA2MusicLogProc)(const char *format, ...);

void JA2MenuMusicSetLogger(JA2MusicLogProc logger);
void JA2MenuMusicStart(void);
void JA2MenuMusicPoll(void);
void JA2MenuMusicShutdown(void);

#endif
