
#define ESO_ID64 unsigned long long
#define ESO_MAXNUMBER2STR                                                      \
  20 // highest value is 2^64-1 (=18446744073709551615) so there should never be
     // more than 20 characters
typedef ESO_ID64 eso_id64;

union esoId64_Cast {
  lua_Number number;
  eso_id64 id64;
};

#define eso_number2id64(i, o)                                                  \
  {                                                                            \
    volatile union esoId64_Cast u;                                             \
    u.number = i;                                                              \
    (o) = u.id64;                                                              \
  }

#define eso_id642number(i, o)                                                  \
  {                                                                            \
    volatile union esoId64_Cast u;                                             \
    u.id64 = i;                                                                \
    (o) = u.number;                                                            \
  }

// helper functions

static inline eso_id64 eso_reinterpretNumberAsId64(lua_Number value) {
  eso_id64 result;
  eso_number2id64(value, result);
  return result;
}

static inline lua_Number eso_reinterpretId64AsNumber(eso_id64 value) {
  lua_Number result;
  eso_id642number(value, result);
  return result;
}

static eso_id64 eso_convertId64StringToId64(const char *str) {
  char *s;
  return strtoull(str, &s, 10);
}

static int eso_compareId64s(eso_id64 a, eso_id64 b) {
  if (a < b) {
    return -1;
  } else if (a > b) {
    return 1;
  } else {
    return 0;
  }
}

static int esoL_stringtoid64(lua_State *L) {
  const char *s = luaL_checkstring(L, 1);
  eso_id64 n = eso_convertId64StringToId64(s);
  lua_Number d = eso_reinterpretId64AsNumber(n);
  lua_pushnumber(L, d);
  return 1;
}

static int esoL_id64tostring(lua_State *L) {
  lua_Number d = luaL_checknumber(L, 1);
  eso_id64 n = eso_reinterpretNumberAsId64(d);
  char s[ESO_MAXNUMBER2STR];
  sprintf(s, "%llu", n);
  lua_pushstring(L, s);
  return 1;
}

static int esoL_compareid64s(lua_State *L) {
  lua_Number a = luaL_checknumber(L, 1);
  lua_Number b = luaL_checknumber(L, 2);
  eso_id64 a_ = eso_reinterpretNumberAsId64(a);
  eso_id64 b_ = eso_reinterpretNumberAsId64(b);
  int result = eso_compareId64s(a_, b_);
  lua_pushnumber(L, result);
  return 1;
}

static int esoL_compareid64tonumber(lua_State *L) {
  lua_Number a = luaL_checknumber(L, 1);
  lua_Number b = luaL_checknumber(L, 2);
  eso_id64 a_ = eso_reinterpretNumberAsId64(a);
  eso_id64 b_ = (eso_id64)b;
  int result = eso_compareId64s(a_, b_);
  lua_pushnumber(L, result);
  return 1;
}
