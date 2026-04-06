// Buffer state management.
// Tracks all open browser buffers and the active selection.
// This is the TUI's mirror of the core's buffer state, updated via RPC callbacks.

/// State for a single browser buffer (tab).
#[derive(Debug, Clone)]
pub struct Buffer {
    pub id: i32,
    pub url: String,
    pub title: String,
    pub loading: bool,
    pub can_go_back: bool,
    pub can_go_forward: bool,
    pub load_progress: f64,
}

impl Buffer {
    pub fn new(id: i32) -> Self {
        Self {
            id,
            url: String::from("about:blank"),
            title: String::new(),
            loading: false,
            can_go_back: false,
            can_go_forward: false,
            load_progress: 0.0,
        }
    }

    /// Display title: falls back to URL, then buffer ID.
    pub fn display_title(&self) -> String {
        if !self.title.is_empty() {
            self.title.clone()
        } else if !self.url.is_empty() && self.url != "about:blank" {
            self.url.clone()
        } else {
            format!("[Buffer {}]", self.id)
        }
    }
}

/// Manages the set of open buffers and tracks the active one.
pub struct BufferList {
    buffers: Vec<Buffer>,
    active_index: usize,
}

impl BufferList {
    pub fn new() -> Self {
        Self {
            buffers: Vec::new(),
            active_index: 0,
        }
    }

    /// Add a buffer. If it's the first one, it becomes active.
    pub fn add(&mut self, buffer: Buffer) {
        self.buffers.push(buffer);
        if self.buffers.len() == 1 {
            self.active_index = 0;
        }
    }

    /// Remove a buffer by ID. Returns true if found.
    /// If the active buffer is removed, active moves to the previous one.
    pub fn remove(&mut self, id: i32) -> bool {
        if let Some(pos) = self.buffers.iter().position(|b| b.id == id) {
            self.buffers.remove(pos);
            if self.buffers.is_empty() {
                self.active_index = 0;
            } else if self.active_index >= self.buffers.len() {
                self.active_index = self.buffers.len() - 1;
            } else if pos < self.active_index {
                self.active_index -= 1;
            }
            true
        } else {
            false
        }
    }

    /// Set the active buffer by ID. Returns false if not found.
    pub fn set_active(&mut self, id: i32) -> bool {
        if let Some(pos) = self.buffers.iter().position(|b| b.id == id) {
            self.active_index = pos;
            true
        } else {
            false
        }
    }

    /// Cycle to the next buffer. Wraps around.
    pub fn next(&mut self) {
        if !self.buffers.is_empty() {
            self.active_index = (self.active_index + 1) % self.buffers.len();
        }
    }

    /// Cycle to the previous buffer. Wraps around.
    pub fn prev(&mut self) {
        if !self.buffers.is_empty() {
            if self.active_index == 0 {
                self.active_index = self.buffers.len() - 1;
            } else {
                self.active_index -= 1;
            }
        }
    }

    /// Get the active buffer.
    pub fn active(&self) -> Option<&Buffer> {
        self.buffers.get(self.active_index)
    }

    /// Get a mutable reference to the active buffer.
    pub fn active_mut(&mut self) -> Option<&mut Buffer> {
        self.buffers.get_mut(self.active_index)
    }

    /// Get a buffer by ID.
    pub fn get(&self, id: i32) -> Option<&Buffer> {
        self.buffers.iter().find(|b| b.id == id)
    }

    /// Get a mutable buffer by ID.
    pub fn get_mut(&mut self, id: i32) -> Option<&mut Buffer> {
        self.buffers.iter_mut().find(|b| b.id == id)
    }

    /// All buffers, in order.
    pub fn all(&self) -> &[Buffer] {
        &self.buffers
    }

    /// Number of open buffers.
    pub fn len(&self) -> usize {
        self.buffers.len()
    }

    pub fn is_empty(&self) -> bool {
        self.buffers.is_empty()
    }

    /// Index of the active buffer (0-based).
    pub fn active_index(&self) -> usize {
        self.active_index
    }

