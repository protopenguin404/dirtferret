#pragma once

#include "plugin/plugin_host.h"

#include <string>
#include <vector>

// Forward-declare Lua state to keep lua.h out of our header.
struct lua_State;

namespace cef_terminal {

class Dispatcher;

// Lua-based plugin host. Embeds a Lua 5.4 runtime (swappable to LuaJIT
// later behind the PluginHost abstraction).
//
// Plugins are .lua files. Each gets its own environment table so plugins
// can't accidentally stomp each other's globals. The host exposes a
// bridge table ("cef") to Lua that lets plugins register command/query
// handlers and emit commands/queries through the dispatcher.
//
// Lua-side API (exposed to plugins):
//   cef.on_command(name, function(args) ... end)
//   cef.on_query(name, function(args) return data end)
//   cef.command(name, args)
//   cef.query(name, args) -> data
class LuaHost : public PluginHost {
 public:
    LuaHost();
    ~LuaHost() override;

    bool initialize() override;
    bool load_plugin(const std::string& path) override;
    void register_handlers(Dispatcher& dispatcher) override;
    void shutdown() override;

 private:
    // Set up the "cef" bridge table in the Lua state.
    void setup_bridge();

    // Register a single Lua function as a C closure.
    void register_lua_func(const char* table, const char* name,
                           int (*func)(lua_State*));

    lua_State* L_ = nullptr;
    Dispatcher* dispatcher_ = nullptr;
    std::vector<std::string> loaded_plugins_;
};

}  // namespace cef_terminal
