#include "api/dispatcher.h"
#include "backend/engine.h"

#include <iostream>

int main(int argc, char* argv[]) {
    // --- CEF engine ---
    cef_terminal::Engine engine;

    if (!engine.initialize(argc, argv)) {
        // Either we're a CEF child process (normal), or init failed.
        int code = engine.child_exit_code();
        if (code >= 0) {
            return code;  // clean child process exit
        }
        std::cerr << "[backend] Engine init failed." << std::endl;
        return 1;
    }

    std::cerr << "[backend] Starting cef-backend..." << std::endl;

    // --- API layer ---
    cef_terminal::Dispatcher dispatcher;
    engine.register_handlers(dispatcher);

    // --- Main loop ---
    // For now, just tick CEF. Later: IPC poll + dispatch.
    std::cerr << "[backend] Entering main loop..." << std::endl;
    bool running = true;
    while (running) {
        engine.tick();
        // TODO: poll IPC, dispatch commands
        // TODO: exit condition (for now, CEF message loop will just run)
    }

    engine.shutdown();
    std::cerr << "[backend] Shutdown." << std::endl;
    return 0;
}
