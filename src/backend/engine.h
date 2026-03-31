#pragma once

#include "include/cef_app.h"

#include <cstdint>
#include <functional>

namespace cef_terminal {

class Dispatcher;

using FrameCallback = std::function<void(int32_t, const void*, int, int)>;

// The Engine is the backend's interface to CEF.
// It manages buffer (browser) lifecycles and captures rendered frames.
// Only this layer touches CEF headers — everything else goes through
// the Dispatcher via commands and queries.
//
// This is NOT a CefBase-derived class — it's our own class that *owns*
// CefRefPtr'd objects. No IMPLEMENT_REFCOUNTING needed.
class Engine {
 public:
    Engine();
    ~Engine();

    // Initialize CEF. Returns false if this is a CEF child process
    // (caller should exit with child_exit_code()) or if init fails.
    bool initialize(int argc, char* argv[]);

    // If initialize() returned false, this is the child process exit code.
    // -1 means it wasn't a child process (init failed for another reason).
    int child_exit_code() const { return child_exit_code_; }

    // Run one iteration of CEF's message loop.
    void tick();

    // Shut down CEF cleanly.
    void shutdown();

    // TODO: wire these up
    void register_handlers(Dispatcher& dispatcher) {}
    void set_frame_callback(FrameCallback cb) {}

 private:
    CefRefPtr<CefApp> app_;
    bool initialized_ = false;
    int child_exit_code_ = -1;
};

} // namespace cef_terminal
