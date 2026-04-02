// ============================================================================
// LEARNING TASK: KittyRenderer
// ============================================================================
//
// PURPOSE:
//   Takes a raw pixel buffer (BGRA, 4 bytes per pixel) and draws it to the
//   terminal using the kitty graphics protocol. This is the frontend's
//   "screen" — it turns CEF's rendered frames into something you can see.
//
// ---- WHAT THIS CLASS NEEDS TO DO ----
//
// Construction / setup:
//   - Detect the terminal's size in both characters AND pixels. You need
//     pixel dimensions to know how to scale/place the image.
//     HINT: ioctl() with TIOCGWINSZ gives you a winsize struct containing
//     ws_col, ws_row (char size) and ws_xpixel, ws_ypixel (pixel size).
//     Header: <sys/ioctl.h>, operates on STDOUT_FILENO.
//     PITFALL: Some terminals report 0 for pixel dimensions. You'll want
//     a fallback (assume 8x16 cell size, or query via escape sequence).
//
// Core method — render_frame(const uint8_t* pixels, int width, int height):
//   - Takes a BGRA pixel buffer (this is what CEF gives us in OnPaint).
//   - Encodes it into kitty graphics protocol escape sequences.
//   - Writes those escape sequences to stdout.
//
//   THE KITTY GRAPHICS PROTOCOL:
//     The basic escape sequence is:
//       \033_G<key>=<value>,<key>=<value>,...;<base64 payload>\033\\
//
//     Key parameters you'll need:
//       a=T    — action: transmit and display
//       f=32   — pixel format: 32-bit RGBA (you'll need to convert BGRA→RGBA)
//       s=W    — image width in pixels
//       v=H    — image height in pixels
//       t=d    — transmission: direct (payload is inline base64)
//       m=0    — no more chunks (or m=1 if there are more chunks to follow)
//
//     For images larger than 4096 bytes of base64 data, you MUST chunk:
//       - First chunk:  \033_Ga=T,f=32,s=W,v=H,m=1;<base64 chunk>\033\\
//       - Middle chunks: \033_Gm=1;<base64 chunk>\033\\
//       - Final chunk:   \033_Gm=0;<base64 chunk>\033\\
//
//     PITFALL: BGRA ≠ RGBA. CEF gives BGRA (blue first). Kitty wants RGBA.
//     You must swizzle each pixel: swap bytes [0] and [2] for every 4-byte
//     group. Do this BEFORE base64 encoding.
//
//   BASE64 ENCODING:
//     You need to encode raw pixel bytes as base64 text. Options:
//     - Write your own (~30 lines, table-lookup, good exercise)
//     - Use a small header-only lib
//     - The lookup table approach: for every 3 input bytes, produce 4 output
//       chars from "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"
//     HINT: std::vector<uint8_t> for the converted RGBA buffer,
//           std::string for the base64 output.
//
//   WRITING TO TERMINAL:
//     Use write(STDOUT_FILENO, ...) or std::cout. write() is better for
//     binary-ish data and avoids buffering surprises.
//     HINT: Build the entire escape sequence as a std::string first, then
//     write it in one shot. Partial writes of escape sequences will produce
//     garbage.
//
// Cleanup:
//   - Kitty images persist until cleared. On destruction or between frames,
//     send: \033_Ga=d;\033\\  (action=delete, deletes all images)
//
// ---- SUGGESTED METHODS ----
//
//   KittyRenderer();
//   ~KittyRenderer();                          // clear images on teardown
//   void render_frame(const uint8_t* bgra_pixels, int width, int height);
//   void clear();                              // send delete-all-images
//
// Private helpers you'll likely want:
//   std::string base64_encode(const uint8_t* data, size_t len);
//   std::vector<uint8_t> bgra_to_rgba(const uint8_t* bgra, int w, int h);
//   void query_terminal_size();                // fills internal w/h state
//
// ---- ARCHITECTURAL CONTEXT ----
//
//   - This class lives in the frontend process. It never touches CEF headers.
//   - It receives pixel data that came across IPC from the backend.
//   - frontend_main.cc will create a KittyRenderer, then in its main loop:
//     1. Check for incoming FRAME messages from the backend
//     2. Extract pixel buffer from the message payload
//     3. Call renderer.render_frame(pixels, width, height)
//   - The Message struct (ipc/message.h) has width, height, buffer_id fields
//     specifically for FRAME messages. The payload carries the raw pixels.
//
// ---- CPP THINGS YOU'LL WANT TO READ UP ON ----
//
//   - std::vector<uint8_t> — contiguous byte buffer, use .data() for raw ptr
//   - std::string — also contiguous, good for building escape sequences
//   - write(2) — POSIX unbuffered write to file descriptor
//   - ioctl(2) + TIOCGWINSZ — terminal size query
//   - static const arrays — for the base64 lookup table
//   - size_t — the type for sizes/lengths (unsigned, platform-width)
//
// ============================================================================
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace cef_terminal {

class KittyRenderer {
    // YOUR IMPLEMENTATION HERE
};

}  // namespace cef_terminal
