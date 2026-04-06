use crossterm::event::{Event, KeyCode, KeyEvent, KeyEventKind, KeyModifiers};
use ratatui::{
    layout::{Constraint, Layout, Position},
    style::{Color, Style},
    text::Line,
    widgets::Paragraph,
    Frame,
};

/// Input mode — follows Neovim's modal model.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Mode {
    Normal,
    Insert,
    Command,
}

/// Application state.
pub struct App {
    mode: Mode,
    command_buffer: String,
    quit: bool,

    // Buffer state (received from core via RPC)
    current_url: String,
    current_title: String,
    loading: bool,
    load_progress: f64,

    // TODO: frame buffer from shared memory
    // TODO: tab list
}

impl App {
    pub fn new() -> Self {
        Self {
            mode: Mode::Normal,
            command_buffer: String::new(),
            quit: false,
            current_url: String::from("about:blank"),
            current_title: String::from("dirtferret"),
            loading: false,
            load_progress: 0.0,
        }
    }

    pub fn should_quit(&self) -> bool {
        self.quit
    }

    pub fn mode(&self) -> Mode {
        self.mode
    }

    /// Handle a terminal event.
    pub fn handle_event(&mut self, event: Event) {
        if let Event::Key(key) = event {
            if key.kind != KeyEventKind::Press {
                return;
            }
            match self.mode {
                Mode::Normal => self.handle_normal(key),
                Mode::Insert => self.handle_insert(key),
                Mode::Command => self.handle_command(key),
            }
        }
        // TODO: handle mouse events, resize events
    }

    /// Render the UI.
    pub fn render(&mut self, frame: &mut Frame) {
        let [tab_bar, viewport, status_line, command_line] = Layout::vertical([
            Constraint::Length(1),
            Constraint::Fill(1),
            Constraint::Length(1),
            Constraint::Length(1),
        ])
        .areas(frame.area());

        // Tab bar
        let tab_text = format!(" [1: {}] ", self.current_title);
        frame.render_widget(
            Paragraph::new(tab_text).style(Style::new().bg(Color::DarkGray)),
            tab_bar,
        );

        // Viewport — mark as skip for kitty graphics
        let buf = frame.buffer_mut();
        for pos in viewport.positions() {
            if let Some(cell) = buf.cell_mut(pos) {
                cell.set_skip(true);
            }
        }

        // Status line
        let mode_str = match self.mode {
            Mode::Normal => " NORMAL ",
            Mode::Insert => " INSERT ",
            Mode::Command => " COMMAND ",
        };
        let progress = if self.loading {
            format!(" {}% ", (self.load_progress * 100.0) as u32)
        } else {
            String::new()
        };
        let status = format!(" {}{}{}", self.current_url, progress, mode_str);
        let status_style = match self.mode {
            Mode::Normal => Style::new().bg(Color::Blue).fg(Color::White),
            Mode::Insert => Style::new().bg(Color::Green).fg(Color::Black),
            Mode::Command => Style::new().bg(Color::Yellow).fg(Color::Black),
        };
        frame.render_widget(Paragraph::new(status).style(status_style), status_line);

        // Command line
        match self.mode {
            Mode::Command => {
                let text = format!(":{}", self.command_buffer);
                frame.render_widget(Paragraph::new(text), command_line);
                frame.set_cursor_position(Position::new(
                    command_line.x + 1 + self.command_buffer.len() as u16,
                    command_line.y,
                ));
            }
            _ => {
                frame.render_widget(Paragraph::new(""), command_line);
            }
        }
    }

    // --- Mode handlers ---

    fn handle_normal(&mut self, key: KeyEvent) {
        match key.code {
            KeyCode::Char('q') => self.quit = true,
            KeyCode::Char('i') => self.mode = Mode::Insert,
            KeyCode::Char(':') => {
                self.mode = Mode::Command;
                self.command_buffer.clear();
            }
            // TODO: j/k scroll, gt/gT tab switch, o open URL, etc.
            _ => {}
        }
    }

