#include "backend/engine.h"
#include "backend/app.h"
#include "include/cef_app.h"

#include <iostream>

namespace cef_terminal {

Engine::Engine() = default;
Engine::~Engine() = default;

bool Engine::initialize(int argc, char* argv[]) {
    CefMainArgs main_args(argc, argv);
    app_ = new MinimalApp();

    // CEF child process check — if this is a renderer/GPU/utility process,
    // CefExecuteProcess handles it and returns >= 0. We must exit.
    int exit_code = CefExecuteProcess(main_args, app_, nullptr);
    if (exit_code >= 0) {
        // We are a CEF child process. Caller should exit with this code.
        child_exit_code_ = exit_code;
        return false;
    }

    CefSettings settings;
    settings.windowless_rendering_enabled = true;
    settings.no_sandbox = true;

    if (!CefInitialize(main_args, settings, app_, nullptr)) {
        std::cerr << "[cef] Failed to initialize." << std::endl;
        return false;
    }

    std::cerr << "[cef] Initialized." << std::endl;
    initialized_ = true;
    return true;
}

void Engine::tick() {
    if (initialized_) {
        CefDoMessageLoopWork();
    }
}

void Engine::shutdown() {
    if (initialized_) {
        CefShutdown();
        initialized_ = false;
        std::cerr << "[cef] Shutdown complete." << std::endl;
    }
}

} // namespace cef_terminal
