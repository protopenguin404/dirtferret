use std::io::Write;

use crate::layout;
use crate::region::{Anchor, Region};
use crate::ui::Ui;
use crate::viewport::Viewport;

/// Owns the three screen regions and orchestrates rendering.
///
/// The compositor creates tab bar, viewport, and status bar regions with
/// deterministic kitty image IDs (1, 2, 3). It drives the layout engine
/// to position them and delegates rendering to each Region independently.
pub struct Compositor {
    pub tab_bar: Region,
    pub viewport_region: Region,
    pub status_bar: Region,
    pixel_width: u32,
    pixel_height: u32,
    cell_cols: u16,
    cell_rows: u16,
}

impl Compositor {
    /// Create a new compositor with three regions laid out for the given
    /// terminal dimensions.
    ///
    /// Region IDs are deterministic: tab_bar=1, viewport=2, status_bar=3.
    pub fn new(
        pixel_width: u32,
        pixel_height: u32,
        cell_cols: u16,
        cell_rows: u16,
    ) -> anyhow::Result<Self> {
        let mut tab_bar = Region::new(1, Anchor::Top { height_cells: 1 })?;
        let mut viewport_region = Region::new(2, Anchor::Fill)?;
        let mut status_bar = Region::new(3, Anchor::Bottom { height_cells: 1 })?;

        // Run initial layout to set positions and sizes.
        {
            let regions: &mut [&mut Region] = &mut [
                &mut tab_bar,
                &mut viewport_region,
                &mut status_bar,
            ];
            layout::compute_layout(regions, pixel_width, pixel_height, cell_cols, cell_rows)?;
        }

        Ok(Compositor {
            tab_bar,
            viewport_region,
            status_bar,
            pixel_width,
            pixel_height,
            cell_cols,
            cell_rows,
        })
    }

    /// Run one render tick: poll the viewport for a new frame, then render
    /// all dirty regions to stdout.
    ///
    /// Returns `true` if any region actually wrote output this tick.
    pub fn render_tick(
        &mut self,
        viewport: &mut Viewport,
        ui: &Ui,
        stdout: &mut impl Write,
    ) -> anyhow::Result<bool> {
        // The viewport region stays at its layout-computed size. If CEF sends
        // frames larger than the region (e.g., full terminal height before
        // processing a viewport-only resize), convert_rect_bgra_to_rgba's
        // bounds check (viewport.rs:41) safely clips the extra rows. This
        // prevents the viewport's kitty image from extending into chrome rows.

        // Zero-copy: viewport writes BGRA->RGBA directly into the region buffer.
        let target = self.viewport_region.rgba_mut();
        let has_frame = viewport.poll_and_convert(target)?;
        if has_frame {
            self.viewport_region.mark_dirty();
        }

        // Render order: viewport FIRST, then chrome ON TOP.
        //
        // Kitty images are cell-based — the most recently placed image at a
        // cell position wins. The viewport re-renders every frame; if chrome
        // rendered before it, the viewport image would cover chrome regions.
        // By rendering chrome AFTER viewport, chrome is always the top layer.
        //
        // When the viewport renders a new frame, we force chrome dirty so it
        // re-renders on the same tick. Cost is minimal (1-row chrome regions
        // are a few KB vs the viewport's megabytes).
        let mut any_rendered = false;

        if self.viewport_region.is_dirty() {
            any_rendered = true;
        }
        self.viewport_region.render(stdout)?;

        // Force chrome re-render whenever viewport rendered, so chrome
        // stays on top of the viewport's kitty image.
        if has_frame {
            self.tab_bar.mark_dirty();
            self.status_bar.mark_dirty();
        }

        if self.tab_bar.is_dirty() {
            any_rendered = true;
        }
        self.tab_bar
            .render_with_text(stdout, &ui.tab_bar_ansi(self.cell_cols))?;

        if self.status_bar.is_dirty() {
            any_rendered = true;
        }
        self.status_bar
            .render_with_text(stdout, &ui.status_line_ansi(self.cell_cols, self.cell_rows))?;

        if any_rendered {
            stdout.flush()?;
        }

        Ok(any_rendered)
    }

    /// Mark chrome regions (tab bar, status bar) as dirty by filling them
    /// with their background colors. They will re-render on the next tick.
    ///
    /// Call this when mode changes, URL updates, tab switches, etc.
    pub fn invalidate_chrome(&mut self, ui: &Ui) {
        self.tab_bar.fill_solid(ui.tab_bar_bg);
        self.status_bar.fill_solid(ui.status_bar_bg);
    }

    /// Recompute layout for new terminal dimensions and mark all regions dirty.
    pub fn resize(
        &mut self,
        pixel_width: u32,
        pixel_height: u32,
        cell_cols: u16,
        cell_rows: u16,
    ) -> anyhow::Result<()> {
        self.pixel_width = pixel_width;
        self.pixel_height = pixel_height;
        self.cell_cols = cell_cols;
        self.cell_rows = cell_rows;

        {
            let regions: &mut [&mut Region] = &mut [
                &mut self.tab_bar,
                &mut self.viewport_region,
                &mut self.status_bar,
            ];
            layout::compute_layout(regions, pixel_width, pixel_height, cell_cols, cell_rows)?;
        }

        // Mark viewport dirty so it re-sends at new position/size.
        // Chrome regions get filled by the caller via invalidate_chrome().
        self.viewport_region.mark_dirty();

        Ok(())
    }

    /// Returns (pixel_width, pixel_height) of the viewport region.
    ///
    /// Used by main.rs to pass correct dimensions to the core resize RPC.
    pub fn viewport_dims(&self) -> (u32, u32) {
        (self.viewport_region.pixel_width, self.viewport_region.pixel_height)
    }

    /// Delete all kitty images explicitly for graceful shutdown.
    ///
    /// This supplements the Drop impls on each Region (which write to stderr
    /// as a last resort).
    pub fn cleanup(&self, stdout: &mut impl Write) -> anyhow::Result<()> {
        for id in [self.tab_bar.image_id, self.viewport_region.image_id, self.status_bar.image_id] {
            write!(stdout, "\x1b_Ga=d,d=i,i={}\x1b\\", id)?;
        }
        stdout.flush()?;
        Ok(())
    }
}
