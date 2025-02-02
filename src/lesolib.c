#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#define lesolib_c
#define LUA_LIB

#include "lua.h"

#include "lapi.h"
#include "lauxlib.h"
#include "lualib.h"

static long eso_getgametimemilliseconds(bool init) {
  static struct timeval startTime;
  if (init) {
    gettimeofday(&startTime, NULL);
    return 0;
  } else {
    struct timeval now;
    gettimeofday(&now, NULL);
    long deltaS = (now.tv_sec - startTime.tv_sec) * 1000;
    long deltaMS = (now.tv_usec - startTime.tv_usec) / 1000;
    return deltaS + deltaMS;
  }
}

int show_debug_output = 0;
LUA_API void eso_set_debug_enabled(int enable) { show_debug_output = enable; }

static void eso_log(const char *format, ...) {
  if (show_debug_output) {
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    printf("\n");
    va_end(args);
  }
}

static bool eso_isrelativepath(const char *filePath) {
  if (filePath[0] == '\\' || filePath[0] == '/') {
    return false;
  } else if (filePath[1] == ':' &&
             (filePath[2] == '\\' || filePath[2] == '/')) {
    return false;
  }
  return true;
}

static char *eso_resolvefilepath(const char *filePath) {
  char *result;

  if (eso_isrelativepath(filePath)) {
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
      eso_log("failed to get current working directory");
      return NULL;
    }

    char path[strlen(cwd) + strlen(filePath) + 1];
    sprintf(path, "%s/%s", cwd, filePath);

    result = malloc(strlen(path) + 1);
    strcpy(result, path);
  } else {
    result = malloc(strlen(filePath) + 1);
    strcpy(result, filePath);
  }

  for (int i = 0; i < strlen(result); ++i) {
    if (result[i] == '\\') {
      result[i] = '/';
    }
  }

  return result;
}

static char *eso_getbasepath(const char *filePath) {
  char *basepath = strdup(filePath);
  char *lastslash = strrchr(basepath, '/');
  if (lastslash != NULL) {
    lastslash[1] = '\0';
  } else {
    basepath[0] = '\0';
  }
  return basepath;
}

static bool eso_isluafile(const char *fileName) {
  return strlen(fileName) > 4 &&
         strcmp(fileName + strlen(fileName) - 4, ".lua") == 0;
}

static bool eso_tryloadluafile(lua_State *L, const char *fileName) {
  eso_log("try load Lua file '%s'", fileName);

  if (luaL_loadfile(L, fileName) != 0) {
    eso_log(lua_tostring(L, -1));
    return false;
  }

  if (lua_pcall(L, 0, 0, 0) != 0) {
    eso_log(lua_tostring(L, -1));
    return false;
  }

  return true;
}

// actual lib functions

static int esoL_loadaddon(lua_State *L) {
  const char *filePath = luaL_checkstring(L, 1);

  char *path = eso_resolvefilepath(filePath);
  if (path == NULL) {
    lua_pushboolean(L, 0);
    return 1;
  }

  FILE *fp = fopen(path, "r");
  eso_log("try open manifest file '%s'", path);

  if (fp == NULL) {
    lua_pushboolean(L, 0);
    eso_log("failed to open manifest file '%s'", path);
    return 1;
  }

  char *basepath = eso_getbasepath(path);

  char line[1024];
  char fileToLoad[1024 + PATH_MAX];
  while (fgets(line, sizeof(line), fp)) {
    if (line[0] == '#' || line[0] == ';' || line[0] == '\r' ||
        line[0] == '\n') {
      continue;
    }

    if (line[strlen(line) - 1] == '\n') {
      line[strlen(line) - 1] = '\0';
    }

    if (eso_isluafile(line)) {
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

static int esoL_loadluafile(lua_State *L) {
  const char *filePath = luaL_checkstring(L, 1);

  char *path = eso_resolvefilepath(filePath);
  if (path == NULL) {
    lua_pushboolean(L, 0);
    return 1;
  }

  bool success = false;
  if (eso_isluafile(path)) {
    success = eso_tryloadluafile(L, path);
  }

  free(path);

  lua_pushboolean(L, success ? 1 : 0);
  return 1;
}

static int esoL_getgametimemilliseconds(lua_State *L) {
  lua_pushnumber(L, eso_getgametimemilliseconds(false));
  return 1;
}

static int esoL_sleep(lua_State *L) {
  lua_Number ms = luaL_checknumber(L, 1);
  usleep(ms * 1000);
  return 0;
}

#include "eso/id64.c"

static const luaL_Reg eso_funcs[] = {
    {"StringToId64", esoL_stringtoid64},
    {"Id64ToString", esoL_id64tostring},
    {"CompareId64s", esoL_compareid64s},
    {"CompareId64ToNumber", esoL_compareid64tonumber},
    {"BitAnd", esoL_bitAnd},
    {"BitOr", esoL_bitOr},
    {"BitXor", esoL_bitXor},
    {"BitNot", esoL_bitNot},
    {"BitLShift", esoL_bitLShift},
    {"BitRShift", esoL_bitRShift},
    {"GetGameTimeMilliseconds", esoL_getgametimemilliseconds},
    {NULL, NULL}};

static const luaL_Reg esolib[] = {{"LoadAddon", esoL_loadaddon},
                                  {"LoadLuaFile", esoL_loadluafile},
                                  {"Sleep", esoL_sleep},
                                  {NULL, NULL}};

LUALIB_API int luaopen_eso(lua_State *L) {
  eso_getgametimemilliseconds(true);

  lua_pushvalue(L, LUA_GLOBALSINDEX);
  luaL_register(L, NULL, eso_funcs);
  lua_pop(L, 1);

  luaL_register(L, LUA_ESOLIBNAME, esolib);
  return 2;
}
