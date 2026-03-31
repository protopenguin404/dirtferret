#pragma once

#include <cstdint>

namespace cef_terminal {

// The Renderer is the frontend's pixel output interface.
// It takes raw pixel data and draws it to the terminal using
// the kitty graphics protocol (or a future alternative).
class Renderer {
 public:
    virtual ~Renderer() = default;

    // Initialize the renderer (terminal setup, capability detection).
    virtual bool initialize() = 0;

    // Draw a frame to the terminal.
    // data is BGRA pixel data, width x height.
    virtual void draw_frame(const void* data, int width, int height) = 0;

    // Get the terminal's available dimensions in pixels.
    virtual void get_dimensions(int& width, int& height) const = 0;

    // Clean up terminal state.
    virtual void shutdown() = 0;
};

}  // namespace cef_terminal
