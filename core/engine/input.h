#pragma once

#include <cstdint>

constexpr uint32_t MODIFIER_SHIFT = 1;
constexpr uint32_t MODIFIER_CTRL = 2;
constexpr uint32_t MODIFIER_ALT = 4;
constexpr uint32_t MODIFIER_META = 8;

constexpr uint32_t EVENTFLAG_SHIFT_DOWN = 1 << 1;
constexpr uint32_t EVENTFLAG_CONTROL_DOWN = 1 << 2;
constexpr uint32_t EVENTFLAG_ALT_DOWN = 1 << 3;

struct TranslatedKey {
  int windows_key_code;
  int native_key_code;
  uint32_t character;
  uint32_t unmodified_character;
  uint32_t modifiers;
};

struct TranslatedMouse {
  int x;
  int y;
  int button;
  bool mouse_up;
  int click_count;
  uint32_t modifiers;
};

struct TranslatedScroll {
  int x;
  int y;
  int delta_x;
  int delta_y;
};

struct PixelCoord {
  int x;
  int y;
};

TranslatedKey translate_key(uint32_t character, uint32_t modifiers);
TranslatedMouse translate_mouse(int x, int y, int button, bool is_down, uint32_t modifiers);
TranslatedScroll translate_scroll(int x, int y, int delta_x, int delta_y);
PixelCoord cell_to_pixel(int cell_x, int cell_y, int cell_width, int cell_height);
