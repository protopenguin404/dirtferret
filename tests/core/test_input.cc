#include <gtest/gtest.h>
#include "engine/input.h"

TEST(InputTranslation, PrintableChar_a) {
    auto result = translate_key('a', 0);
    EXPECT_EQ(result.windows_key_code, 0x41);
    EXPECT_EQ(result.character, (uint32_t)'a');
    EXPECT_EQ(result.unmodified_character, (uint32_t)'a');
}

TEST(InputTranslation, PrintableChar_A_Shift) {
    auto result = translate_key('A', MODIFIER_SHIFT);
    EXPECT_EQ(result.windows_key_code, 0x41);
    EXPECT_EQ(result.character, (uint32_t)'A');
    EXPECT_EQ(result.unmodified_character, (uint32_t)'A');
}

TEST(InputTranslation, Enter) {
    auto result = translate_key('\r', 0);
    EXPECT_EQ(result.windows_key_code, 0x0D);
    EXPECT_EQ(result.character, (uint32_t)'\r');
}

TEST(InputTranslation, Escape) {
    auto result = translate_key(0x1B, 0);
    EXPECT_EQ(result.windows_key_code, 0x1B);
}

TEST(InputTranslation, Tab) {
    auto result = translate_key('\t', 0);
    EXPECT_EQ(result.windows_key_code, 0x09);
}

TEST(InputTranslation, Backspace) {
    auto result = translate_key(0x08, 0);
    EXPECT_EQ(result.windows_key_code, 0x08);
}

TEST(InputTranslation, CtrlC) {
    auto result = translate_key('c', MODIFIER_CTRL);
    EXPECT_EQ(result.windows_key_code, 0x43);
    EXPECT_EQ(result.character, (uint32_t)3);
    EXPECT_TRUE(result.modifiers & EVENTFLAG_CONTROL_DOWN);
}

TEST(InputTranslation, MouseClick) {
    auto result = translate_mouse(100, 200, 0, true, 0);
    EXPECT_EQ(result.x, 100);
    EXPECT_EQ(result.y, 200);
    EXPECT_EQ(result.button, 0);
    EXPECT_FALSE(result.mouse_up);
    EXPECT_EQ(result.click_count, 1);
}

TEST(InputTranslation, MouseScroll) {
    auto result = translate_scroll(50, 60, 0, -3);
    EXPECT_EQ(result.x, 50);
    EXPECT_EQ(result.y, 60);
    EXPECT_EQ(result.delta_x, 0);
    EXPECT_EQ(result.delta_y, -360);
}

TEST(InputTranslation, CellToPixel) {
    auto result = cell_to_pixel(10, 5, 8, 16);
    EXPECT_EQ(result.x, 80);
    EXPECT_EQ(result.y, 80);
}
