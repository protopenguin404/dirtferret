#include "frontend/kitty_renderer.h"

#include <cstring>
#include <iostream>
#include <sys/ioctl.h>
#include <unistd.h>

namespace cef_terminal {

// Base64 encoding table.
static const char b64_table[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static std::string base64_encode(const uint8_t* data, size_t len) {
    std::string out;
    out.reserve(((len + 2) / 3) * 4);

    for (size_t i = 0; i < len; i += 3) {
        uint32_t n = static_cast<uint32_t>(data[i]) << 16;
        if (i + 1 < len) n |= static_cast<uint32_t>(data[i + 1]) << 8;
        if (i + 2 < len) n |= static_cast<uint32_t>(data[i + 2]);

        out.push_back(b64_table[(n >> 18) & 0x3F]);
        out.push_back(b64_table[(n >> 12) & 0x3F]);
        out.push_back((i + 1 < len) ? b64_table[(n >> 6) & 0x3F] : '=');
        out.push_back((i + 2 < len) ? b64_table[n & 0x3F] : '=');
    }
    return out;
}

bool KittyRenderer::initialize() {
    // Query terminal pixel dimensions via ioctl.
    struct winsize ws {};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_xpixel > 0) {
        term_width_ = ws.ws_xpixel;
        term_height_ = ws.ws_ypixel;
        std::cerr << "[render] Terminal: " << term_width_ << "x"
                  << term_height_ << " pixels" << std::endl;
    } else {
        // Fallback — common default.
        term_width_ = 800;
        term_height_ = 600;
        std::cerr << "[render] Could not query terminal size, "
                  << "using fallback " << term_width_ << "x"
                  << term_height_ << std::endl;
    }

    return true;
}

void KittyRenderer::bgra_to_rgba(const void* src, int pixel_count) {
    rgba_buf_.resize(pixel_count * 4);
    const auto* s = static_cast<const uint8_t*>(src);
    auto* d = rgba_buf_.data();

    for (int i = 0; i < pixel_count; ++i) {
        d[0] = s[2];  // R ← B
        d[1] = s[1];  // G ← G
        d[2] = s[0];  // B ← R
        d[3] = s[3];  // A ← A
        s += 4;
        d += 4;
    }
}

void KittyRenderer::write_chunk(const std::string& b64, int width, int height,
                                bool first, bool last) {
    // Kitty graphics escape: ESC _ G <payload> ESC backslash
    // ESC _ is APC (Application Program Command).
    std::string esc = "\033_G";

    if (first) {
        // a=T: transmit and display
        // f=32: RGBA (32 bits per pixel)
        // s=width, v=height
        esc += "a=T,f=32,s=" + std::to_string(width) +
               ",v=" + std::to_string(height);
    }

    // m=1 means more chunks follow; m=0 means this is the last.
    esc += first ? ",m=" : "m=";
    esc += last ? "0" : "1";

    esc += ";";
    esc += b64;
    esc += "\033\\";

    // Write to stdout (the terminal).
    ::write(STDOUT_FILENO, esc.data(), esc.size());
}

void KittyRenderer::draw_frame(const void* data, int width, int height) {
    int pixel_count = width * height;
    bgra_to_rgba(data, pixel_count);

    std::string b64 = base64_encode(rgba_buf_.data(), rgba_buf_.size());

    // Move cursor to top-left before drawing.
    ::write(STDOUT_FILENO, "\033[H", 3);

    // Kitty protocol recommends chunking at 4096 bytes of base64 per sequence.
    static constexpr size_t CHUNK_SIZE = 4096;

    if (b64.size() <= CHUNK_SIZE) {
        write_chunk(b64, width, height, true, true);
    } else {
        for (size_t offset = 0; offset < b64.size(); offset += CHUNK_SIZE) {
            size_t remaining = b64.size() - offset;
            size_t len = std::min(remaining, CHUNK_SIZE);
            std::string chunk = b64.substr(offset, len);

            bool first = (offset == 0);
            bool last = (offset + len >= b64.size());
            write_chunk(chunk, width, height, first, last);
        }
    }
}

void KittyRenderer::get_dimensions(int& width, int& height) const {
    width = term_width_;
    height = term_height_;
}

void KittyRenderer::shutdown() {
    std::cerr << "[render] Shutdown." << std::endl;
}

}  // namespace cef_terminal
