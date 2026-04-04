#pragma once

#include <string>

namespace cef_terminal {

class Dispatcher;

// The PluginHost manages the plugin runtime (Lua or other).
// All policy logic lives in plugins — the host just provides
// the execution environment and wires plugins into the dispatcher.
class PluginHost {
 public:
    virtual ~PluginHost() = default;

    // Initialize the plugin runtime.
    virtual bool initialize() = 0;

    // Load a plugin from a file path.
    virtual bool load_plugin(const std::string& path) = 0;

    // Register the plugin system's handlers with the dispatcher.
    // This allows plugins to both handle and emit commands/queries.
    virtual void register_handlers(Dispatcher& dispatcher) = 0;

    // Shut down the plugin runtime.
    virtual void shutdown() = 0;
};

}  // namespace cef_terminal
