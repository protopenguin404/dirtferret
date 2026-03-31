#pragma once

#include "api/command.h"
#include <functional>
#include <optional>

namespace cef_terminal {

// The InputHandler reads keyboard and mouse events from the terminal
// and translates them into Commands to send to the backend.
// What a key does is policy — that logic lives in the plugin layer.
// This layer only captures raw input and forwards it.
class InputHandler {
 public:
    virtual ~InputHandler() = default;

    // Initialize terminal input (raw mode, mouse capture, etc.).
    virtual bool initialize() = 0;

    // Non-blocking poll for the next input event.
    // Returns a Command representing the raw input, or nullopt if
    // no input is available.
    virtual std::optional<Command> poll() = 0;

    // Restore terminal input state.
    virtual void shutdown() = 0;
};

}  // namespace cef_terminal
