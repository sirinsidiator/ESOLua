#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

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

// actual lib functions

#define debug(active, format, ...)     \
    if (active)                        \
    {                                  \
        printf(format, ##__VA_ARGS__); \
        printf("\n");                  \
    }

static int esoL_loadaddon(lua_State *L)
{
    const char *relativePath = luaL_checkstring(L, 1);
    const int showOutput = lua_toboolean(L, 2);

    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) == NULL)
    {
        lua_pushboolean(L, 0);
        debug(showOutput, "failed to get current working directory");
        return 1;
    }

    char path[strlen(cwd) + strlen(relativePath) + 1];
    sprintf(path, "%s/%s", cwd, relativePath);
    for (int i = 0; i < strlen(path); ++i)
    {
        if (path[i] == '\\')
        {
            path[i] = '/';
        }
    }

    FILE *fp = fopen(path, "r");
    debug(showOutput, "try open manifest file '%s'", path);

    if (fp == NULL)
    {
        lua_pushboolean(L, 0);
        debug(showOutput, "failed to open manifest file '%s'", path);
        return 1;
    }

    char *basepath = strdup(path);
    char *lastslash = strrchr(basepath, '/');
    if (lastslash != NULL)
    {
        lastslash[1] = '\0';
    }
    else
    {
        basepath[0] = '\0';
    }

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

        if (strlen(line) > 4 && strcmp(line + strlen(line) - 4, ".lua") == 0)
        {
            sprintf(fileToLoad, "%s%s", basepath, line);
            debug(showOutput, "try load Lua file '%s'", fileToLoad);

            if (luaL_loadfile(L, fileToLoad) != 0)
            {
                debug(showOutput, lua_tostring(L, -1));
            }

            if (lua_pcall(L, 0, 0, 0) != 0)
            {
                debug(showOutput, lua_tostring(L, -1));
            }
        }
    }

    fclose(fp);

    lua_pushboolean(L, 1);
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

static const luaL_Reg eso_funcs[] = {
    {"StringToId64", esoL_stringtoid64},
    {"Id64ToString", esoL_id64tostring},
    {"CompareId64s", esoL_compareid64s},
    {"CompareId64ToNumber", esoL_compareid64tonumber},
    {NULL, NULL}};

static const luaL_Reg esolib[] = {
    {"LoadAddon", esoL_loadaddon},
    {NULL, NULL}};

LUALIB_API int luaopen_eso(lua_State *L)
{
    lua_pushvalue(L, LUA_GLOBALSINDEX);
    luaL_register(L, NULL, eso_funcs);
    lua_pop(L, 1);

    luaL_register(L, LUA_ESOLIBNAME, esolib);
    return 2;
}
