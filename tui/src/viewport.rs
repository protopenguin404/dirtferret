use base64::Engine as _;
use std::io::Write;

/// Convert BGRA pixel buffer to RGBA in-place.
pub fn bgra_to_rgba(pixels: &mut [u8]) {
    for chunk in pixels.chunks_exact_mut(4) {
        chunk.swap(0, 2);
    }
}

/// Send RGBA pixels directly inline via base64, chunked per kitty spec.
/// Each chunk is at most 4096 base64 chars (3072 raw bytes).
/// This avoids the file intermediary entirely.
pub fn kitty_display_direct(
    rgba: &[u8],
    width: u32,
    height: u32,
    image_id: u32,
) -> Vec<u8> {
    let b64 = base64::engine::general_purpose::STANDARD.encode(rgba);
    let mut out = Vec::new();
    let chunks: Vec<&[u8]> = b64.as_bytes().chunks(4096).collect();

    if chunks.len() <= 1 {
        // Fits in one escape
        out.extend_from_slice(
            format!(
                "\x1b_Ga=T,t=d,f=32,s={},v={},i={};",
                width, height, image_id
            )
            .as_bytes(),
        );
        out.extend_from_slice(chunks.first().copied().unwrap_or(b""));
        out.extend_from_slice(b"\x1b\\");
    } else {
        // First chunk: m=1 (more data coming)
        out.extend_from_slice(
            format!(
                "\x1b_Ga=T,t=d,f=32,s={},v={},i={},m=1;",
                width, height, image_id
            )
            .as_bytes(),
        );
        out.extend_from_slice(chunks[0]);
        out.extend_from_slice(b"\x1b\\");

        // Middle chunks
        for chunk in &chunks[1..chunks.len() - 1] {
            out.extend_from_slice(b"\x1b_Gm=1;");
            out.extend_from_slice(chunk);
            out.extend_from_slice(b"\x1b\\");
        }

        // Last chunk: m=0
        out.extend_from_slice(b"\x1b_Gm=0;");
        out.extend_from_slice(chunks[chunks.len() - 1]);
        out.extend_from_slice(b"\x1b\\");
    }

    out
}

/// Generate a test pattern: 4 colored quadrants (red, green, blue, yellow).
/// Already in RGBA format — no conversion needed.
pub fn test_pattern(width: u32, height: u32) -> Vec<u8> {
    let mut pixels = Vec::with_capacity((width * height * 4) as usize);
    for y in 0..height {
        for x in 0..width {
            let (r, g, b) = if x < width / 2 && y < height / 2 {
                (255u8, 0u8, 0u8) // top-left: red
            } else if x >= width / 2 && y < height / 2 {
                (0, 255, 0) // top-right: green
            } else if x < width / 2 {
                (0, 0, 255) // bottom-left: blue
            } else {
                (255, 255, 0) // bottom-right: yellow
            };
            pixels.extend_from_slice(&[r, g, b, 255]);
        }
    }
    pixels
}

/// Write RGBA pixels to a tmpfs file and return a kitty graphics escape
/// sequence that tells kitty to read from the file.
pub fn kitty_display_file(
    rgba: &[u8],
    width: u32,
    height: u32,
    image_id: u32,
) -> std::io::Result<Vec<u8>> {
    let path = "/dev/shm/dirtferret-display";

    let mut f = std::fs::File::create(path)?;
    f.write_all(rgba)?;
    f.flush()?;

    let path_b64 = base64::engine::general_purpose::STANDARD.encode(path.as_bytes());
    let escape = format!(
        "\x1b_Ga=T,t=f,f=32,s={},v={},i={},C=1,q=2;{}\x1b\\",
        width, height, image_id, path_b64
    );
    Ok(escape.into_bytes())
}
