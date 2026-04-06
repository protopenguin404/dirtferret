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

/// Generate a kitty graphics escape sequence for direct inline display.
/// Handles chunked transfer for payloads > 4096 bytes.
///
/// `pixels` must already be RGBA format.
pub fn kitty_direct_display(
    pixels: &[u8],
    width: u32,
    height: u32,
    image_id: u32,
    cols: u16,
    rows: u16,
) -> Vec<u8> {
    use std::fmt::Write;

    let b64 = base64_encode(pixels);
    let mut output = String::new();
    let chunk_size = 4096;

    if b64.len() <= chunk_size {
        write!(
            output,
            "\x1b_Ga=T,f=32,s={},v={},i={},c={},r={},C=1;{}\x1b\\",
            width, height, image_id, cols, rows, b64
        ).unwrap();
    } else {
        let chunks: Vec<&[u8]> = b64.as_bytes().chunks(chunk_size).collect();
        for (i, chunk) in chunks.iter().enumerate() {
            let m = if i < chunks.len() - 1 { 1 } else { 0 };
            let chunk_str = std::str::from_utf8(chunk).unwrap();
            if i == 0 {
                write!(
                    output,
                    "\x1b_Ga=T,f=32,s={},v={},i={},c={},r={},C=1,m={};{}\x1b\\",
                    width, height, image_id, cols, rows, m, chunk_str
                ).unwrap();
            } else {
                write!(output, "\x1b_Gm={};{}\x1b\\", m, chunk_str).unwrap();
            }
        }
    }

    output.into_bytes()
}

/// Simple base64 encoder (no external dep needed for tests).
fn base64_encode(data: &[u8]) -> String {
    const CHARS: &[u8] = b"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    let mut result = String::with_capacity((data.len() + 2) / 3 * 4);
    for chunk in data.chunks(3) {
        let b0 = chunk[0] as u32;
        let b1 = if chunk.len() > 1 { chunk[1] as u32 } else { 0 };
        let b2 = if chunk.len() > 2 { chunk[2] as u32 } else { 0 };
        let n = (b0 << 16) | (b1 << 8) | b2;
        result.push(CHARS[((n >> 18) & 0x3F) as usize] as char);
        result.push(CHARS[((n >> 12) & 0x3F) as usize] as char);
        if chunk.len() > 1 {
            result.push(CHARS[((n >> 6) & 0x3F) as usize] as char);
        } else {
            result.push('=');
        }
        if chunk.len() > 2 {
            result.push(CHARS[(n & 0x3F) as usize] as char);
        } else {
            result.push('=');
        }
    }
    result
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

    // --- Direct display (inline base64) ---

    #[test]
    fn direct_display_small_image() {
        // 1x1 RGBA pixel (4 bytes) — well under 4096 chunk limit
        let pixels = vec![255u8, 0, 0, 255]; // red pixel
        let seq = kitty_direct_display(&pixels, 1, 1, 1, 1, 1);
        let s = String::from_utf8(seq).unwrap();

        assert!(s.starts_with("\x1b_G"));
        assert!(s.contains("a=T"));
        assert!(s.contains("f=32"));
        assert!(s.contains("s=1"));
        assert!(s.contains("v=1"));
        // Should be a single chunk (no m= param, or m=0 implicitly)
        assert!(!s.contains("m=1"));
    }

    #[test]
    fn direct_display_large_image_is_chunked() {
        // Make a payload that exceeds 4096 base64 chars
        // 4096 base64 chars = 3072 bytes of raw data
        // So 3100 bytes should produce a chunked transfer
        let pixels = vec![128u8; 3100];
        let seq = kitty_direct_display(&pixels, 100, 8, 1, 50, 4);
        let s = String::from_utf8(seq).unwrap();

        // Should contain m=1 (more data) and m=0 (last chunk)
        assert!(s.contains("m=1"));
        assert!(s.contains("m=0"));

        // First chunk has the image params
        assert!(s.contains("a=T"));
        assert!(s.contains("s=100"));
    }

    // --- Base64 encoding ---

    #[test]
    fn base64_encode_basic() {
        assert_eq!(base64_encode(b"Hello"), "SGVsbG8=");
        assert_eq!(base64_encode(b""), "");
        assert_eq!(base64_encode(b"f"), "Zg==");
        assert_eq!(base64_encode(b"fo"), "Zm8=");
        assert_eq!(base64_encode(b"foo"), "Zm9v");
    }

    // --- BGRA conversion preserves alpha ---

    #[test]
    fn bgra_to_rgba_preserves_alpha() {
        let mut pixels = vec![
            0, 0, 255, 128,   // semi-transparent red (in BGRA)
        ];
        bgra_to_rgba(&mut pixels);
        assert_eq!(pixels[3], 128); // alpha unchanged
        assert_eq!(pixels[0], 255); // R (was at position 2)
    }

    // --- Partial pixel buffer (not multiple of 4) ---

    #[test]
    fn bgra_to_rgba_ignores_trailing_bytes() {
        // chunks_exact_mut skips trailing bytes that don't form a full pixel
        let mut pixels = vec![0, 128, 255, 255, 99, 99]; // 4 bytes + 2 trailing
        bgra_to_rgba(&mut pixels);
        assert_eq!(pixels[0], 255); // B and R swapped in first pixel
        assert_eq!(pixels[4], 99);  // trailing bytes untouched
        assert_eq!(pixels[5], 99);
    }
}
