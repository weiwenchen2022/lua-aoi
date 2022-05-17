#include "aoi.h"

#include "lua.h"
#include "lauxlib.h"

#include <string.h>

#define AOI_META "Aoi"

typedef struct laoi_userdata {
    aoi_space *space;
    lua_State *L;
} laoi_userdata;

static int lnew(lua_State *L)
{
    aoi_space *space;
    laoi_userdata *aoiu = (laoi_userdata *)lua_newuserdatauv(L, sizeof(*aoiu), 0);
    aoiu->space = NULL;

    luaL_getmetatable(L, AOI_META);
    lua_setmetatable(L, -2);

    float radius = (float)luaL_checknumber(L, 1);
    void *ud;
    aoi_Alloc alloc = lua_getallocf(L, &ud);

    space = aoiu->space = aoi_create(radius, alloc, ud);
    if (!space)
	return luaL_error(L, "aoi_create failed");

    return 1;
}

static int lupdate(lua_State *L)
{
    laoi_userdata *aoiu = (laoi_userdata *)luaL_checkudata(L, 1, AOI_META);
    luaL_argcheck(L, aoiu->space != NULL, 1, "space not created");

    uint32_t id = (uint32_t)luaL_checkinteger(L, 2);
    const char *mode = luaL_checkstring(L, 3);
    int i;
    for (i = 0; mode[i]; i++)
	luaL_argcheck(L, strchr("wmd", mode[i]) != NULL, 2, "modestring error");

    float pos[3];
    pos[0] = (float)luaL_optnumber(L, 4, 0.0f);
    pos[1] = (float)luaL_optnumber(L, 5, 0.0f);
    pos[2] = (float)luaL_optnumber(L, 6, 0.0f);

    aoi_update(aoiu->space, id, mode, pos);
    return 0;
}

static void message(void *ud, uint32_t watcher, uint32_t marker)
{
    laoi_userdata *aoiu = (laoi_userdata *)ud;
    lua_State *L = aoiu->L;

    lua_pushvalue(L, 2);
    lua_pushvalue(L, 3);
    lua_pushinteger(L, watcher);
    lua_pushinteger(L, marker);

    lua_callk(L, 3, 0, 0, NULL);
}

static int lmessage(lua_State *L)
{
    laoi_userdata *aoiu = (laoi_userdata *)luaL_checkudata(L, 1, AOI_META);
    luaL_argcheck(L, aoiu->space != NULL, 1, "space not created");

    luaL_checktype(L, 2, LUA_TFUNCTION);
    lua_settop(L, 3);
    aoiu->L = L;

    aoi_message(aoiu->space, message, aoiu);
    return 0;
}

static int lclose(lua_State *L)
{
    laoi_userdata *aoiu = (laoi_userdata *)luaL_checkudata(L, 1, AOI_META);
    if (aoiu->space)
	aoi_release(aoiu->space);

    aoiu->space = NULL;
    return 0;
}

int luaopen_aoi(lua_State *L)
{
    luaL_newmetatable(L, AOI_META);
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");

    const luaL_Reg meths[] = {
	{"__gc", lclose,},

	{"close", lclose,},
	{"update", lupdate,},
	{"message", lmessage,},

	{NULL, NULL,},
    };

    luaL_setfuncs(L, meths, 0);

    const luaL_Reg funcs[] = {
	{"new", lnew,},

	{NULL, NULL,},
    };
    luaL_newlib(L, funcs);

    return 1;
}
