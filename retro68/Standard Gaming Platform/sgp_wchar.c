/* JA2 PPC: MSVC 6 wide-string runtime (see sgp_wchar.h). */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "sgp_wchar.h"

size_t SGPwcslen(const wchar_t *text)
{
    const wchar_t *end = text;
    while (*end)
        ++end;
    return (size_t)(end - text);
}

wchar_t *SGPwcscpy(wchar_t *destination, const wchar_t *source)
{
    wchar_t *out = destination;
    while ((*out++ = *source++) != 0)
        ;
    return destination;
}

/* As in the C library: pads with zeros and does not terminate when the
   source is at least count characters long. */
wchar_t *SGPwcsncpy(wchar_t *destination, const wchar_t *source, size_t count)
{
    size_t index = 0;
    for (; index < count && source[index]; ++index)
        destination[index] = source[index];
    for (; index < count; ++index)
        destination[index] = 0;
    return destination;
}

wchar_t *SGPwcscat(wchar_t *destination, const wchar_t *source)
{
    SGPwcscpy(destination + SGPwcslen(destination), source);
    return destination;
}

wchar_t *SGPwcsncat(wchar_t *destination, const wchar_t *source, size_t count)
{
    wchar_t *out = destination + SGPwcslen(destination);
    while (count-- && *source)
        *out++ = *source++;
    *out = 0;
    return destination;
}

int SGPwcscmp(const wchar_t *left, const wchar_t *right)
{
    while (*left && *left == *right) {
        ++left;
        ++right;
    }
    return (int)*left - (int)*right;
}

int SGPwcsncmp(const wchar_t *left, const wchar_t *right, size_t count)
{
    for (; count; --count, ++left, ++right) {
        if (*left != *right || !*left)
            return (int)*left - (int)*right;
    }
    return 0;
}

/* MSVC's towupper/towlower in the "C" locale change only ASCII letters. */
wchar_t SGPtowupper(wchar_t character)
{
    return (character >= 'a' && character <= 'z') ? (wchar_t)(character - 32) : character;
}

wchar_t SGPtowlower(wchar_t character)
{
    return (character >= 'A' && character <= 'Z') ? (wchar_t)(character + 32) : character;
}

int SGPwcsicmp(const wchar_t *left, const wchar_t *right)
{
    wchar_t a, b;
    do {
        a = SGPtowlower(*left++);
        b = SGPtowlower(*right++);
    } while (a && a == b);
    return (int)a - (int)b;
}

wchar_t *SGPwcsupr(wchar_t *text)
{
    wchar_t *cursor;
    for (cursor = text; *cursor; ++cursor)
        *cursor = SGPtowupper(*cursor);
    return text;
}

wchar_t *SGPwcschr(const wchar_t *text, wchar_t character)
{
    for (;; ++text) {
        if (*text == character)
            return (wchar_t *)text;
        if (!*text)
            return NULL;
    }
}

wchar_t *SGPwcsrchr(const wchar_t *text, wchar_t character)
{
    const wchar_t *found = NULL;
    for (;; ++text) {
        if (*text == character)
            found = text;
        if (!*text)
            return (wchar_t *)found;
    }
}

wchar_t *SGPwcsstr(const wchar_t *text, const wchar_t *search)
{
    size_t length = SGPwcslen(search);
    if (!length)
        return (wchar_t *)text;
    for (; *text; ++text) {
        if (*text == *search && SGPwcsncmp(text, search, length) == 0)
            return (wchar_t *)text;
    }
    return NULL;
}

wchar_t *SGPwcstok(wchar_t *text, const wchar_t *delimiters)
{
    static wchar_t *next;
    wchar_t *start;

    if (text)
        next = text;
    if (!next)
        return NULL;
    while (*next && SGPwcschr(delimiters, *next))
        ++next;
    if (!*next) {
        next = NULL;
        return NULL;
    }
    start = next;
    while (*next && !SGPwcschr(delimiters, *next))
        ++next;
    if (*next)
        *next++ = 0;
    else
        next = NULL;
    return start;
}

long SGPwcstol(const wchar_t *text, wchar_t **end, int base)
{
    char narrow[64];
    char *narrowEnd;
    size_t length = 0;
    long value;
    const wchar_t *start = text;

    while (*start == ' ' || *start == '\t' || *start == '\n')
        ++start;
    while (length + 1 < sizeof(narrow) && start[length] && start[length] < 128) {
        narrow[length] = (char)start[length];
        ++length;
    }
    narrow[length] = 0;
    value = strtol(narrow, &narrowEnd, base);
    if (end)
        *end = (wchar_t *)(narrowEnd == narrow ? text : start + (narrowEnd - narrow));
    return value;
}

/* Formatting. Numbers go through the narrow vsnprintf (their output is
   ASCII); strings and characters are handled here because their argument
   width depends on MSVC's %s/%S rules. */

static void PutPadded(wchar_t **out, const wchar_t *text, size_t length, int width, int left)
{
    int pad = width > (int)length ? width - (int)length : 0;
    if (!left)
        while (pad-- > 0)
            *(*out)++ = ' ';
    while (length--)
        *(*out)++ = *text++;
    if (left)
        while (pad-- > 0)
            *(*out)++ = ' ';
}

