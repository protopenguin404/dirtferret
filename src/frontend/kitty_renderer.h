#pragma once

#include "frontend/renderer.h"

#include <cstdint>
#include <string>

namespace cef_terminal {

// Kitty graphics protocol renderer.
// Takes raw BGRA pixel buffers (from CEF via IPC) and draws them
// to a kitty-protocol-capable terminal.
//
// Protocol summary:
//   ESC_G <control>;<base64 payload> ST
//   Control keys: a=action, f=format, s=width, t=height, m=more-data
//   We transmit+display (a=T) with RGBA format (f=32).
//   Payloads are chunked at 4096 bytes of base64 per escape sequence.
class KittyRenderer : public Renderer {
 public:
    bool initialize() override;
    void draw_frame(const void* data, int width, int height) override;
    void get_dimensions(int& width, int& height) const override;
    void shutdown() override;

 private:
    // Convert BGRA → RGBA in-place in a working buffer.
    void bgra_to_rgba(const void* src, int pixel_count);

    // Write a single kitty graphics escape sequence chunk.
    // first: include control data (dimensions, format).
    // last: set m=0 (no more chunks).
    void write_chunk(const std::string& b64, int width, int height,
                     bool first, bool last);

    // RGBA working buffer — reused across frames to avoid allocation.
    std::vector<uint8_t> rgba_buf_;

    // Cached terminal dimensions in pixels.
    int term_width_ = 0;
    int term_height_ = 0;
};

}  // namespace cef_terminal
