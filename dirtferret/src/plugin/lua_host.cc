#include "plugin/lua_host.h"
#include "api/dispatcher.h"

#include <iostream>

extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

namespace cef_terminal {

// Key used to stash the LuaHost pointer in the Lua registry,
// so C callbacks can get back to the host.
static const char* REGISTRY_HOST_KEY = "cef_terminal.host";

// Helper: retrieve the LuaHost* from a Lua state.
static LuaHost* get_host(lua_State* L) {
    lua_getfield(L, LUA_REGISTRYINDEX, REGISTRY_HOST_KEY);
    auto* host = static_cast<LuaHost*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    return host;
}

// --- Lua-side bridge functions ---
// These are exposed as cef.on_command, cef.on_query, etc.

// cef.on_command(name, handler)
// Registers a Lua function as a command handler.
static int l_on_command(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);

    // Store the handler function in the registry keyed by command name.
    lua_setfield(L, LUA_REGISTRYINDEX, (std::string("cmd:") + name).c_str());

    std::cerr << "[plugin] Registered Lua command handler: " << name << std::endl;
    return 0;
}

// cef.on_query(name, handler)
// Registers a Lua function as a query handler.
static int l_on_query(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);

    lua_setfield(L, LUA_REGISTRYINDEX, (std::string("qry:") + name).c_str());

    std::cerr << "[plugin] Registered Lua query handler: " << name << std::endl;
    return 0;
}

// cef.command(name, args)
// Emits a command from Lua through the dispatcher.
static int l_command(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);

    // TODO: convert Lua table at index 2 to Args map.
    // For now, fire with empty args.
    auto* host = get_host(L);
    (void)host;  // Will use host->dispatcher_ once wired.

    std::cerr << "[plugin] Lua emitted command: " << name << std::endl;
    return 0;
}

// cef.query(name, args) -> table
// Emits a query from Lua through the dispatcher and returns the result.
static int l_query(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);

    auto* host = get_host(L);
    (void)host;

    std::cerr << "[plugin] Lua emitted query: " << name << std::endl;

    // TODO: dispatch query, convert QueryResult.data to Lua table.
    // For now, return empty table.
    lua_newtable(L);
    return 1;
}

// --- LuaHost implementation ---

LuaHost::LuaHost() = default;

LuaHost::~LuaHost() {
    shutdown();
}

bool LuaHost::initialize() {
    L_ = luaL_newstate();
    if (!L_) {
        std::cerr << "[plugin] Failed to create Lua state." << std::endl;
        return false;
    }

    luaL_openlibs(L_);
    setup_bridge();

    std::cerr << "[plugin] Lua " << LUA_VERSION_MAJOR << "."
              << LUA_VERSION_MINOR << " initialized." << std::endl;
    return true;
}

void LuaHost::setup_bridge() {
    // Stash host pointer in registry so C callbacks can find us.
    lua_pushlightuserdata(L_, this);
    lua_setfield(L_, LUA_REGISTRYINDEX, REGISTRY_HOST_KEY);

    // Create the "cef" table.
    lua_newtable(L_);

    // Register bridge functions.
    lua_pushcfunction(L_, l_on_command);
    lua_setfield(L_, -2, "on_command");

    lua_pushcfunction(L_, l_on_query);
    lua_setfield(L_, -2, "on_query");

    lua_pushcfunction(L_, l_command);
    lua_setfield(L_, -2, "command");

    lua_pushcfunction(L_, l_query);
    lua_setfield(L_, -2, "query");

    // Set as global "cef".
    lua_setglobal(L_, "cef");
}

bool LuaHost::load_plugin(const std::string& path) {
    if (luaL_dofile(L_, path.c_str()) != LUA_OK) {
        const char* err = lua_tostring(L_, -1);
        std::cerr << "[plugin] Error loading " << path << ": "
                  << (err ? err : "unknown error") << std::endl;
        lua_pop(L_, 1);
        return false;
    }

    loaded_plugins_.push_back(path);
    std::cerr << "[plugin] Loaded: " << path << std::endl;
    return true;
}

void LuaHost::register_handlers(Dispatcher& dispatcher) {
    dispatcher_ = &dispatcher;

    // TODO: Walk the registry for any "cmd:" and "qry:" entries that
    // plugins registered during load_plugin(), and register them as
    // real handlers with the dispatcher. This bridges Lua handlers
    // into the C++ dispatch system.
    //
    // For each "cmd:<name>" entry:
    //   dispatcher.register_command(name, [this](const Command& cmd) {
    //       // push the Lua function, convert args, pcall, convert result
    //   });

    std::cerr << "[plugin] Handlers registered with dispatcher." << std::endl;
}

void LuaHost::shutdown() {
    if (L_) {
        lua_close(L_);
        L_ = nullptr;
        std::cerr << "[plugin] Lua shutdown." << std::endl;
    }
    dispatcher_ = nullptr;
    loaded_plugins_.clear();
}

}  // namespace cef_terminal
