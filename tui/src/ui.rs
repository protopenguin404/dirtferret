// tui/src/ui.rs
//
// Chrome text builders for the region-based compositing model.
// No ratatui — ANSI escape sequences only.
//
// Chrome rendering is two layers:
//   1. Solid-color pixel fill (kitty image) — background color
//   2. ANSI text overlay (terminal font) — sharp text at any size
//
// This module produces the ANSI text layer. The pixel fill is handled
// by the Region/Compositor.

/// One segment of the status line.
#[derive(Clone)]
pub struct Segment {
    pub name: String,
    pub text: String,
    /// Foreground color as (R, G, B).
    pub fg: (u8, u8, u8),
    /// Optional background color as (R, G, B).
    /// When None, the kitty image pixel fill shows through.
    pub bg: Option<(u8, u8, u8)>,
    pub bold: bool,
}

/// The visual chrome state: tab bar + status line.
/// Produces ANSI escape strings for text overlay on kitty pixel regions.
pub struct Ui {
    status_left: Vec<Segment>,
    status_center: Vec<Segment>,
    status_right: Vec<Segment>,
    tab_titles: Vec<String>,
    active_tab: usize,
    /// RGBA fill color for tab bar region pixels.
    pub tab_bar_bg: [u8; 4],
    /// RGBA fill color for status bar region pixels.
    pub status_bar_bg: [u8; 4],
}

impl Ui {
    pub fn new() -> Self {
        let status_left = vec![Segment {
            name: "mode".into(),
            text: "NORMAL".into(),
            fg: (0, 0, 0),
            bg: Some((0, 200, 0)),
            bold: true,
        }];
        let status_center = vec![Segment {
            name: "url".into(),
            text: String::new(),
            fg: (200, 200, 200),
            bg: None,
            bold: false,
        }];
        let status_right = vec![Segment {
            name: "fps".into(),
            text: String::new(),
            fg: (100, 100, 100),
            bg: None,
            bold: false,
        }];

        Ui {
            status_left,
            status_center,
            status_right,
            tab_titles: Vec::new(),
            active_tab: 0,
            tab_bar_bg: [40, 40, 40, 255],
            status_bar_bg: [50, 50, 50, 255],
        }
    }

    /// Update the mode segment text and colors.
    pub fn set_mode(&mut self, mode: &str) {
        if let Some(seg) = self.status_left.iter_mut().find(|s| s.name == "mode") {
            seg.text = mode.to_uppercase();
            match mode {
                "normal" => {
                    seg.fg = (0, 0, 0);
                    seg.bg = Some((0, 200, 0));
                    seg.bold = true;
                }
                "passthrough" => {
                    seg.fg = (0, 0, 0);
                    seg.bg = Some((200, 200, 0));
                    seg.bold = true;
                }
                _ => {
                    seg.fg = (0, 0, 0);
                    seg.bg = Some((0, 100, 200));
                    seg.bold = true;
                }
            }
        }
    }

    /// Update the URL segment text.
    pub fn set_url(&mut self, url: &str) {
        if let Some(seg) = self.status_center.iter_mut().find(|s| s.name == "url") {
            seg.text = url.to_string();
        }
    }

    /// Update the FPS segment text.
    pub fn set_fps(&mut self, fps: usize) {
        if let Some(seg) = self.status_right.iter_mut().find(|s| s.name == "fps") {
            seg.text = format!("{}fps", fps);
        }
    }

    /// Update tab list from mux state.
    pub fn set_tabs(&mut self, titles: Vec<String>, active: usize) {
        self.tab_titles = titles;
        self.active_tab = active;
    }

    /// Build ANSI escape string for the tab bar text overlay.
    ///
    /// Positions cursor at row 1, disables autowrap, renders tab titles
    /// separated by `|`, truncates to `cols` width.
    pub fn tab_bar_ansi(&self, cols: u16) -> String {
        let max_w = cols as usize;
        let mut out = String::with_capacity(256);

        // Cursor to row 1, col 1; disable autowrap; fill row with bg color
        let [r, g, b, _] = self.tab_bar_bg;
        out.push_str(&format!(
            "\x1b[1;1H\x1b[?7l\x1b[48;2;{};{};{}m\x1b[2K", r, g, b
        ));

        if self.tab_titles.is_empty() {
            // Reset and re-enable autowrap
            out.push_str("\x1b[0m\x1b[?7h");
            return out;
        }

        let mut width_used: usize = 0;
        let mut truncated = false;

        for (i, title) in self.tab_titles.iter().enumerate() {
            // Separator before all tabs except the first
            if i > 0 {
                // Check if separator fits
                if width_used + 1 > max_w {
                    truncated = true;
                    break;
                }
                // Gray separator
                out.push_str("\x1b[38;2;80;80;80m|");
                width_used += 1;
            }

            // Format tab text with padding: " title "
            let padded = format!(" {} ", title);
            let tab_len = padded.len();

            // Check if this tab fits
            if width_used + tab_len > max_w {
                // Try to fit a truncated version
                let remaining = max_w.saturating_sub(width_used);
                if remaining > 3 {
                    // Apply style for this tab
                    if i == self.active_tab {
                        out.push_str("\x1b[38;2;255;255;255m\x1b[1m");
                    } else {
                        out.push_str("\x1b[38;2;150;150;150m");
                    }
                    // Truncate with ellipsis
                    let truncated_text: String = padded.chars().take(remaining - 3).collect();
                    out.push_str(&truncated_text);
                    out.push_str("...");
                }
                truncated = true;
                break;
            }

            // Apply style
            if i == self.active_tab {
                out.push_str("\x1b[38;2;255;255;255m\x1b[1m");
            } else {
                out.push_str("\x1b[38;2;150;150;150m");
            }

            out.push_str(&padded);
            width_used += tab_len;
        }

        // If we truncated and there's room for an indicator, it's already handled above
        let _ = truncated;

        // Reset attributes, re-enable autowrap
        out.push_str("\x1b[0m\x1b[?7h");

        out
    }

