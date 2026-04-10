use base64::Engine as _;
use std::io::Write;

/// Convert BGRA pixel buffer to RGBA in-place.
pub fn bgra_to_rgba(pixels: &mut [u8]) {
    for chunk in pixels.chunks_exact_mut(4) {
        chunk.swap(0, 2);
    }
}

/// Write RGBA pixels to a tmpfs file and return a kitty graphics escape
/// sequence that tells kitty to read from the file (~100 bytes instead of
/// ~17MB of inline base64).
pub fn kitty_display_file(
    rgba: &[u8],
    width: u32,
    height: u32,
    image_id: u32,
) -> std::io::Result<Vec<u8>> {
    let path = "/dev/shm/dirtferret-display";

    // Write raw RGBA pixel data to tmpfs (RAM-backed, fast)
    let mut f = std::fs::File::create(path)?;
    f.write_all(rgba)?;

    // Kitty protocol: t=f means read pixel data from file.
    // Payload is the base64-encoded file path.
    // q=2 suppresses kitty response codes.
    let path_b64 = base64::engine::general_purpose::STANDARD.encode(path.as_bytes());
    let escape = format!(
        "\x1b_Ga=T,t=f,f=32,s={},v={},i={},C=1,q=2;{}\x1b\\",
        width, height, image_id, path_b64
    );
    Ok(escape.into_bytes())
}