    fn handle_insert(&mut self, key: KeyEvent) {
        match key.code {
            KeyCode::Esc => self.mode = Mode::Normal,
            // TODO: forward keystrokes to CEF via RPC
            _ => {}
        }
    }

    fn handle_command(&mut self, key: KeyEvent) {
        match key.code {
            KeyCode::Esc => {
                self.mode = Mode::Normal;
                self.command_buffer.clear();
            }
            KeyCode::Enter => {
                self.execute_command();
                self.mode = Mode::Normal;
            }
            KeyCode::Backspace => {
                self.command_buffer.pop();
                if self.command_buffer.is_empty() {
                    self.mode = Mode::Normal;
                }
            }
            KeyCode::Char(c) => {
                self.command_buffer.push(c);
            }
            _ => {}
        }
    }

    fn execute_command(&mut self) {
        let cmd = self.command_buffer.clone();
        self.command_buffer.clear();

        match cmd.as_str() {
            "q" | "quit" => self.quit = true,
            _ => {
                // TODO: parse and dispatch commands
                // e.g., "open <url>", "tabclose", etc.
            }
        }
    }
}

// --- Tests ---

#[cfg(test)]
mod tests {
    use super::*;
    use crossterm::event::{KeyCode, KeyEvent, KeyEventKind, KeyEventState, KeyModifiers};

    fn press(code: KeyCode) -> Event {
        Event::Key(KeyEvent {
            code,
            modifiers: KeyModifiers::NONE,
            kind: KeyEventKind::Press,
            state: KeyEventState::NONE,
        })
    }

    #[test]
    fn initial_mode_is_normal() {
        let app = App::new();
        assert_eq!(app.mode(), Mode::Normal);
    }

    #[test]
    fn i_enters_insert_mode() {
        let mut app = App::new();
        app.handle_event(press(KeyCode::Char('i')));
        assert_eq!(app.mode(), Mode::Insert);
    }

    #[test]
    fn esc_returns_to_normal_from_insert() {
        let mut app = App::new();
        app.handle_event(press(KeyCode::Char('i')));
        assert_eq!(app.mode(), Mode::Insert);
        app.handle_event(press(KeyCode::Esc));
        assert_eq!(app.mode(), Mode::Normal);
    }

    #[test]
    fn colon_enters_command_mode() {
        let mut app = App::new();
        app.handle_event(press(KeyCode::Char(':')));
        assert_eq!(app.mode(), Mode::Command);
    }

    #[test]
    fn command_mode_accumulates_text() {
        let mut app = App::new();
        app.handle_event(press(KeyCode::Char(':')));
        app.handle_event(press(KeyCode::Char('q')));
        app.handle_event(press(KeyCode::Char('u')));
        app.handle_event(press(KeyCode::Char('i')));
        app.handle_event(press(KeyCode::Char('t')));
        assert_eq!(app.command_buffer, "quit");
    }

    #[test]
    fn command_quit() {
        let mut app = App::new();
        assert!(!app.should_quit());
        app.handle_event(press(KeyCode::Char(':')));
        app.handle_event(press(KeyCode::Char('q')));
        app.handle_event(press(KeyCode::Enter));
        assert!(app.should_quit());
    }

    #[test]
    fn q_quits_in_normal_mode() {
        let mut app = App::new();
        app.handle_event(press(KeyCode::Char('q')));
        assert!(app.should_quit());
    }

    #[test]
    fn esc_cancels_command() {
        let mut app = App::new();
        app.handle_event(press(KeyCode::Char(':')));
        app.handle_event(press(KeyCode::Char('x')));
        app.handle_event(press(KeyCode::Esc));
        assert_eq!(app.mode(), Mode::Normal);
        assert!(app.command_buffer.is_empty());
    }

    #[test]
    fn backspace_in_empty_command_returns_to_normal() {
        let mut app = App::new();
        app.handle_event(press(KeyCode::Char(':')));
        app.handle_event(press(KeyCode::Backspace));
        assert_eq!(app.mode(), Mode::Normal);
    }
}