    /// Active buffer's ID, or None if no buffers.
    pub fn active_id(&self) -> Option<i32> {
        self.active().map(|b| b.id)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn buf(id: i32, title: &str) -> Buffer {
        Buffer {
            id,
            title: title.to_string(),
            ..Buffer::new(id)
        }
    }

    // --- Buffer display ---

    #[test]
    fn display_title_prefers_title() {
        let b = Buffer { title: "Example".into(), url: "https://example.com".into(), ..Buffer::new(1) };
        assert_eq!(b.display_title(), "Example");
    }

    #[test]
    fn display_title_falls_back_to_url() {
        let b = Buffer { url: "https://example.com".into(), ..Buffer::new(1) };
        assert_eq!(b.display_title(), "https://example.com");
    }

    #[test]
    fn display_title_falls_back_to_id() {
        let b = Buffer::new(7);
        assert_eq!(b.display_title(), "[Buffer 7]");
    }

    // --- BufferList basics ---

    #[test]
    fn new_list_is_empty() {
        let list = BufferList::new();
        assert!(list.is_empty());
        assert_eq!(list.len(), 0);
        assert!(list.active().is_none());
    }

    #[test]
    fn add_first_buffer_becomes_active() {
        let mut list = BufferList::new();
        list.add(buf(1, "First"));
        assert_eq!(list.len(), 1);
        assert_eq!(list.active().unwrap().id, 1);
    }

    #[test]
    fn add_second_buffer_doesnt_change_active() {
        let mut list = BufferList::new();
        list.add(buf(1, "First"));
        list.add(buf(2, "Second"));
        assert_eq!(list.active().unwrap().id, 1);
    }

    #[test]
    fn set_active_by_id() {
        let mut list = BufferList::new();
        list.add(buf(1, "A"));
        list.add(buf(2, "B"));
        list.add(buf(3, "C"));

        assert!(list.set_active(3));
        assert_eq!(list.active().unwrap().id, 3);
    }

    #[test]
    fn set_active_nonexistent_returns_false() {
        let mut list = BufferList::new();
        list.add(buf(1, "A"));
        assert!(!list.set_active(99));
        assert_eq!(list.active().unwrap().id, 1); // unchanged
    }

    // --- Cycling ---

    #[test]
    fn next_cycles_forward() {
        let mut list = BufferList::new();
        list.add(buf(1, "A"));
        list.add(buf(2, "B"));
        list.add(buf(3, "C"));

        assert_eq!(list.active().unwrap().id, 1);
        list.next();
        assert_eq!(list.active().unwrap().id, 2);
        list.next();
        assert_eq!(list.active().unwrap().id, 3);
    }

    #[test]
    fn next_wraps_around() {
        let mut list = BufferList::new();
        list.add(buf(1, "A"));
        list.add(buf(2, "B"));

        list.next(); // → B
        list.next(); // → wraps to A
        assert_eq!(list.active().unwrap().id, 1);
    }

    #[test]
    fn prev_cycles_backward() {
        let mut list = BufferList::new();
        list.add(buf(1, "A"));
        list.add(buf(2, "B"));
        list.add(buf(3, "C"));

        list.set_active(3);
        list.prev();
        assert_eq!(list.active().unwrap().id, 2);
    }

    #[test]
    fn prev_wraps_around() {
        let mut list = BufferList::new();
        list.add(buf(1, "A"));
        list.add(buf(2, "B"));

        list.prev(); // at 0, wraps to 1
        assert_eq!(list.active().unwrap().id, 2);
    }

    #[test]
    fn cycling_empty_list_is_safe() {
        let mut list = BufferList::new();
        list.next(); // should not panic
        list.prev(); // should not panic
        assert!(list.active().is_none());
    }

    // --- Removal ---

    #[test]
    fn remove_only_buffer() {
        let mut list = BufferList::new();
        list.add(buf(1, "A"));
        assert!(list.remove(1));
        assert!(list.is_empty());
        assert!(list.active().is_none());
    }

    #[test]
    fn remove_active_moves_to_previous() {
        let mut list = BufferList::new();
        list.add(buf(1, "A"));
        list.add(buf(2, "B"));
        list.add(buf(3, "C"));
        list.set_active(2); // active = B (index 1)

        list.remove(2); // remove B
        // Active should move to A (index 0) since B was at index 1
        // After removal: [A, C], active_index stays at 1 → C
        assert_eq!(list.active().unwrap().id, 3);
    }

    #[test]
    fn remove_before_active_adjusts_index() {
        let mut list = BufferList::new();
        list.add(buf(1, "A"));
        list.add(buf(2, "B"));
        list.add(buf(3, "C"));
        list.set_active(3); // active = C (index 2)

        list.remove(1); // remove A (index 0, before active)
        // Active should still point to C
        assert_eq!(list.active().unwrap().id, 3);
    }

    #[test]
    fn remove_after_active_doesnt_change() {
        let mut list = BufferList::new();
        list.add(buf(1, "A"));
        list.add(buf(2, "B"));
        list.add(buf(3, "C"));

        list.remove(3); // remove C (after active A)
        assert_eq!(list.active().unwrap().id, 1);
    }

    #[test]
    fn remove_nonexistent_returns_false() {
        let mut list = BufferList::new();
        list.add(buf(1, "A"));
        assert!(!list.remove(99));
    }

    #[test]
    fn remove_last_with_active_at_end() {
        let mut list = BufferList::new();
        list.add(buf(1, "A"));
        list.add(buf(2, "B"));
        list.set_active(2); // active = B (index 1)

        list.remove(2); // remove B (the last one, which is active)
        // Should fall back to A (index 0)
        assert_eq!(list.active().unwrap().id, 1);
    }

    // --- Lookup ---

    #[test]
    fn get_by_id() {
        let mut list = BufferList::new();
        list.add(buf(1, "A"));
        list.add(buf(2, "B"));

        assert_eq!(list.get(2).unwrap().title, "B");
        assert!(list.get(99).is_none());
    }

    // --- Mutation ---

    #[test]
    fn mutate_buffer_state() {
        let mut list = BufferList::new();
        list.add(buf(1, "Original"));

        list.get_mut(1).unwrap().title = "Updated".to_string();
        assert_eq!(list.get(1).unwrap().title, "Updated");
    }

    #[test]
    fn mutate_active_buffer() {
        let mut list = BufferList::new();
        list.add(buf(1, "A"));
        list.active_mut().unwrap().url = "https://changed.com".to_string();
        assert_eq!(list.active().unwrap().url, "https://changed.com");
    }
}
