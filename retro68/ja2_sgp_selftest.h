#ifndef JA2_SGP_SELFTEST_H
#define JA2_SGP_SELFTEST_H

/* PPC-only check of the ported Standard Gaming Platform core (file
 * database, file manager, random, clock, containers). There is no
 * original counterpart. It runs the same initialization sequence as the
 * original JA2 Splash.c, reports one line per check through the callback
 * and returns the number of failed checks. The autotest command "sgptest"
 * runs it on the Mac; tools/sgp_host_test.sh runs it on the build host. */

typedef void (*JA2SGPReportProc)(const char *format, ...);

int JA2SGPSelfTest(JA2SGPReportProc report);

#endif
