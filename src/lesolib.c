#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <sys/time.h>

#define lesolib_c
#define LUA_LIB

#include "lua.h"

#include "lapi.h"
#include "lauxlib.h"
#include "lualib.h"

#define ESO_ID64 unsigned long long
#define ESO_MAXNUMBER2STR 20 // highest value is 2^64-1 (=18446744073709551615) so there should never be more than 20 characters
typedef ESO_ID64 eso_id64;

union esoId64_Cast
{
    lua_Number number;
    eso_id64 id64;
};

#define eso_number2id64(i, o)          \
    {                                  \
        volatile union esoId64_Cast u; \
        u.number = i;                  \
        (o) = u.id64;                  \
    }

#define eso_id642number(i, o)          \
    {                                  \
        volatile union esoId64_Cast u; \
        u.id64 = i;                    \
        (o) = u.number;                \
    }

// helper functions

static inline eso_id64 eso_reinterpretNumberAsId64(lua_Number value)
{
    eso_id64 result;
    eso_number2id64(value, result);
    return result;
}

static inline lua_Number eso_reinterpretId64AsNumber(eso_id64 value)
{
    lua_Number result;
    eso_id642number(value, result);
    return result;
}

static eso_id64 eso_convertId64StringToId64(const char *str)
{
    char *s;
    return strtoull(str, &s, 10);
}

static eso_id64 eso_convertNumberToId64(lua_Number value)
{
    char s[LUAI_MAXNUMBER2STR];
    sprintf(s, "%.f", value); // TODO ensure that number is in range of 64bit
    return eso_convertId64StringToId64(s);
}

static int eso_compareId64s(eso_id64 a, eso_id64 b)
{
    if (a < b)
    {
        return -1;
    }
    else if (a > b)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

static long eso_getgametimemilliseconds(bool init)
{
    static struct timeval startTime;
    if (init)
    {
        gettimeofday(&startTime, NULL);
        return 0;
    }
    else
    {
        struct timeval now;
        gettimeofday(&now, NULL);
        long deltaS = (now.tv_sec - startTime.tv_sec) * 1000;
        long deltaMS = (now.tv_usec - startTime.tv_usec) / 1000;
        return deltaS + deltaMS;
    }
}

int show_debug_output = 0;
LUA_API void eso_set_debug_enabled(int enable)
{
    show_debug_output = enable;
}

static void eso_log(const char *format, ...)
{
    if (show_debug_output)
    {
        va_list args;
        va_start(args, format);
        vprintf(format, args);
        printf("\n");
        va_end(args);
    }
}

static bool eso_isrelativepath(const char *filePath)
{
    if (filePath[0] == '\\' || filePath[0] == '/')
    {
        return false;
    }
    else if (filePath[1] == ':' && (filePath[2] == '\\' || filePath[2] == '/'))
    {
        return false;
    }
    return true;
}

static char *eso_resolvefilepath(const char *filePath)
{
    char *result;

    if (eso_isrelativepath(filePath))
    {
        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd)) == NULL)
        {
            eso_log("failed to get current working directory");
            return NULL;
        }

        char path[strlen(cwd) + strlen(filePath) + 1];
        sprintf(path, "%s/%s", cwd, filePath);

        result = malloc(strlen(path) + 1);
        strcpy(result, path);
    }
    else
    {
        result = malloc(strlen(filePath) + 1);
        strcpy(result, filePath);
    }

    for (int i = 0; i < strlen(result); ++i)
    {
        if (result[i] == '\\')
        {
            result[i] = '/';
        }
    }

    return result;
}

static char *eso_getbasepath(const char *filePath)
{
    char *basepath = strdup(filePath);
    char *lastslash = strrchr(basepath, '/');
    if (lastslash != NULL)
    {
        lastslash[1] = '\0';
    }
    else
    {
        basepath[0] = '\0';
    }
    return basepath;
}

static bool eso_isluafile(const char *fileName)
{
    return strlen(fileName) > 4 && strcmp(fileName + strlen(fileName) - 4, ".lua") == 0;
}

static bool eso_tryloadluafile(lua_State *L, const char *fileName)
{
    eso_log("try load Lua file '%s'", fileName);

    if (luaL_loadfile(L, fileName) != 0)
    {
        eso_log(lua_tostring(L, -1));
        return false;
    }

    if (lua_pcall(L, 0, 0, 0) != 0)
    {
        eso_log(lua_tostring(L, -1));
        return false;
    }

    return true;
}

// actual lib functions

