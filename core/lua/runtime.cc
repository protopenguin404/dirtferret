#include "lua/runtime.h"

#include <iostream>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

LuaRuntime::LuaRuntime() = default;

LuaRuntime::~LuaRuntime() {
  if (L_) {
    lua_close(L_);
    L_ = nullptr;
  }
}

bool LuaRuntime::initialize() {
  L_ = luaL_newstate();
  if (!L_) {
    std::cerr << "[lua] Failed to create Lua state." << std::endl;
    return false;
  }

  luaL_openlibs(L_);
  setup_dirtferret_table();

  std::cerr << "[lua] Lua runtime initialized." << std::endl;
  return true;
}

void LuaRuntime::setup_dirtferret_table() {
  lua_newtable(L_);

  const char *subtables[] = {"opt", "buffer", "keymap", "mode",
                             "scroll", "find", "clipboard", "command"};
  for (auto name : subtables) {
    lua_newtable(L_);
    lua_setfield(L_, -2, name);
  }

  // dirtferret.on = function() end (placeholder)
  lua_pushcfunction(L_, [](lua_State *) -> int { return 0; });
  lua_setfield(L_, -2, "on");

  lua_setglobal(L_, "dirtferret");
}

bool LuaRuntime::exec_string(const std::string &code) {
  if (!L_) return false;
  if (luaL_dostring(L_, code.c_str()) != 0) {
    const char *err = lua_tostring(L_, -1);
    std::cerr << "[lua] Error: " << (err ? err : "unknown") << std::endl;
    lua_pop(L_, 1);
    return false;
  }
  return true;
}

bool LuaRuntime::exec_file(const std::string &path) {
  if (!L_) return false;
  if (luaL_dofile(L_, path.c_str()) != 0) {
    const char *err = lua_tostring(L_, -1);
    std::cerr << "[lua] Error loading " << path << ": "
              << (err ? err : "unknown") << std::endl;
    lua_pop(L_, 1);
    return false;
  }
  std::cerr << "[lua] Loaded: " << path << std::endl;
  return true;
}

std::string LuaRuntime::get_opt_string(const std::string &key) {
  if (!L_) return "";
  lua_getglobal(L_, "dirtferret");
  lua_getfield(L_, -1, "opt");
  lua_getfield(L_, -1, key.c_str());
  const char *val = lua_tostring(L_, -1);
  std::string result = val ? val : "";
  lua_pop(L_, 3);
  return result;
}

double LuaRuntime::get_opt_number(const std::string &key) {
  if (!L_) return 0.0;
  lua_getglobal(L_, "dirtferret");
  lua_getfield(L_, -1, "opt");
  lua_getfield(L_, -1, key.c_str());
  double val = lua_tonumber(L_, -1);
  lua_pop(L_, 3);
  return val;
}

bool LuaRuntime::get_opt_bool(const std::string &key) {
  if (!L_) return false;
  lua_getglobal(L_, "dirtferret");
  lua_getfield(L_, -1, "opt");
  lua_getfield(L_, -1, key.c_str());
  bool val = lua_toboolean(L_, -1);
  lua_pop(L_, 3);
  return val;
}

void LuaRuntime::register_function(const std::string &path, CFunction fn) {
  if (!L_) return;
  size_t start = 0;
  size_t dot = path.find('.');
  std::string first = path.substr(0, dot);
  lua_getglobal(L_, first.c_str());

  while (dot != std::string::npos) {
    start = dot + 1;
    dot = path.find('.', start);
    if (dot != std::string::npos) {
      std::string key = path.substr(start, dot - start);
      lua_getfield(L_, -1, key.c_str());
    } else {
      std::string key = path.substr(start);
      lua_pushcfunction(L_, fn);
      lua_setfield(L_, -2, key.c_str());
    }
  }
  lua_settop(L_, 0);
}
