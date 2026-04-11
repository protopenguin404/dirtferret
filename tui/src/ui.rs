// tui/src/ui.rs
use ratatui::prelude::*;
use ratatui::widgets::{Paragraph, Tabs};
use ratatui::style::{Color, Modifier, Style};

use crate::mux::Rect as PixelRect;

/// One segment of the status line.
#[derive(Clone)]
pub struct Segment {
    pub name: String,
    pub text: String,
    pub style: Style,
}

/// The visual frame: tab bar + status line.
/// Computes the viewport rect from total terminal area.
pub struct Ui {
    status_left: Vec<Segment>,
    status_center: Vec<Segment>,
    status_right: Vec<Segment>,
    tab_titles: Vec<String>,
    active_tab: usize,
}

impl Ui {
    pub fn new() -> Self {
        let status_left = vec![Segment {
            name: "mode".into(),
            text: "NORMAL".into(),
            style: Style::default().fg(Color::Black).bg(Color::Green).add_modifier(Modifier::BOLD),
        }];
        let status_center = vec![Segment {
            name: "url".into(),
            text: String::new(),
            style: Style::default().fg(Color::White),
        }];
        let status_right = vec![Segment {
            name: "fps".into(),
            text: String::new(),
            style: Style::default().fg(Color::DarkGray),
        }];

        Ui {
            status_left,
            status_center,
            status_right,
            tab_titles: Vec::new(),
            active_tab: 0,
        }
    }

    /// Compute the pixel rect available for viewports after chrome takes its rows.
    /// Tab bar = 1 row at top, status line = 1 row at bottom.
    pub fn viewport_rect(&self, total_cols: u16, total_rows: u16, pixel_w: u32, pixel_h: u32) -> PixelRect {
        let chrome_rows: u16 = 2; // 1 tab bar + 1 status line
        let content_rows = total_rows.saturating_sub(chrome_rows);
        let row_h = if total_rows > 0 { pixel_h / total_rows as u32 } else { 16 };
        PixelRect {
            x: 0,
            y: row_h, // skip tab bar row
            width: pixel_w,
            height: content_rows as u32 * row_h,
        }
    }

    /// Update the mode segment.
    pub fn set_mode(&mut self, mode: &str) {
        if let Some(seg) = self.status_left.iter_mut().find(|s| s.name == "mode") {
            seg.text = mode.to_uppercase();
            seg.style = match mode {
                "normal" => Style::default().fg(Color::Black).bg(Color::Green).add_modifier(Modifier::BOLD),
                "passthrough" => Style::default().fg(Color::Black).bg(Color::Yellow).add_modifier(Modifier::BOLD),
                _ => Style::default().fg(Color::Black).bg(Color::Blue).add_modifier(Modifier::BOLD),
            };
        }
    }

    /// Update the URL segment.
    pub fn set_url(&mut self, url: &str) {
        if let Some(seg) = self.status_center.iter_mut().find(|s| s.name == "url") {
            seg.text = url.to_string();
        }
    }

    /// Update the FPS segment.
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

    /// Render the tab bar and status line using ratatui.
    pub fn render(&self, frame: &mut ratatui::Frame) {
        let area = frame.area();
        if area.height < 3 { return; }

        // Tab bar: top row
        let tab_area = ratatui::layout::Rect {
            x: area.x, y: area.y, width: area.width, height: 1,
        };
        let titles: Vec<Line> = self.tab_titles.iter().enumerate().map(|(i, t)| {
            let style = if i == self.active_tab {
                Style::default().fg(Color::White).bg(Color::DarkGray).add_modifier(Modifier::BOLD)
            } else {
                Style::default().fg(Color::Gray)
            };
            Line::from(Span::styled(format!(" {} ", t), style))
        }).collect();

        if !titles.is_empty() {
            let tabs = Tabs::new(titles)
                .select(self.active_tab)
                .style(Style::default().bg(Color::Black))
                .divider(Span::raw("|"));
            frame.render_widget(tabs, tab_area);
        }

        // Status line: bottom row
        let status_area = ratatui::layout::Rect {
            x: area.x, y: area.y + area.height - 1, width: area.width, height: 1,
        };

        // Build spans
        let left_span = self.status_left.first().map(|s| {
            Span::styled(format!(" {} ", s.text), s.style)
        }).unwrap_or_default();

        let center_text: String = self.status_center.iter().map(|s| s.text.clone()).collect();
        let center_span = Span::styled(
            format!(" {} ", center_text),
            self.status_center.first().map(|s| s.style).unwrap_or_default(),
        );

        let right_text: String = self.status_right.iter().map(|s| format!(" {} ", s.text)).collect();
        let right_span = Span::styled(
            right_text.clone(),
            self.status_right.first().map(|s| s.style).unwrap_or_default(),
        );

        // Pad to fill width
        let left_len = self.status_left.first().map(|s| s.text.len() + 2).unwrap_or(0);
        let center_len = center_text.len() + 2;
        let right_len = right_text.len();
        let used = left_len + center_len + right_len;
        let padding = (status_area.width as usize).saturating_sub(used);
        let left_pad = padding / 2;
        let right_pad = padding - left_pad;

        let line = Line::from(vec![
            left_span,
            Span::raw(" ".repeat(left_pad)),
            center_span,
            Span::raw(" ".repeat(right_pad)),
            right_span,
        ]);

        let status = Paragraph::new(line)
            .style(Style::default().bg(Color::DarkGray));
        frame.render_widget(status, status_area);

        // Mark viewport rows as skip so ratatui doesn't overwrite kitty graphics
        let viewport_start = area.y + 1;
        let viewport_end = area.y + area.height - 1;
        for row in viewport_start..viewport_end {
            for col in area.x..area.x + area.width {
                if let Some(cell) = frame.buffer_mut().cell_mut(ratatui::layout::Position { x: col, y: row }) {
                    cell.set_skip(true);
                }
            }
        }
    }
}
