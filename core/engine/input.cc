#include "engine/input.h"

static uint32_t capnp_to_cef_modifiers(uint32_t mods) {
  uint32_t flags = 0;
  if (mods & MODIFIER_SHIFT) flags |= DF_EVENTFLAG_SHIFT_DOWN;
  if (mods & MODIFIER_CTRL) flags |= DF_EVENTFLAG_CONTROL_DOWN;
  if (mods & MODIFIER_ALT) flags |= DF_EVENTFLAG_ALT_DOWN;
  return flags;
}

static int char_to_vk(uint32_t ch) {
  if (ch >= 'a' && ch <= 'z') return 0x41 + (ch - 'a');
  if (ch >= 'A' && ch <= 'Z') return 0x41 + (ch - 'A');
  if (ch >= '0' && ch <= '9') return 0x30 + (ch - '0');
  switch (ch) {
  case '\r': case '\n': return 0x0D;
  case 0x1B: return 0x1B;
  case '\t': return 0x09;
  case 0x08: return 0x08;
  case ' ': return 0x20;
  default: return ch;
  }
}

TranslatedKey translate_key(uint32_t character, uint32_t modifiers) {
  TranslatedKey result;
  result.windows_key_code = char_to_vk(character);
  result.native_key_code = result.windows_key_code;
  result.modifiers = capnp_to_cef_modifiers(modifiers);
  if (modifiers & MODIFIER_CTRL) {
    if (character >= 'a' && character <= 'z')
      result.character = character - 'a' + 1;
    else if (character >= 'A' && character <= 'Z')
      result.character = character - 'A' + 1;
    else
      result.character = character;
  } else {
    result.character = character;
  }
  result.unmodified_character = character;
  return result;
}

TranslatedMouse translate_mouse(int x, int y, int button, bool is_down, uint32_t modifiers) {
  TranslatedMouse result;
  result.x = x;
  result.y = y;
  result.button = button;
  result.mouse_up = !is_down;
  result.click_count = 1;
  result.modifiers = capnp_to_cef_modifiers(modifiers);
  return result;
}

TranslatedScroll translate_scroll(int x, int y, int delta_x, int delta_y) {
  TranslatedScroll result;
  result.x = x;
  result.y = y;
  result.delta_x = delta_x * 120;
  result.delta_y = delta_y * 120;
  return result;
}

PixelCoord cell_to_pixel(int cell_x, int cell_y, int cell_width, int cell_height) {
  return {cell_x * cell_width, cell_y * cell_height};
}
