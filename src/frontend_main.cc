#include <iostream>

// Frontend entry point.
// This is the terminal client process. It connects to the backend
// via IPC, receives pixel frames, draws them via kitty graphics,
// and sends input events back as commands.
//
// This binary does NOT link against CEF.

int main(int argc, char* argv[]) {
    std::cerr << "[frontend] Starting cef-frontend..." << std::endl;

    // --- Layer setup ---

    // 1. Connect to backend via IPC.
    // TODO: transport.connect(endpoint);

    // 2. Initialize renderer (terminal setup, kitty graphics detection).
    // TODO: renderer.initialize();

    // 3. Initialize input handler (raw mode, mouse capture).
    // TODO: input.initialize();

    std::cerr << "[frontend] All layers initialized (stubs)." << std::endl;

    // --- Main loop ---
    // Will be: poll input → send commands → receive frames → render, in a loop.
    // For now, just exit.

    std::cerr << "[frontend] Shutting down." << std::endl;
    return 0;
}
