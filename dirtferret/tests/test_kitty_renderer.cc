// ============================================================================
// TEST: KittyRenderer
// ============================================================================
//
// These tests verify the pure-function internals of KittyRenderer. They
// WON'T COMPILE until you implement the class.
//
// IMPORTANT DESIGN NOTE:
//   The tests below call bgra_to_rgba() and base64_encode() as PUBLIC
//   STATIC methods of KittyRenderer. These are pure functions (no instance
//   state needed), so making them static is the clean C++ approach.
//
//   If you'd rather make them free functions in the cef_terminal namespace,
//   adjust the test calls from KittyRenderer::method() to just method().
//   Either way, they must be accessible from this test file.
//
// WHAT THESE TESTS FORCE YOU TO IMPLEMENT:
//   - static std::vector<uint8_t> bgra_to_rgba(const uint8_t* bgra, int w, int h)
//   - static std::string base64_encode(const uint8_t* data, size_t len)
//   - void render_frame(const uint8_t* bgra_pixels, int width, int height)
//   - void clear()
//   - Constructor and destructor
//
// ============================================================================
#include <gtest/gtest.h>

#include "frontend/kitty_renderer.h"

#include <cstdint>
#include <string>
#include <vector>

using namespace cef_terminal;

// --- BGRA → RGBA conversion ---

TEST(KittyRenderer_BGRA, SinglePixel) {
    // BGRA: blue=0xAA, green=0xBB, red=0xCC, alpha=0xDD
    uint8_t bgra[] = {0xAA, 0xBB, 0xCC, 0xDD};

    auto rgba = KittyRenderer::bgra_to_rgba(bgra, 1, 1);

    // RGBA: red=0xCC, green=0xBB, blue=0xAA, alpha=0xDD
    ASSERT_EQ(rgba.size(), 4);
    EXPECT_EQ(rgba[0], 0xCC);  // red  (was at [2])
    EXPECT_EQ(rgba[1], 0xBB);  // green (unchanged)
    EXPECT_EQ(rgba[2], 0xAA);  // blue  (was at [0])
    EXPECT_EQ(rgba[3], 0xDD);  // alpha (unchanged)
}

TEST(KittyRenderer_BGRA, MultiplePixels) {
    // 2x1 image: two pixels
    uint8_t bgra[] = {
        0x00, 0x00, 0xFF, 0xFF,  // pixel 0: B=0, G=0, R=FF, A=FF (pure red)
        0xFF, 0x00, 0x00, 0xFF,  // pixel 1: B=FF, G=0, R=0, A=FF (pure blue)
    };

    auto rgba = KittyRenderer::bgra_to_rgba(bgra, 2, 1);

    ASSERT_EQ(rgba.size(), 8);
    // Pixel 0: should be RGBA red
    EXPECT_EQ(rgba[0], 0xFF);  // R
    EXPECT_EQ(rgba[1], 0x00);  // G
    EXPECT_EQ(rgba[2], 0x00);  // B
    EXPECT_EQ(rgba[3], 0xFF);  // A
    // Pixel 1: should be RGBA blue
    EXPECT_EQ(rgba[4], 0x00);  // R
    EXPECT_EQ(rgba[5], 0x00);  // G
    EXPECT_EQ(rgba[6], 0xFF);  // B
    EXPECT_EQ(rgba[7], 0xFF);  // A
}

TEST(KittyRenderer_BGRA, OutputSizeMatchesInput) {
    // 4x3 image = 12 pixels = 48 bytes
    std::vector<uint8_t> bgra(4 * 3 * 4, 0x42);
    auto rgba = KittyRenderer::bgra_to_rgba(bgra.data(), 4, 3);
    EXPECT_EQ(rgba.size(), 48);
}

// --- Base64 encoding ---

TEST(KittyRenderer_Base64, Empty) {
    auto result = KittyRenderer::base64_encode(nullptr, 0);
    EXPECT_EQ(result, "");
}

TEST(KittyRenderer_Base64, RFC4648TestVectors) {
    // These are the canonical test vectors from RFC 4648 section 10.
    auto encode = [](const std::string& s) {
        return KittyRenderer::base64_encode(
            reinterpret_cast<const uint8_t*>(s.data()), s.size());
    };

    EXPECT_EQ(encode("f"),      "Zg==");
    EXPECT_EQ(encode("fo"),     "Zm8=");
    EXPECT_EQ(encode("foo"),    "Zm9v");
    EXPECT_EQ(encode("foob"),   "Zm9vYg==");
    EXPECT_EQ(encode("fooba"),  "Zm9vYmE=");
    EXPECT_EQ(encode("foobar"), "Zm9vYmFy");
}

TEST(KittyRenderer_Base64, BinaryData) {
    // Verify it handles non-ASCII bytes (pixel data is binary).
    uint8_t data[] = {0x00, 0xFF, 0x80, 0x7F, 0x01, 0xFE};
    auto result = KittyRenderer::base64_encode(data, 6);

    // Verify output contains only valid base64 characters.
    for (char c : result) {
        bool valid = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
                  || (c >= '0' && c <= '9') || c == '+' || c == '/'
                  || c == '=';
        EXPECT_TRUE(valid) << "Invalid base64 character: " << c;
    }

    // Output length should be ceil(6/3)*4 = 8
    EXPECT_EQ(result.size(), 8);
}

// --- Renderer lifecycle ---

TEST(KittyRenderer_Lifecycle, ConstructAndDestruct) {
    // Must be constructible and destructible without crashing.
    // The destructor should send a clear command (delete all images),
    // but we don't verify the escape sequence here — just that it
    // doesn't blow up.
    {
        KittyRenderer renderer;
    }
}

TEST(KittyRenderer_Lifecycle, ClearIsCallable) {
    KittyRenderer renderer;
    // clear() sends the delete-all-images escape sequence.
    // We can't easily capture stdout in a unit test, but it must
    // not crash or throw.
    renderer.clear();
}

TEST(KittyRenderer_Lifecycle, RenderFrameAcceptsBGRABuffer) {
    KittyRenderer renderer;

    // Tiny 2x2 BGRA image (16 bytes). This exercises the full pipeline:
    // bgra→rgba conversion, base64 encoding, escape sequence output.
    uint8_t pixels[16] = {
        0x00, 0x00, 0xFF, 0xFF,  // red pixel
        0x00, 0xFF, 0x00, 0xFF,  // green pixel
        0xFF, 0x00, 0x00, 0xFF,  // blue pixel
        0xFF, 0xFF, 0xFF, 0xFF,  // white pixel
    };

    // Must not crash. Actual visual output goes to stdout.
    renderer.render_frame(pixels, 2, 2);
}
