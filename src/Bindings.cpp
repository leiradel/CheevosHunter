#include "Bindings.h"
#include "Memory.h"
#include "Snapshot.h"

#include <lualib.h>
#include <lauxlib.h>

#define METATABLE_SNAPSHOT "snapshot"

struct LuaSnapshot {
    Snapshot* snapshot;
    Snapshot::Size size;
    Snapshot::Format format;
};

static int snapshotIndex(lua_State* const L) {
  size_t length;
  char const* key;
  
  key = luaL_checklstring(L, 2, &length);

  if (length == 4 && !strcmp(key, "size")) {
    lua_pushcfunction(L, image_size);
    return 1;
  }
  else if (length == 9 && !strcmp(key, "blit_nobg")) {
    lua_pushcfunction(L, image_blit_nobg);
    return 1;
  }

  return luaL_error(L, "unknown " METATABLE_NAME " method %s", key);
}

static int snapshotGc(lua_State* const L) {
  rl_image_t const* const self = (rl_image_t*)lua_touserdata(L, 1);
  rl_image_destroy(self);
  return 0;
}

static int click(lua_State* const L) {
  LuaSnapshot* const self = static_cast<LuaSnapshot*>lua_newuserdata(L, sizeof(LuaSnapshot));

  //self->snapshot = Memory::click();
  self->size = Snapshot::Size::_8;
  self->format = Snapshot::Format::UIntLittleEndian;
  
  if (luaL_newmetatable(L, METATABLE_SNAPSHOT) != 0) {
    lua_pushcfunction(L, snapshotIndex);
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, snapshotGc);
    lua_setfield(L, -2, "__gc");
  }

  lua_setmetatable(L, -2);
  return 1;
}

void registerBindings(lua_State* const L) {
  static luaL_Reg const functions[] = {
    {"click", click},
    {NULL,    NULL}
  };

  luaL_setfuncs(L, functions);
}
