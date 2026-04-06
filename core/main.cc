#include "engine/engine.h"

#include <iostream>
#include <unistd.h>

int main(int argc, char* argv[]) {
    // --- CEF engine ---
    Engine engine;

    if (!engine.initialize(argc, argv)) {
        int code = engine.child_exit_code();
        if (code >= 0)
            return code;  // Clean child process exit
        std::cerr << "[core] Engine init failed." << std::endl;
        return 1;
    }

    std::cerr << "[core] dirtferret-core starting..." << std::endl;

    // TODO: Bind Cap'n Proto RPC server on Unix socket
    // TODO: Wait for UI attachment before sourcing config
    // TODO: Initialize Lua runtime

    // --- Main loop ---
    // For now, just pump CEF. RPC server will be added here.
    bool running = true;
    while (running) {
        engine.do_message_loop_work();
        usleep(1000);  // 1ms — will be replaced by proper event loop
    }

    engine.shutdown();
    std::cerr << "[core] Shutdown." << std::endl;
    return 0;
}
