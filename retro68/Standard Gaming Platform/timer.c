#ifdef JA2_PRECOMPILED_HEADERS
	#include "JA2 SGP ALL.H"
#elif defined( WIZ8_PRECOMPILED_HEADERS )
	#include "WIZ8 SGP ALL.H"
#else
	#include "types.h"
#ifdef JA2_PPC
	#include "sgp_win32.h"
#else
	#include <windows.h>
#endif
	#if defined( JA2_PPC )
		// JA2 PPC: no window timer message; GetClock() samples the clock.
	#elif defined( JA2 ) || defined( UTIL )
		#include "video.h"
	#else
		#include "video2.h"
	#endif
	#include "timer.h"
#endif

#ifndef WIN32_LEAN_AND_MEAN
	#define WIN32_LEAN_AND_MEAN
#endif

UINT32 guiStartupTime;
UINT32 guiCurrentTime;

#ifdef JA2_PPC
#define CALLBACK
#endif

void CALLBACK Clock( HWND hWindow, UINT uMessage, UINT idEvent, DWORD dwTime )
{
  guiCurrentTime = GetTickCount();
  if (guiCurrentTime < guiStartupTime)
  { // Adjust guiCurrentTime because of loopback on the timer value
    guiCurrentTime = guiCurrentTime + (0xffffffff - guiStartupTime);
  }
  else
  { // Adjust guiCurrentTime because of loopback on the timer value
    guiCurrentTime = guiCurrentTime - guiStartupTime;
  }
}

BOOLEAN InitializeClockManager(void)
{

  // Register the start time (use WIN95 API call)
  guiCurrentTime = guiStartupTime = GetTickCount();
#ifndef JA2_PPC
  SetTimer(ghWindow, MAIN_TIMER_ID, 10, (TIMERPROC)Clock);
#endif


  return TRUE;
}

void    ShutdownClockManager(void)
{

  // Make sure we kill the timer
#ifndef JA2_PPC
  KillTimer(ghWindow, MAIN_TIMER_ID);
#endif

}

TIMER   GetClock(void)
{
#ifdef JA2_PPC
  // The x86 build refreshed guiCurrentTime from a 10 ms WM_TIMER.
  Clock( NULL, 0, 0, 0 );
#endif
  return guiCurrentTime;
}

TIMER   SetCountdownClock(UINT32 uiTimeToElapse)
{
  return (guiCurrentTime + uiTimeToElapse);
}

UINT32 ClockIsTicking(TIMER uiTimer)
{
#ifdef JA2_PPC
  Clock( NULL, 0, 0, 0 );
#endif
  if (uiTimer > guiCurrentTime)
  { // Well timer still hasn't elapsed
    return (uiTimer - guiCurrentTime);
  }
  // Time's up
  return 0;
}
