#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace cef_terminal {

class Dispatcher;

// Callback for when a buffer produces a new frame.
// Receives: buffer_id, pixel data (BGRA), width, height.
using FrameCallback = std::function<void(int32_t, const void*, int, int)>;

// The Engine is the backend's interface to CEF.
// It manages buffer (browser) lifecycles and captures rendered frames.
// Only this layer touches CEF headers — everything else goes through
// the Dispatcher via commands and queries.
class Engine {
 public:
    virtual ~Engine() = default;

    // Initialize CEF and start the engine.
    virtual bool initialize(int argc, char* argv[]) = 0;

    // Run one iteration of CEF's message loop.
    // Call this from the backend's main loop instead of CefRunMessageLoop().
    virtual void tick() = 0;

    // Shut down CEF cleanly.
    virtual void shutdown() = 0;

    // Register command/query handlers with the dispatcher.
    // Called once during setup to wire the engine into the API layer.
    virtual void register_handlers(Dispatcher& dispatcher) = 0;

    // Set the callback for new frames. The engine calls this whenever
    // a buffer's OnPaint fires with new pixel data.
    virtual void set_frame_callback(FrameCallback cb) = 0;
};

}  // namespace cef_terminal
