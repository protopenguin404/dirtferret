// Input handling utilities.
// Translates crossterm events into Cap'n Proto KeyEvent/MouseEvent
// messages for the dirtferret-core RPC.

// TODO: Implement key code mapping (crossterm KeyCode -> Cap'n Proto KeyEvent)
// TODO: Implement mouse coordinate translation (cell -> pixel)
// TODO: Implement modifier flag mapping

/// Convert terminal cell coordinates to pixel coordinates.
/// cell_width and cell_height are the terminal's font metrics in pixels.
pub fn cell_to_pixel(col: u16, row: u16, cell_width: u16, cell_height: u16) -> (i32, i32) {
    (col as i32 * cell_width as i32, row as i32 * cell_height as i32)
}

/// Modifier bitmask values (matching types.capnp definition).
pub const MOD_SHIFT: u32 = 1;
pub const MOD_CTRL: u32 = 2;
pub const MOD_ALT: u32 = 4;
pub const MOD_META: u32 = 8;

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn cell_to_pixel_origin() {
        let (x, y) = cell_to_pixel(0, 0, 8, 16);
        assert_eq!(x, 0);
        assert_eq!(y, 0);
    }

    #[test]
    fn cell_to_pixel_offset() {
        let (x, y) = cell_to_pixel(10, 5, 8, 16);
        assert_eq!(x, 80);
        assert_eq!(y, 80);
    }

    #[test]
    fn cell_to_pixel_different_font() {
        let (x, y) = cell_to_pixel(3, 2, 10, 20);
        assert_eq!(x, 30);
        assert_eq!(y, 40);
    }
}
