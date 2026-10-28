#ifndef _GAME_VERSION_H_
#define _GAME_VERSION_H_


#ifdef __cplusplus
extern "C" {
#endif


//
//	Keeps track of the game version
//

#ifdef JA2_PPC
// wchar_t is unsigned on the Mac; MSVC accepted L"" for INT16 arrays.
extern	CHAR16		zVersionLabel[16];
extern	INT8		czVersionNumber[16];
extern	CHAR16		zTrackingNumber[16];
#else
extern	INT16		zVersionLabel[16];
extern	INT8		czVersionNumber[16];
extern	INT16		zTrackingNumber[16];
#endif



//#define RUSSIANGOLD

//
//		Keeps track of the saved game version.  Increment the saved game version whenever 
//	you will invalidate the saved game file
//

const extern	UINT32	guiSavedGameVersion;

#ifdef __cplusplus
}
#endif


#endif
