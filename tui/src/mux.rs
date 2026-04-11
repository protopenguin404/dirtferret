// tui/src/mux.rs

/// A pixel rectangle: x, y, width, height.
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct Rect {
    pub x: u32,
    pub y: u32,
    pub width: u32,
    pub height: u32,
}

/// One tab — currently maps 1:1 to a buffer.
/// Future: could hold a LayoutTree for split viewports within a tab.
pub struct Tab {
    pub buffer_id: i32,
    pub title: String,
}

/// Multiplexer: manages tabs and computes viewport layout rects.
/// For now: flat list, one viewport per tab.
/// Later: each tab holds a layout tree for splits.
pub struct Mux {
    tabs: Vec<Tab>,
    active: usize,
}

impl Mux {
    pub fn new() -> Self {
        Mux { tabs: Vec::new(), active: 0 }
    }

    pub fn add_tab(&mut self, buffer_id: i32, title: String) -> usize {
        let idx = self.tabs.len();
        self.tabs.push(Tab { buffer_id, title });
        idx
    }

    pub fn close_tab(&mut self, index: usize) {
        if index >= self.tabs.len() { return; }
        self.tabs.remove(index);
        if self.active >= self.tabs.len() && self.active > 0 {
            self.active = self.tabs.len() - 1;
        }
    }

    pub fn active_tab(&self) -> Option<&Tab> {
        self.tabs.get(self.active)
    }

    pub fn active_buffer_id(&self) -> Option<i32> {
        self.active_tab().map(|t| t.buffer_id)
    }

    pub fn set_active(&mut self, index: usize) {
        if index < self.tabs.len() {
            self.active = index;
        }
    }

    pub fn active_index(&self) -> usize {
        self.active
    }

    pub fn next_tab(&mut self) {
        if !self.tabs.is_empty() {
            self.active = (self.active + 1) % self.tabs.len();
        }
    }

    pub fn prev_tab(&mut self) {
        if !self.tabs.is_empty() {
            self.active = if self.active == 0 { self.tabs.len() - 1 } else { self.active - 1 };
        }
    }

    pub fn tab_count(&self) -> usize {
        self.tabs.len()
    }

    pub fn tabs(&self) -> &[Tab] {
        &self.tabs
    }

    pub fn update_title(&mut self, buffer_id: i32, title: &str) {
        for tab in &mut self.tabs {
            if tab.buffer_id == buffer_id {
                tab.title = title.to_string();
            }
        }
    }

    /// Compute viewport rects for the active tab's layout.
    /// Returns (buffer_id, rect) pairs.
    /// For now: one viewport gets the full available area.
    /// Future: walk a LayoutTree and return one rect per leaf.
    pub fn layout(&self, available: Rect) -> Vec<(i32, Rect)> {
        match self.active_tab() {
            Some(tab) => vec![(tab.buffer_id, available)],
            None => vec![],
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_add_and_get() {
        let mut mux = Mux::new();
        mux.add_tab(1, "Tab 1".into());
        mux.add_tab(2, "Tab 2".into());
        assert_eq!(mux.tab_count(), 2);
        assert_eq!(mux.active_buffer_id(), Some(1));
    }

    #[test]
    fn test_switch_tabs() {
        let mut mux = Mux::new();
        mux.add_tab(1, "A".into());
        mux.add_tab(2, "B".into());
        mux.add_tab(3, "C".into());

        mux.next_tab();
        assert_eq!(mux.active_buffer_id(), Some(2));
        mux.next_tab();
        assert_eq!(mux.active_buffer_id(), Some(3));
        mux.next_tab(); // wraps
        assert_eq!(mux.active_buffer_id(), Some(1));

        mux.prev_tab(); // wraps back
        assert_eq!(mux.active_buffer_id(), Some(3));
    }

    #[test]
    fn test_close_tab() {
        let mut mux = Mux::new();
        mux.add_tab(1, "A".into());
        mux.add_tab(2, "B".into());
        mux.set_active(1);
        assert_eq!(mux.active_buffer_id(), Some(2));

        mux.close_tab(1);
        assert_eq!(mux.tab_count(), 1);
        assert_eq!(mux.active_buffer_id(), Some(1)); // clamped
    }

    #[test]
    fn test_layout_single() {
        let mut mux = Mux::new();
        mux.add_tab(1, "A".into());
        let rects = mux.layout(Rect { x: 0, y: 16, width: 1920, height: 1048 });
        assert_eq!(rects.len(), 1);
        assert_eq!(rects[0].0, 1);
        assert_eq!(rects[0].1.width, 1920);
    }

    #[test]
    fn test_empty_mux() {
        let mux = Mux::new();
        assert_eq!(mux.active_buffer_id(), None);
        assert!(mux.layout(Rect { x: 0, y: 0, width: 100, height: 100 }).is_empty());
    }

    #[test]
    fn test_update_title() {
        let mut mux = Mux::new();
        mux.add_tab(1, "Loading...".into());
        mux.update_title(1, "Google");
        assert_eq!(mux.tabs()[0].title, "Google");
    }
}
