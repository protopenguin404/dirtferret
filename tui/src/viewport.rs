// Viewport rendering — reads pixel data from shared memory
// and outputs kitty graphics protocol escape sequences.

/// Convert a BGRA pixel buffer to RGBA in-place.
/// CEF outputs BGRA, kitty graphics protocol expects RGBA.
pub fn bgra_to_rgba(pixels: &mut [u8]) {
    for chunk in pixels.chunks_exact_mut(4) {
        chunk.swap(0, 2); // swap B and R
    }
}

/// Generate a kitty graphics protocol escape sequence for displaying
/// an image from shared memory.
///
/// Parameters:
/// - shm_name: POSIX shared memory name (e.g., "/dirtferret-frame-0")
/// - width, height: pixel dimensions
/// - image_id: kitty image ID for replacement
/// - cols, rows: display area in terminal cells
///
/// Returns the complete escape sequence as bytes.
pub fn kitty_shm_display(
    shm_name: &str,
    width: u32,
    height: u32,
    image_id: u32,
    cols: u16,
    rows: u16,
) -> Vec<u8> {
    // Kitty graphics protocol:
    // ESC _G <params> ; <payload> ESC \
    //
    // For shared memory mode:
    //   a=T  — transmit and display
    //   t=s  — shared memory transmission
    //   f=32 — 32-bit RGBA pixel format
    //   s=W  — pixel width
    //   v=H  — pixel height
    //   i=ID — image ID (for replacement)
    //   c=C  — display columns
    //   r=R  — display rows
    //   C=1  — do not move cursor
    format!(
        "\x1b_Ga=T,t=s,f=32,s={},v={},i={},c={},r={},C=1;{}\x1b\\",
        width, height, image_id, cols, rows, shm_name
    )
    .into_bytes()
}

/// Generate a kitty graphics protocol escape sequence to delete
/// an image by ID.
pub fn kitty_delete_image(image_id: u32) -> Vec<u8> {
    format!("\x1b_Ga=d,d=i,i={}\x1b\\", image_id).into_bytes()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn bgra_to_rgba_conversion() {
        // BGRA: Blue=0, Green=128, Red=255, Alpha=255
        let mut pixels = vec![0u8, 128, 255, 255];
        bgra_to_rgba(&mut pixels);
        // RGBA: Red=255, Green=128, Blue=0, Alpha=255
        assert_eq!(pixels, vec![255, 128, 0, 255]);
    }

    #[test]
    fn bgra_to_rgba_multiple_pixels() {
        let mut pixels = vec![
            0, 0, 255, 255, // blue pixel -> red
            0, 255, 0, 255, // green pixel -> green (no change for G/A)
            255, 0, 0, 255, // red pixel -> blue
        ];
        bgra_to_rgba(&mut pixels);
        assert_eq!(
            pixels,
            vec![
                255, 0, 0, 255, // was blue, now red
                0, 255, 0, 255, // green unchanged
                0, 0, 255, 255, // was red, now blue
            ]
        );
    }

    #[test]
    fn bgra_to_rgba_empty() {
        let mut pixels: Vec<u8> = vec![];
        bgra_to_rgba(&mut pixels);
        assert!(pixels.is_empty());
    }

    #[test]
    fn kitty_escape_contains_params() {
        let seq = kitty_shm_display("/test-frame", 800, 600, 1, 100, 37);
        let s = String::from_utf8(seq).unwrap();

        assert!(s.starts_with("\x1b_G"));
        assert!(s.ends_with("\x1b\\"));
        assert!(s.contains("a=T"));
        assert!(s.contains("t=s"));
        assert!(s.contains("f=32"));
        assert!(s.contains("s=800"));
        assert!(s.contains("v=600"));
        assert!(s.contains("i=1"));
        assert!(s.contains("/test-frame"));
    }

    #[test]
    fn kitty_delete_escape() {
        let seq = kitty_delete_image(42);
        let s = String::from_utf8(seq).unwrap();

        assert!(s.starts_with("\x1b_G"));
        assert!(s.contains("a=d"));
        assert!(s.contains("i=42"));
    }
}
