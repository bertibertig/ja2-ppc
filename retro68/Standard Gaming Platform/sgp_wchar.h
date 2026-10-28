#ifndef SGP_WCHAR_H
#define SGP_WCHAR_H

/* JA2 PPC: the wide-string runtime of MSVC 6, which the original game code
 * calls more than 1,500 times. MSVC differs from C99 in ways the code
 * depends on:
 *   - swprintf(buffer, format, ...) has no size argument;
 *   - in wide formats %s and %c take wide arguments, %S and %C narrow ones;
 *   - wcstok(string, delimiters) keeps its position in a static.
 * wchar_t is 16 bits (Retro68's default, -fshort-wchar on hosts), and these
 * functions never call the C library's wide functions, whose idea of
 * wchar_t can differ (glibc uses 32 bits). Included from sgp_platform.h;
 * the macros below come after <wchar.h> so its prototypes stay intact. */

#include <stdarg.h>
#include <stddef.h>
#include <wchar.h>
#include <wctype.h>

#ifdef __cplusplus
extern "C" {
#endif

int SGPswprintf(wchar_t *buffer, const wchar_t *format, ...);
int SGPvswprintf(wchar_t *buffer, const wchar_t *format, va_list arguments);
int SGPswscanf(const wchar_t *text, const wchar_t *format, ...);
size_t SGPwcslen(const wchar_t *text);
wchar_t *SGPwcscpy(wchar_t *destination, const wchar_t *source);
wchar_t *SGPwcsncpy(wchar_t *destination, const wchar_t *source, size_t count);
wchar_t *SGPwcscat(wchar_t *destination, const wchar_t *source);
wchar_t *SGPwcsncat(wchar_t *destination, const wchar_t *source, size_t count);
int SGPwcscmp(const wchar_t *left, const wchar_t *right);
int SGPwcsncmp(const wchar_t *left, const wchar_t *right, size_t count);
int SGPwcsicmp(const wchar_t *left, const wchar_t *right);
wchar_t *SGPwcschr(const wchar_t *text, wchar_t character);
wchar_t *SGPwcsrchr(const wchar_t *text, wchar_t character);
wchar_t *SGPwcsstr(const wchar_t *text, const wchar_t *search);
wchar_t *SGPwcstok(wchar_t *text, const wchar_t *delimiters);
long SGPwcstol(const wchar_t *text, wchar_t **end, int base);
wchar_t *SGPwcsupr(wchar_t *text);
wchar_t SGPtowupper(wchar_t character);
wchar_t SGPtowlower(wchar_t character);

#ifdef __cplusplus
}
#endif

#undef swprintf
#undef vswprintf
#undef swscanf
#undef wcslen
#undef wcscpy
#undef wcsncpy
#undef wcscat
#undef wcsncat
#undef wcscmp
#undef wcsncmp
#undef wcschr
#undef wcsrchr
#undef wcsstr
#undef wcstok
#undef wcstol
#undef towupper
#undef towlower
#define swprintf SGPswprintf
#define vswprintf SGPvswprintf
#define swscanf SGPswscanf
#define wcslen SGPwcslen
#define wcscpy SGPwcscpy
#define wcsncpy SGPwcsncpy
#define wcscat SGPwcscat
#define wcsncat SGPwcsncat
#define wcscmp SGPwcscmp
#define wcsncmp SGPwcsncmp
#define _wcsicmp SGPwcsicmp
#define wcschr SGPwcschr
#define wcsrchr SGPwcsrchr
#define wcsstr SGPwcsstr
#define wcstok SGPwcstok
#define wcstol SGPwcstol
#define _wcsupr SGPwcsupr
#define towupper SGPtowupper
#define towlower SGPtowlower

/* Win32's wsprintf/wvsprintf in a non-UNICODE build are narrow. */
#define wsprintf sprintf
#define wvsprintf vsprintf

#endif
