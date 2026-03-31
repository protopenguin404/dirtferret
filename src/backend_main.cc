#include "api/dispatcher.h"

#include <iostream>

// Backend entry point.
// This will become the daemon/server process that runs CEF,
// hosts the API dispatcher and plugin runtime, and serves
// connected frontends via IPC.
//
// For now it just proves the layers wire together.

int main(int argc, char* argv[]) {
    std::cerr << "[backend] Starting cef-backend..." << std::endl;

    // --- Layer setup (order matters) ---

    // 1. API layer — the central hub, created first so layers can register with it.
    cef_terminal::Dispatcher dispatcher;

    // 2. CEF engine — will be initialized here, registers its handlers.
    // TODO: Engine engine; engine.initialize(argc, argv);
    //       engine.register_handlers(dispatcher);

    // 3. Plugin runtime — loads plugins, registers their handlers.
    // TODO: PluginHost plugins; plugins.initialize();
    //       plugins.register_handlers(dispatcher);

    // 4. IPC server — listens for frontend connections.
    // TODO: transport.listen(endpoint, on_client);

    std::cerr << "[backend] All layers initialized (stubs)." << std::endl;

    // --- Main loop ---
    // Will be: CEF tick + IPC poll + dispatch, in a loop.
    // For now, just exit.

    std::cerr << "[backend] Shutting down." << std::endl;
    return 0;
}