static int esoL_loadaddon(lua_State *L)
{
    const char *filePath = luaL_checkstring(L, 1);

    char *path = eso_resolvefilepath(filePath);
    if (path == NULL)
    {
        lua_pushboolean(L, 0);
        return 1;
    }

    FILE *fp = fopen(path, "r");
    eso_log("try open manifest file '%s'", path);

    if (fp == NULL)
    {
        lua_pushboolean(L, 0);
        eso_log("failed to open manifest file '%s'", path);
        return 1;
    }

    char *basepath = eso_getbasepath(path);

    char line[1024];
    char fileToLoad[1024 + PATH_MAX];
    while (fgets(line, sizeof(line), fp))
    {
        if (line[0] == '#' || line[0] == ';' || line[0] == '\r' || line[0] == '\n')
        {
            continue;
        }

        if (line[strlen(line) - 1] == '\n')
        {
            line[strlen(line) - 1] = '\0';
        }

        if (eso_isluafile(line))
        {
            sprintf(fileToLoad, "%s%s", basepath, line);
            eso_tryloadluafile(L, fileToLoad);
        }
    }

    fclose(fp);
    free(basepath);
    free(path);

    lua_pushboolean(L, 1);
    return 1;
}

static int esoL_loadluafile(lua_State *L)
{
    const char *filePath = luaL_checkstring(L, 1);

    char *path = eso_resolvefilepath(filePath);
    if (path == NULL)
    {
        lua_pushboolean(L, 0);
        return 1;
    }

    bool success = false;
    if (eso_isluafile(path))
    {
        success = eso_tryloadluafile(L, path);
    }

    free(path);

    lua_pushboolean(L, success ? 1 : 0);
    return 1;
}

static int esoL_stringtoid64(lua_State *L)
{
    const char *s = luaL_checkstring(L, 1);
    eso_id64 n = eso_convertId64StringToId64(s);
    lua_Number d = eso_reinterpretId64AsNumber(n);
    lua_pushnumber(L, d);
    return 1;
}

static int esoL_id64tostring(lua_State *L)
{
    lua_Number d = luaL_checknumber(L, 1);
    eso_id64 n = eso_reinterpretNumberAsId64(d);
    char s[ESO_MAXNUMBER2STR];
    sprintf(s, "%llu", n);
    lua_pushstring(L, s);
    return 1;
}

static int esoL_compareid64s(lua_State *L)
{
    lua_Number a = luaL_checknumber(L, 1);
    lua_Number b = luaL_checknumber(L, 2);
    eso_id64 a_ = eso_reinterpretNumberAsId64(a);
    eso_id64 b_ = eso_reinterpretNumberAsId64(b);
    int result = eso_compareId64s(a_, b_);
    lua_pushnumber(L, result);
    return 1;
}

static int esoL_compareid64tonumber(lua_State *L)
{
    lua_Number a = luaL_checknumber(L, 1);
    lua_Number b = luaL_checknumber(L, 2);
    eso_id64 a_ = eso_reinterpretNumberAsId64(a);
    eso_id64 b_ = eso_convertNumberToId64(b);
    int result = eso_compareId64s(a_, b_);
    lua_pushnumber(L, result);
    return 1;
}

static int esoL_getgametimemilliseconds(lua_State *L)
{
    lua_pushnumber(L, eso_getgametimemilliseconds(false));
    return 1;
}

static int esoL_sleep(lua_State *L)
{
    lua_Number ms = luaL_checknumber(L, 1);
    usleep(ms * 1000);
    return 0;
}

static const luaL_Reg eso_funcs[] = {
    {"StringToId64", esoL_stringtoid64},
    {"Id64ToString", esoL_id64tostring},
    {"CompareId64s", esoL_compareid64s},
    {"CompareId64ToNumber", esoL_compareid64tonumber},
    {"GetGameTimeMilliseconds", esoL_getgametimemilliseconds},
    {NULL, NULL}};

static const luaL_Reg esolib[] = {
    {"LoadAddon", esoL_loadaddon},
    {"LoadLuaFile", esoL_loadluafile},
    {"Sleep", esoL_sleep},
    {NULL, NULL}};

LUALIB_API int luaopen_eso(lua_State *L)
{
    eso_getgametimemilliseconds(true);

    lua_pushvalue(L, LUA_GLOBALSINDEX);
    luaL_register(L, NULL, eso_funcs);
    lua_pop(L, 1);

    luaL_register(L, LUA_ESOLIBNAME, esolib);
    return 2;
}
