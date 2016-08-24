#include "lflatbuffers.hpp"

bool lflatbuffers::load_bfbs_path( const char *path )
{
    return true;
}

bool lflatbuffers::load_bfbs_file( const char *file )
{
    return true;
}

/* ========================== lua 5.1 ======================================= */
#ifndef LUA_MAXINTEGER
    #define LUA_MAXINTEGER (2147483647)
#endif

#ifndef LUA_MININTEGER
    #define LUA_MININTEGER (-2147483647-1)
#endif

/* this function was copy from src of lua5.3.1 */
LUALIB_API void luaL_setfuncs_ex (lua_State *L, const luaL_Reg *l, int nup) {
  luaL_checkstack(L, nup, "too many upvalues");
  for (; l->name != NULL; l++) {  /* fill the table with given functions */
    int i;
    for (i = 0; i < nup; i++)  /* copy upvalues to the top */
      lua_pushvalue(L, -nup);
    lua_pushcclosure(L, l->func, nup);  /* closure with those upvalues */
    lua_setfield(L, -(nup + 2), l->name);
  }
  lua_pop(L, nup);  /* remove upvalues */
}

#ifndef luaL_newlibtable
    #define luaL_newlibtable(L,l)	\
        lua_createtable(L, 0, sizeof(l)/sizeof((l)[0]) - 1)
#endif

#ifndef luaL_newlib
    #define luaL_newlib(L,l)  \
        (luaL_newlibtable(L,l), luaL_setfuncs_ex(L,l,0))
#endif

/* ========================== lua 5.1 ======================================= */

/* ====================LIBRARY INITIALISATION FUNCTION======================= */

static const luaL_Reg lua_parson_lib[] =
{
    // {"encode", encode},
    // {"decode", decode},
    // {"encode_to_file", encode_to_file},
    // {"decode_from_file", decode_from_file},
    {NULL, NULL}
};

int luaopen_lua_flatbuffers(lua_State *L)
{
    luaL_newlib(L, lua_parson_lib);
    return 1;
}
