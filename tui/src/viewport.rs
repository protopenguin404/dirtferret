use base64::Engine as _;
use std::io::Write;

use crate::shm;

/// Cached shm state: both frame buffers mapped, switch by slot.
struct ShmPair {
    reader_0: shm::ShmReader,
    reader_1: shm::ShmReader,
    size: usize,
}

impl ShmPair {
    fn open(buffer_id: i32, width: u32, height: u32) -> anyhow::Result<Self> {
        let size = (width as usize) * (height as usize) * 4;
        let name_0 = format!("/dirtferret-{}-frame-0", buffer_id);
        let name_1 = format!("/dirtferret-{}-frame-1", buffer_id);
        Ok(Self {
            reader_0: shm::ShmReader::open(&name_0, size)?,
            reader_1: shm::ShmReader::open(&name_1, size)?,
            size,
        })
    }

    fn reader_by_slot(&self, slot: u32) -> &shm::ShmReader {
        if slot == 0 { &self.reader_0 } else { &self.reader_1 }
    }
}

/// Convert a sub-rectangle from BGRA to RGBA.
fn convert_rect_bgra_to_rgba(
    src: &[u8], dst: &mut [u8],
    stride: u32, x: u32, y: u32, w: u32, h: u32,
) {
    let stride = stride as usize * 4;
    let x = x as usize;
    let w = w as usize;
    for row in y as usize..(y + h) as usize {
        let offset = row * stride + x * 4;
        let end = offset + w * 4;
        if end > src.len() || end > dst.len() { break; }
        let src_row = &src[offset..end];
        let dst_row = &mut dst[offset..end];
        for chunk in 0..w {
            let i = chunk * 4;
            dst_row[i] = src_row[i + 2];     // R <- B
            dst_row[i + 1] = src_row[i + 1]; // G
            dst_row[i + 2] = src_row[i];     // B <- R
            dst_row[i + 3] = src_row[i + 3]; // A
        }
    }
}

/// Frame data source for one CEF buffer.
/// Polls shm for new frames and converts BGRA→RGBA into an external buffer.
/// Does NOT own display output — that's the Compositor's job via Regions.
pub struct Viewport {
    buffer_id: i32,
    notify: shm::FrameNotify,
    shm_pair: ShmPair,
    frame_width: u32,
    frame_height: u32,
    pending_rects: Vec<[u32; 4]>,
    full_redraw: bool,
}

impl Viewport {
    /// Create a new viewport for a buffer. Opens shm segments.
    /// No display file, no image_id — rendering is the Compositor's job.
    pub fn new(buffer_id: i32, width: u32, height: u32) -> anyhow::Result<Self> {
        let notify_name = format!("/dirtferret-{}-notify", buffer_id);
        let notify = shm::FrameNotify::open(&notify_name)?;
        let shm_pair = ShmPair::open(buffer_id, width, height)?;

        Ok(Viewport {
            buffer_id,
            notify,
            shm_pair,
            frame_width: width,
            frame_height: height,
            pending_rects: Vec::new(),
            full_redraw: true,
        })
    }

    /// Poll for a new frame and convert BGRA→RGBA into the caller's buffer.
    /// Returns true if new pixel data was written to `target`.
    /// The `target` slice is owned by a Region — zero copy into the compositor.
    pub fn poll_and_convert(&mut self, target: &mut [u8]) -> anyhow::Result<bool> {
        let header = self.notify.poll();

        if let Some(ref header) = header {
            let new_size = (header.width as usize) * (header.height as usize) * 4;
            if new_size != self.shm_pair.size {
                self.shm_pair = ShmPair::open(self.buffer_id, header.width, header.height)?;
                self.full_redraw = true;
            }
            self.frame_width = header.width;
            self.frame_height = header.height;
            if !self.full_redraw {
                self.pending_rects.extend_from_slice(&header.dirty_rects);
            }
        }

        if header.is_none() && !self.full_redraw {
            return Ok(false);
        }

        let slot = header.as_ref().map(|h| h.read_slot).unwrap_or(0);
        let reader = self.shm_pair.reader_by_slot(slot);
        let src = reader.as_bytes();

        if self.full_redraw || self.pending_rects.is_empty() {
            convert_rect_bgra_to_rgba(
                src, target,
                self.frame_width, 0, 0, self.frame_width, self.frame_height,
            );
            self.full_redraw = false;
        } else {
            for rect in &self.pending_rects {
                let [x, y, w, h] = *rect;
                convert_rect_bgra_to_rgba(
                    src, target,
                    self.frame_width, x, y, w, h,
                );
            }
        }
        self.pending_rects.clear();

        Ok(true)
    }

    /// Returns (frame_width, frame_height) in pixels.
    pub fn frame_dims(&self) -> (u32, u32) {
        (self.frame_width, self.frame_height)
    }

    /// Force a full redraw on next poll.
    pub fn invalidate(&mut self) {
        self.full_redraw = true;
    }

    pub fn buffer_id(&self) -> i32 {
        self.buffer_id
    }
}

// --- Test utilities (kept from original viewport.rs) ---

/// Generate a test pattern: 4 colored quadrants (red, green, blue, yellow).
pub fn test_pattern(width: u32, height: u32) -> Vec<u8> {
    let mut pixels = Vec::with_capacity((width * height * 4) as usize);
    for y in 0..height {
        for x in 0..width {
            let (r, g, b) = if x < width / 2 && y < height / 2 {
                (255u8, 0u8, 0u8)
            } else if x >= width / 2 && y < height / 2 {
                (0, 255, 0)
            } else if x < width / 2 {
                (0, 0, 255)
            } else {
                (255, 255, 0)
            };
            pixels.extend_from_slice(&[r, g, b, 255]);
        }
    }
    pixels
}

/// Send RGBA pixels directly inline via base64, chunked per kitty spec.
pub fn kitty_display_direct(
    rgba: &[u8], width: u32, height: u32, image_id: u32,
) -> Vec<u8> {
    let b64 = base64::engine::general_purpose::STANDARD.encode(rgba);
    let mut out = Vec::new();
    let chunks: Vec<&[u8]> = b64.as_bytes().chunks(4096).collect();
    if chunks.len() <= 1 {
        out.extend_from_slice(
            format!("\x1b_Ga=T,t=d,f=32,s={},v={},i={};", width, height, image_id).as_bytes(),
        );
        out.extend_from_slice(chunks.first().copied().unwrap_or(b""));
        out.extend_from_slice(b"\x1b\\");
    } else {
        out.extend_from_slice(
            format!("\x1b_Ga=T,t=d,f=32,s={},v={},i={},m=1;", width, height, image_id).as_bytes(),
        );
        out.extend_from_slice(chunks[0]);
        out.extend_from_slice(b"\x1b\\");
        for chunk in &chunks[1..chunks.len() - 1] {
            out.extend_from_slice(b"\x1b_Gm=1;");
            out.extend_from_slice(chunk);
            out.extend_from_slice(b"\x1b\\");
        }
        out.extend_from_slice(b"\x1b_Gm=0;");
        out.extend_from_slice(chunks[chunks.len() - 1]);
        out.extend_from_slice(b"\x1b\\");
    }
    out
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