int SGPvswprintf(wchar_t *buffer, const wchar_t *format, va_list arguments)
{
    wchar_t *out = buffer;

    while (*format) {
        char spec[32];
        size_t specLength = 0;
        int width = 0, precision = -1, left = 0;
        int lengthShort = 0, lengthLong = 0;
        wchar_t conversion;

        if (*format != '%') {
            *out++ = *format++;
            continue;
        }
        spec[specLength++] = (char)*format++;
        if (*format == '%') {
            *out++ = '%';
            ++format;
            continue;
        }
        while (*format == '-' || *format == '+' || *format == ' ' || *format == '#' || *format == '0') {
            if (*format == '-')
                left = 1;
            spec[specLength++] = (char)*format++;
        }
        if (*format == '*') {
            width = va_arg(arguments, int);
            if (width < 0) {
                left = 1;
                width = -width;
            }
            ++format;
            specLength += (size_t)snprintf(spec + specLength, sizeof(spec) - specLength, "%d", width);
        } else {
            while (*format >= '0' && *format <= '9') {
                width = width * 10 + (*format - '0');
                spec[specLength++] = (char)*format++;
            }
        }
        if (*format == '.') {
            spec[specLength++] = (char)*format++;
            precision = 0;
            if (*format == '*') {
                precision = va_arg(arguments, int);
                ++format;
                specLength += (size_t)snprintf(spec + specLength, sizeof(spec) - specLength, "%d", precision);
            } else {
                while (*format >= '0' && *format <= '9') {
                    precision = precision * 10 + (*format - '0');
                    spec[specLength++] = (char)*format++;
                }
            }
        }
        if (*format == 'h') {
            lengthShort = 1;
            ++format;
        } else if (*format == 'l') {
            lengthLong = 1;
            ++format;
        }
        conversion = *format;
        if (!conversion)
            break;
        ++format;

        if (conversion == 's' || conversion == 'S') {
            int wide = lengthLong || (conversion == 's' && !lengthShort);
            if (wide) {
                const wchar_t *text = va_arg(arguments, const wchar_t *);
                size_t length;
                if (!text)
                    text = L"(null)";
                length = SGPwcslen(text);
                if (precision >= 0 && (size_t)precision < length)
                    length = (size_t)precision;
                PutPadded(&out, text, length, width, left);
            } else {
                const unsigned char *text = va_arg(arguments, const unsigned char *);
                size_t length, index;
                int pad;
                if (!text)
                    text = (const unsigned char *)"(null)";
                length = strlen((const char *)text);
                if (precision >= 0 && (size_t)precision < length)
                    length = (size_t)precision;
                pad = width > (int)length ? width - (int)length : 0;
                if (!left)
                    while (pad-- > 0)
                        *out++ = ' ';
                for (index = 0; index < length; ++index)
                    *out++ = text[index];
                if (left)
                    while (pad-- > 0)
                        *out++ = ' ';
            }
        } else if (conversion == 'c' || conversion == 'C') {
            wchar_t character = (wchar_t)va_arg(arguments, int);
            if (conversion == 'C' || lengthShort)
                character = (wchar_t)(unsigned char)character;
            PutPadded(&out, &character, 1, width, left);
        } else {
            char narrow[128];
            int written = 0, index;
            if (conversion == 'n') {
                *va_arg(arguments, int *) = (int)(out - buffer);
                continue;
            }
            if (lengthLong)
                spec[specLength++] = 'l';
            else if (lengthShort)
                spec[specLength++] = 'h';
            spec[specLength++] = (char)conversion;
            spec[specLength] = 0;
            switch (conversion) {
            case 'd': case 'i': case 'u': case 'x': case 'X': case 'o':
                if (lengthLong)
                    written = snprintf(narrow, sizeof(narrow), spec, va_arg(arguments, long));
                else
                    written = snprintf(narrow, sizeof(narrow), spec, va_arg(arguments, int));
                break;
            case 'f': case 'e': case 'E': case 'g': case 'G':
                written = snprintf(narrow, sizeof(narrow), spec, va_arg(arguments, double));
                break;
            case 'p':
                written = snprintf(narrow, sizeof(narrow), "%p", va_arg(arguments, void *));
                break;
            default:
                narrow[0] = 0;
                break;
            }
            if (written > (int)sizeof(narrow) - 1)
                written = (int)sizeof(narrow) - 1;
            for (index = 0; index < written; ++index)
                *out++ = (unsigned char)narrow[index];
        }
    }
    *out = 0;
    return (int)(out - buffer);
}

int SGPswprintf(wchar_t *buffer, const wchar_t *format, ...)
{
    va_list arguments;
    int result;
    va_start(arguments, format);
    result = SGPvswprintf(buffer, format, arguments);
    va_end(arguments);
    return result;
}

/* The game scans only single integers ("%d", "%ld", "%hu"). */
int SGPswscanf(const wchar_t *text, const wchar_t *format, ...)
{
    va_list arguments;
    int assigned = 0;

    va_start(arguments, format);
    while (*format) {
        int lengthShort = 0, lengthLong = 0;
        wchar_t *end;
        long value;

        if (*format != '%') {
            if (*format == ' ') {
                while (*text == ' ' || *text == '\t')
                    ++text;
            } else if (*text++ != *format) {
                break;
            }
            ++format;
            continue;
        }
        ++format;
        if (*format == 'h') {
            lengthShort = 1;
            ++format;
        } else if (*format == 'l') {
            lengthLong = 1;
            ++format;
        }
        if (*format != 'd' && *format != 'u' && *format != 'i')
            break;
        ++format;
        value = SGPwcstol(text, &end, 10);
        if (end == text)
            break;
        text = end;
        if (lengthShort)
            *va_arg(arguments, unsigned short *) = (unsigned short)value;
        else if (lengthLong)
            *va_arg(arguments, long *) = value;
        else
            *va_arg(arguments, int *) = (int)value;
        ++assigned;
    }
    va_end(arguments);
    return assigned;
}
