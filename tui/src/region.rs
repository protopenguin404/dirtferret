use base64::Engine as _;
use std::io::{Seek, SeekFrom, Write};

/// Anchor determines how a region positions itself within the terminal.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Anchor {
    /// Full width, fixed height, anchored to the top edge.
    Top { height_cells: u16 },
    /// Full width, fixed height, anchored to the bottom edge.
    Bottom { height_cells: u16 },
    /// Full width, fills remaining vertical space after Top/Bottom regions.
    Fill,
}

/// A composable rectangle on screen with its own pixel buffer and kitty image.
///
/// Each Region renders independently via the kitty graphics protocol. The
/// terminal composites multiple kitty images natively, so updating one region
/// (e.g. the viewport at 60fps) never re-sends another (e.g. the tab bar).
pub struct Region {
    pub image_id: u32,
    pub anchor: Anchor,
    rgba_buf: Vec<u8>,
    display_file: std::fs::File,
    display_path: String,
    path_b64: String,
    /// 1-indexed terminal row (set by layout engine).
    pub cell_row: u16,
    /// Pixel offset from top of terminal (set by layout engine).
    pub pixel_y: u32,
    /// Width in pixels (set by layout engine).
    pub pixel_width: u32,
    /// Height in pixels (set by layout engine).
    pub pixel_height: u32,
    dirty: bool,
}

impl Region {
    /// Create a new region with the given kitty image ID and anchor rule.
    ///
    /// Creates a display file at `/dev/shm/dirtferret-region-{image_id}`.
    /// The region starts with zero-size buffer; the layout engine sets
    /// real dimensions via `resize()`.
    pub fn new(image_id: u32, anchor: Anchor) -> anyhow::Result<Self> {
        let display_path = format!("/dev/shm/dirtferret-region-{}", image_id);
        let display_file = std::fs::File::create(&display_path)?;
        let path_b64 = base64::engine::general_purpose::STANDARD
            .encode(display_path.as_bytes());

        Ok(Region {
            image_id,
            anchor,
            rgba_buf: Vec::new(),
            display_file,
            display_path,
            path_b64,
            cell_row: 1,
            pixel_y: 0,
            pixel_width: 0,
            pixel_height: 0,
            dirty: false,
        })
    }

    /// Resize the pixel buffer and display file to the given dimensions.
    ///
    /// Only reallocates if dimensions actually changed. Marks the region
    /// dirty so the next `render()` call re-sends the kitty image.
    pub fn resize(&mut self, pixel_width: u32, pixel_height: u32) -> anyhow::Result<()> {
        if self.pixel_width == pixel_width && self.pixel_height == pixel_height {
            return Ok(());
        }
        self.pixel_width = pixel_width;
        self.pixel_height = pixel_height;

        let size = (pixel_width as usize) * (pixel_height as usize) * 4;
        self.rgba_buf.resize(size, 0);

        self.display_file = std::fs::File::create(&self.display_path)?;
        self.display_file.set_len(size as u64)?;

        self.dirty = true;
        Ok(())
    }

    /// Read-only access to the RGBA pixel buffer.
    pub fn rgba_buf(&self) -> &[u8] {
        &self.rgba_buf
    }

    /// Mutable access to the RGBA pixel buffer.
    ///
    /// The compositor passes this to the viewport's `poll_and_convert()`
    /// for zero-copy frame delivery. Call `mark_dirty()` after writing.
    pub fn rgba_mut(&mut self) -> &mut [u8] {
        &mut self.rgba_buf
    }

    /// Mark this region as needing re-render.
    pub fn mark_dirty(&mut self) {
        self.dirty = true;
    }

    /// Check whether this region needs re-rendering.
    pub fn is_dirty(&self) -> bool {
        self.dirty
    }

    /// Fill the entire buffer with a single RGBA color and mark dirty.
    ///
    /// Used for chrome regions (tab bar background, status bar background).
    pub fn fill_solid(&mut self, color: [u8; 4]) {
        for chunk in self.rgba_buf.chunks_exact_mut(4) {
            chunk.copy_from_slice(&color);
        }
        self.dirty = true;
    }

    /// Write the pixel buffer to the display file and send the kitty image
    /// escape sequence to stdout. Only acts if the region is dirty.
    ///
    /// The kitty escape positions the image at `cell_row` column 1 and
    /// transmits the file-backed RGBA data.
    pub fn render(&mut self, stdout: &mut impl Write) -> anyhow::Result<()> {
        if !self.dirty {
            return Ok(());
        }
        if self.pixel_width == 0 || self.pixel_height == 0 {
            return Ok(());
        }

        self.display_file.seek(SeekFrom::Start(0))?;
        self.display_file.write_all(&self.rgba_buf)?;

        write!(
            stdout,
            "\x1b[{};1H\x1b_Ga=T,t=f,f=32,s={},v={},i={},C=1,q=2;{}\x1b\\",
            self.cell_row, self.pixel_width, self.pixel_height, self.image_id, self.path_b64
        )?;

        self.dirty = false;
        Ok(())
    }

    /// Render the pixel background, then overlay ANSI text on top.
    ///
    /// Used for chrome regions that combine a solid-color kitty image with
    /// terminal-font text (tab titles, status line, etc.).
    /// Only writes output when the region is dirty — skips both the pixel
    /// image and the text overlay when clean.
    pub fn render_with_text(&mut self, stdout: &mut impl Write, ansi_text: &str) -> anyhow::Result<()> {
        if !self.dirty {
            return Ok(());
        }
        self.render(stdout)?;
        stdout.write_all(ansi_text.as_bytes())?;
        Ok(())
    }
}

impl Drop for Region {
    fn drop(&mut self) {
        // Delete the kitty image from the terminal. Write to stderr because
        // stdout may already be gone during shutdown.
        let delete_cmd = format!(
            "\x1b_Ga=d,d=i,i={}\x1b\\",
            self.image_id
        );
        let _ = std::io::stderr().write_all(delete_cmd.as_bytes());
        let _ = std::fs::remove_file(&self.display_path);
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_fill_solid() {
        let mut region = Region::new(100, Anchor::Top { height_cells: 1 }).unwrap();
        region.resize(2, 2).unwrap();

        let red = [255, 0, 0, 255];
        region.fill_solid(red);

        let buf = region.rgba_buf();
        assert_eq!(buf.len(), 16); // 2 * 2 * 4
        for chunk in buf.chunks_exact(4) {
            assert_eq!(chunk, &red);
        }
    }

    #[test]
    fn test_dirty_flag() {
        let mut region = Region::new(101, Anchor::Fill).unwrap();
        // New region is not dirty
        assert!(!region.is_dirty());

        region.resize(2, 2).unwrap();
        // resize marks dirty
        assert!(region.is_dirty());

        // Clear dirty via render
        let mut out = Vec::new();
        region.render(&mut out).unwrap();
        assert!(!region.is_dirty());

        // fill_solid marks dirty again
        region.fill_solid([0, 0, 0, 255]);
        assert!(region.is_dirty());

        // render clears dirty
        let mut out2 = Vec::new();
        region.render(&mut out2).unwrap();
        assert!(!region.is_dirty());
    }
}
