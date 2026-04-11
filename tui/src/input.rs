use crossterm::event::{KeyCode, KeyModifiers};

/// Translate crossterm key to (character/keycode, cef_modifiers).
pub fn key_to_cef(code: KeyCode, modifiers: KeyModifiers) -> (u32, u32) {
    let character = match code {
        KeyCode::Char(c) => c as u32,
        KeyCode::Enter => '\r' as u32,
        KeyCode::Tab => '\t' as u32,
        KeyCode::Backspace => 0x08,
        KeyCode::Delete => 0x2E,
        KeyCode::Left => 0x25,
        KeyCode::Right => 0x27,
        KeyCode::Up => 0x26,
        KeyCode::Down => 0x28,
        KeyCode::Home => 0x24,
        KeyCode::End => 0x23,
        KeyCode::PageUp => 0x21,
        KeyCode::PageDown => 0x22,
        KeyCode::Esc => 0x1B,
        _ => 0,
    };
    let mut mods: u32 = 0;
    if modifiers.contains(KeyModifiers::SHIFT) { mods |= 1; }
    if modifiers.contains(KeyModifiers::CONTROL) { mods |= 2; }
    if modifiers.contains(KeyModifiers::ALT) { mods |= 4; }
    (character, mods)
}

/// Convert crossterm mouse modifiers to CEF modifier bitmask.
pub fn mouse_mods_to_cef(modifiers: KeyModifiers) -> u32 {
    let mut mods: u32 = 0;
    if modifiers.contains(KeyModifiers::SHIFT) { mods |= 1; }
    if modifiers.contains(KeyModifiers::CONTROL) { mods |= 2; }
    if modifiers.contains(KeyModifiers::ALT) { mods |= 4; }
    mods
}

/// Convert terminal cell coordinates to pixel coordinates.
pub fn cell_to_pixel(col: u16, row: u16) -> (i32, i32) {
    let Ok(win) = crossterm::terminal::window_size() else {
        return (col as i32 * 8, row as i32 * 16);
    };
    if win.columns == 0 || win.rows == 0 {
        return (col as i32 * 8, row as i32 * 16);
    }
    let cell_w = win.width as f64 / win.columns as f64;
    let cell_h = win.height as f64 / win.rows as f64;
    ((col as f64 * cell_w) as i32, (row as f64 * cell_h) as i32)
}

/// Get terminal viewport size in pixels.
pub fn viewport_pixel_size() -> anyhow::Result<(u32, u32)> {
    let win = crossterm::terminal::window_size()?;
    if win.width > 0 && win.height > 0 {
        Ok((win.width as u32, win.height as u32))
    } else {
        Ok((win.columns as u32 * 8, win.rows as u32 * 16))
    }
}
