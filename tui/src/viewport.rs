use base64::Engine as _;

pub fn bgra_to_rgba(pixels: &mut [u8]) {
    for chunk in pixels.chunks_exact_mut(4) {
        chunk.swap(0, 2);
    }
}

pub fn kitty_display(rgba: &[u8], width: u32, height: u32, image_id: u32) -> Vec<u8> {
    let b64 = base64::engine::general_purpose::STANDARD.encode(rgba);
    let mut out = Vec::with_capacity(b64.len() + 256);

    let chunks: Vec<&[u8]> = b64.as_bytes().chunks(4096).collect();

    for (i, chunk) in chunks.iter().enumerate() {
        let more = if i < chunks.len() - 1 { 1 } else { 0 };
        if i == 0 {
            // Header ends with ';' before the base64 payload
            let header = format!(
                "\x1b_Ga=T,f=32,s={},v={},i={},C=1,m={};",
                width, height, image_id, more
            );
            out.extend_from_slice(header.as_bytes());
        } else {
            let header = format!("\x1b_Gm={};", more);
            out.extend_from_slice(header.as_bytes());
        }
        out.extend_from_slice(chunk);
        out.extend_from_slice(b"\x1b\\");
    }

    out
}
