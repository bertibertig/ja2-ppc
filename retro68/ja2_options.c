#include "ja2_options.h"

#include "ja2_mac_internal.h"

#include <stdio.h>
#include <string.h>

static Boolean gOptionsLoaded;
static Boolean gSkipIntros;
static Boolean gShowFPS = 1;

static char *Trim(char *text)
{
    char *end;

    while (*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n') {
        ++text;
    }
    end = text + strlen(text);
    while (end > text &&
           (end[-1] == ' ' || end[-1] == '\t' ||
            end[-1] == '\r' || end[-1] == '\n')) {
        --end;
    }
    *end = 0;
    return text;
}

static Boolean EqualIgnoreCase(const char *left, const char *right)
{
    while (*left != 0 && *right != 0) {
        char leftChar = *left++;
        char rightChar = *right++;
        if (leftChar >= 'A' && leftChar <= 'Z') {
            leftChar = (char)(leftChar - 'A' + 'a');
        }
        if (rightChar >= 'A' && rightChar <= 'Z') {
            rightChar = (char)(rightChar - 'A' + 'a');
        }
        if (leftChar != rightChar) {
            return 0;
        }
    }
    return *left == 0 && *right == 0;
}

static Boolean ParseBoolean(const char *value)
{
    return EqualIgnoreCase(value, "1") ||
           EqualIgnoreCase(value, "true") ||
           EqualIgnoreCase(value, "yes") ||
           EqualIgnoreCase(value, "on");
}

static FILE *OpenOptionsFile(const char **path)
{
    FILE *file;

    *path = "options.cfg";
    file = fopen(*path, "r");
    if (file == NULL) {
        *path = ":options.cfg";
        file = fopen(*path, "r");
    }
    return file;
}

void JA2OptionsLoad(void)
{
    FILE *file;
    const char *path;
    char line[160];

    if (gOptionsLoaded) {
        return;
    }
    gOptionsLoaded = 1;
    gSkipIntros = 0;
    gShowFPS = 1;

    file = OpenOptionsFile(&path);
    if (file == NULL) {
        DebugLog("Options: options.cfg not found; skip_intros=0 show_fps=1");
        return;
    }
    DebugLog("Options: reading %s", path);
    while (fgets(line, sizeof(line), file) != NULL) {
        char *key;
        char *value;
        char *separator;

        key = Trim(line);
        if (*key == 0 || *key == '#' || *key == ';') {
            continue;
        }
        separator = strchr(key, '=');
        if (separator == NULL) {
            DebugLog("Options: ignored line without '='");
            continue;
        }
        *separator = 0;
        value = Trim(separator + 1);
        key = Trim(key);
        if (EqualIgnoreCase(key, "skip_intros")) {
            gSkipIntros = ParseBoolean(value);
            DebugLog("Options: skip_intros=%d", gSkipIntros);
        } else if (EqualIgnoreCase(key, "show_fps")) {
            gShowFPS = ParseBoolean(value);
            DebugLog("Options: show_fps=%d", gShowFPS);
        }
    }
    fclose(file);
    DebugLog("Options: loaded skip_intros=%d show_fps=%d", gSkipIntros, gShowFPS);
}

Boolean JA2OptionsSkipIntros(void)
{
    JA2OptionsLoad();
    return gSkipIntros;
}

Boolean JA2OptionsShowFPS(void)
{
    JA2OptionsLoad();
    return gShowFPS;
}