    /// Build ANSI escape string for the status line text overlay.
    ///
    /// Positions cursor at the given row, disables autowrap, renders
    /// left/center/right segment groups with padding, truncates to `cols`.
    pub fn status_line_ansi(&self, cols: u16, rows: u16) -> String {
        let max_w = cols as usize;
        let mut out = String::with_capacity(512);

        // Cursor to last row, col 1; disable autowrap; fill row with bg color
        let [r, g, b, _] = self.status_bar_bg;
        out.push_str(&format!(
            "\x1b[{};1H\x1b[?7l\x1b[48;2;{};{};{}m\x1b[2K", rows, r, g, b
        ));

        // Pre-render each group into (ansi_string, display_width) pairs
        let left = Self::render_segments(&self.status_left);
        let center = Self::render_segments(&self.status_center);
        let right = Self::render_segments(&self.status_right);

        let left_w = left.1;
        let center_w = center.1;
        let right_w = right.1;
        let total_content = left_w + center_w + right_w;

        if total_content >= max_w {
            // Everything doesn't fit — truncate
            let mut budget = max_w;

            // Left segments always get priority
            if left_w <= budget {
                out.push_str(&left.0);
                budget -= left_w;
            } else {
                out.push_str(&Self::render_segments_truncated(&self.status_left, budget));
                budget = 0;
            }

            // Then center if room
            if budget > 0 && center_w > 0 {
                out.push(' ');
                budget = budget.saturating_sub(1);
                if center_w <= budget {
                    out.push_str(&center.0);
                    budget -= center_w;
                } else if budget > 3 {
                    out.push_str(&Self::render_segments_truncated(&self.status_center, budget));
                    budget = 0;
                } else {
                    budget = 0;
                }
            }

            // Right if somehow room remains
            if budget > 0 && right_w > 0 {
                out.push(' ');
                budget = budget.saturating_sub(1);
                if right_w <= budget {
                    out.push_str(&right.0);
                } else if budget > 3 {
                    out.push_str(&Self::render_segments_truncated(&self.status_right, budget));
                }
            }
        } else {
            // Everything fits — distribute gaps
            let total_gap = max_w - total_content;
            let left_gap = total_gap / 2;
            let right_gap = total_gap - left_gap;

            out.push_str(&left.0);
            if left_gap > 0 {
                out.push_str(&" ".repeat(left_gap));
            }
            out.push_str(&center.0);
            if right_gap > 0 {
                out.push_str(&" ".repeat(right_gap));
            }
            out.push_str(&right.0);
        }

        // Reset attributes, re-enable autowrap
        out.push_str("\x1b[0m\x1b[?7h");

        out
    }

    /// Render a group of segments into an ANSI string and its display width.
    /// Each segment is rendered as ` text ` with appropriate SGR codes.
    fn render_segments(segments: &[Segment]) -> (String, usize) {
        let mut ansi = String::new();
        let mut width: usize = 0;

        for seg in segments {
            if seg.text.is_empty() {
                continue;
            }

            let padded = format!(" {} ", seg.text);
            let seg_w = padded.len();

            // Build SGR sequence
            let (r, g, b) = seg.fg;
            ansi.push_str(&format!("\x1b[38;2;{};{};{}m", r, g, b));

            if let Some((br, bg, bb)) = seg.bg {
                ansi.push_str(&format!("\x1b[48;2;{};{};{}m", br, bg, bb));
            }

            if seg.bold {
                ansi.push_str("\x1b[1m");
            }

            ansi.push_str(&padded);
            width += seg_w;

            // Reset after each segment so styles don't bleed
            ansi.push_str("\x1b[0m");
        }

        (ansi, width)
    }

    /// Render segments truncated to fit within `budget` display columns.
    /// Adds `...` suffix if truncation occurs.
    fn render_segments_truncated(segments: &[Segment], budget: usize) -> String {
        let mut ansi = String::new();
        let mut remaining = budget;

        for seg in segments {
            if seg.text.is_empty() || remaining == 0 {
                continue;
            }

            let padded = format!(" {} ", seg.text);
            let seg_w = padded.len();

            // Build SGR
            let (r, g, b) = seg.fg;
            ansi.push_str(&format!("\x1b[38;2;{};{};{}m", r, g, b));

            if let Some((br, bg, bb)) = seg.bg {
                ansi.push_str(&format!("\x1b[48;2;{};{};{}m", br, bg, bb));
            }

            if seg.bold {
                ansi.push_str("\x1b[1m");
            }

            if seg_w <= remaining {
                ansi.push_str(&padded);
                remaining -= seg_w;
            } else {
                // Truncate with ellipsis
                if remaining > 3 {
                    let truncated: String = padded.chars().take(remaining - 3).collect();
                    ansi.push_str(&truncated);
                    ansi.push_str("...");
                } else {
                    // Not enough room for ellipsis, just fill what we can
                    let truncated: String = padded.chars().take(remaining).collect();
                    ansi.push_str(&truncated);
                }
                remaining = 0;
            }

            ansi.push_str("\x1b[0m");
        }

        ansi
    }
}
