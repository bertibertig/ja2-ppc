#ifdef PRECOMPILEDHEADERS
	#include "JA2 All.h"
#else
	#include "Types.h"
	#include "GameVersion.h"
#endif
 
   
//
//	Keeps track of the game version
//
 
#ifdef JA2_PPC

// wchar_t is unsigned on the Mac, so the wide labels are CHAR16 arrays
// (MSVC accepted L"" for INT16). Same text as the release build.
CHAR16	zVersionLabel[16]		= { L"Release v1.12" };
INT8		czVersionNumber[16]	= { "Build 04.12.02" };
CHAR16	zTrackingNumber[16]	= { L"Z" };

#else
#ifdef JA2BETAVERSION

//BETA/TEST BUILD VERSION
INT16		zVersionLabel[16]		= { L"Beta v. 0.98" }; 

#elif defined CRIPPLED_VERSION

//RELEASE BUILD VERSION 
INT16		zVersionLabel[16]		= { L"Beta v. 0.98" }; 

#else

	//RELEASE BUILD VERSION 
	INT16		zVersionLabel[16]		= { L"Release v1.12" }; 


#endif

INT8		czVersionNumber[16]	= { "Build 04.12.02" };
INT16		zTrackingNumber[16]	= { L"Z" };
#endif
	

//
//		Keeps track of the saved game version.  Increment the saved game version whenever 
//	you will invalidate the saved game file

#define			SAVE_GAME_VERSION					99

const	UINT32	guiSavedGameVersion = SAVE_GAME_VERSION;
