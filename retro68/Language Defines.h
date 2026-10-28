#ifndef __LANGUAGE_DEFINES_H
#define __LANGUAGE_DEFINES_H

//ONLY ONE OF THESE CAN BE DEFINED AT A TIME!

#ifdef JA2_PPC
	// JA2 PPC: the language comes from CMake (JA2_LANGUAGE=german|english).
	#if defined(JA2_LANGUAGE_GERMAN)
		#define GERMAN
	#else
		#define ENGLISH
	#endif
#else
#define ENGLISH
#endif
//#define GERMAN
//#define RUSSIAN
//#define DUTCH
//#define POLISH

#define BLOOD_N_GORE_ENABLED



//if the language represents words as single chars
#ifdef TAIWAN
	#define	SINGLE_CHAR_WORDS
#endif

#endif