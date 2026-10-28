#ifndef _EXCEPTION_HANDLING__H_
#define _EXCEPTION_HANDLING__H_


//uncomment this line if you want Exceptions to be handled
#ifdef JA2_PPC
	// JA2 PPC: no structured exception handling on the Mac.
#elif defined( JA2 )

#ifndef _DEBUG
	#define ENABLE_EXCEPTION_HANDLING
#endif

#else
	//Wizardry 
//#define ENABLE_EXCEPTION_HANDLING
#endif


#ifdef __cplusplus
extern "C" {
#endif



#ifndef JA2_PPC
INT32 RecordExceptionInfo( EXCEPTION_POINTERS *pExceptInfo );
#endif


#ifdef __cplusplus
}
#endif

#endif