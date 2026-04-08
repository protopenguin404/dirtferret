use crossterm::event::{KeyCode, KeyEvent, KeyEventKind, KeyModifiers};
use ratatui::prelude::*;
use ratatui::widgets::Paragraph;

#[derive(Debug, Clone, Copy, PartialEq)]
pub enum Mode {
    Normal,
    Insert,
    Command,
}

pub struct BufferEntry {
    pub id: i32,
    pub title: String,
    pub url: String,
    pub loading: bool,
}

pub struct App {
    pub mode: Mode,
    pub command_buf: String,
    pub url: String,
    pub title: String,
    pub loading: bool,
    pub frame: Option<FrameData>,
    pub should_quit: bool,
    pub buffers: Vec<BufferEntry>,
    pub active_buffer_id: i32,
}

impl App {
    pub fn new() -> Self {
        Self {
            mode: Mode::Normal,
            command_buf: String::new(),
            url: String::new(),
            title: String::from("dirtferret"),
            loading: false,
            frame: None,
            should_quit: false,
            buffers: Vec::new(),
            active_buffer_id: -1,
        }
    }
}

pub struct FrameData {
    pub shm_name: String,
    pub width: u32,
    pub height: u32,
}

impl App {
    pub fn handle_key(&mut self, key: KeyEvent) -> Option<Action> {
        if key.kind != KeyEventKind::Press {
            return None;
        }
        match self.mode {
            Mode::Normal => match key.code {
                KeyCode::Char(':') => {
                    self.mode = Mode::Command;
                    None
                }
                KeyCode::Char('q') => {
                    self.should_quit = true;
                    None
                }
                KeyCode::Char('i') => {
                    self.mode = Mode::Insert;
                    None
                }
                KeyCode::Char('r') => Some(Action::Reload),
                KeyCode::Char('H') => Some(Action::GoBack),
                KeyCode::Char('L') => Some(Action::GoForward),
                KeyCode::Char('t') => Some(Action::NextTab),
                KeyCode::Char('T') => Some(Action::PrevTab),
                _ => None,
            },
            Mode::Insert => match key.code {
                KeyCode::Esc => {
                    self.mode = Mode::Normal;
                    None
                }
                _ => {
                    let (character, modifiers) = key_to_cef(key);
                    Some(Action::SendKey { character, modifiers })
                }
            },
            Mode::Command => match key.code {
                KeyCode::Esc => {
                    self.mode = Mode::Normal;
                    self.command_buf.clear();
                    None
                }
                KeyCode::Enter => {
                    let cmd = std::mem::take(&mut self.command_buf);
                    self.mode = Mode::Normal;
                    Some(Action::Command(cmd))
                }
                KeyCode::Backspace => {
                    self.command_buf.pop();
                    None
                }
                KeyCode::Char(c) => {
                    self.command_buf.push(c);
                    None
                }
                _ => None,
            },
        }
    }

    pub fn add_buffer(&mut self, id: i32, url: String, title: String) {
        self.buffers.push(BufferEntry { id, title, url, loading: false });
        if self.active_buffer_id < 0 {
            self.active_buffer_id = id;
        }
    }

    pub fn remove_buffer(&mut self, id: i32) {
        self.buffers.retain(|b| b.id != id);
        if self.active_buffer_id == id {
            self.active_buffer_id = self.buffers.first().map_or(-1, |b| b.id);
        }
    }

    pub fn update_buffer_title(&mut self, id: i32, title: String) {
        if let Some(buf) = self.buffers.iter_mut().find(|b| b.id == id) {
            buf.title = title.clone();
        }
        if id == self.active_buffer_id {
            self.title = title;
        }
    }

    pub fn update_buffer_url(&mut self, id: i32, url: String) {
        if let Some(buf) = self.buffers.iter_mut().find(|b| b.id == id) {
            buf.url = url.clone();
        }
        if id == self.active_buffer_id {
            self.url = url;
        }
    }

    pub fn next_buffer(&mut self) {
        if self.buffers.len() <= 1 { return; }
        let idx = self.buffers.iter().position(|b| b.id == self.active_buffer_id).unwrap_or(0);
        let next = (idx + 1) % self.buffers.len();
        self.active_buffer_id = self.buffers[next].id;
    }

    pub fn prev_buffer(&mut self) {
        if self.buffers.len() <= 1 { return; }
        let idx = self.buffers.iter().position(|b| b.id == self.active_buffer_id).unwrap_or(0);
        let prev = if idx == 0 { self.buffers.len() - 1 } else { idx - 1 };
        self.active_buffer_id = self.buffers[prev].id;
    }
}

pub enum Action {
    Navigate(String),
    GoBack,
    GoForward,
    Reload,
    Command(String), // parsed in the event loop
    SendKey { character: u32, modifiers: u32 },
    NextTab,
    PrevTab,
    NewTab(String),
    CloseTab,
}

fn key_to_cef(key: KeyEvent) -> (u32, u32) {
    let character = match key.code {
        KeyCode::Char(c) => c as u32,
        KeyCode::Enter => '\r' as u32,
        KeyCode::Tab => '\t' as u32,
        KeyCode::Backspace => 0x08,
        KeyCode::Delete => 0x2E,
        KeyCode::Left => 0x25,
        KeyCode::Right => 0x27,
        KeyCode::Up => 0x26,
        KeyCode::Down => 0x28,
        KeyCode::Home => 0x24,
        KeyCode::End => 0x23,
        KeyCode::PageUp => 0x21,
        KeyCode::PageDown => 0x22,
        KeyCode::Esc => 0x1B,
        _ => 0,
    };

    let mut modifiers: u32 = 0;
    if key.modifiers.contains(KeyModifiers::SHIFT) {
        modifiers |= 1;
    }
    if key.modifiers.contains(KeyModifiers::CONTROL) {
        modifiers |= 2;
    }
    if key.modifiers.contains(KeyModifiers::ALT) {
        modifiers |= 4;
    }

    (character, modifiers)
}

pub fn render(frame: &mut Frame, app: &App, viewport_area: &mut Rect) {
    let [tab_bar, viewport, status, cmd] = Layout::vertical([
        Constraint::Length(1),
        Constraint::Fill(1),
        Constraint::Length(1),
        Constraint::Length(1),
    ])
    .areas(frame.area());

    *viewport_area = viewport; // save for kitty graphics positioning

    // Tab bar
    let tab_text = if app.buffers.is_empty() {
        " dirtferret ".to_string()
    } else {
        app.buffers.iter().map(|b| {
            if b.id == app.active_buffer_id {
                format!(" [{}] ", if b.title.is_empty() { "..." } else { &b.title })
            } else {
                format!("  {}  ", if b.title.is_empty() { "..." } else { &b.title })
            }
        }).collect::<String>()
    };
    frame.render_widget(
        Paragraph::new(tab_text)
            .style(Style::new().bg(Color::DarkGray).fg(Color::White)),
        tab_bar,
    );

    // Viewport — mark cells as skip so ratatui doesn't overwrite kitty graphics
    let buf = frame.buffer_mut();
    for pos in viewport.positions() {
        if let Some(cell) = buf.cell_mut(pos) {
            cell.set_skip(true);
        }
    }

    // Status line
    let mode_indicator = match app.mode {
        Mode::Normal => " NORMAL ",
        Mode::Insert => " INSERT ",
        Mode::Command => " COMMAND ",
    };
    let status_text = if app.loading {
        format!("{}Loading... {}", mode_indicator, app.url)
    } else {
        format!("{}{}", mode_indicator, app.url)
    };
    frame.render_widget(
        Paragraph::new(status_text).style(Style::new().bg(Color::Blue).fg(Color::White)),
        status,
    );

    // Command line
    let cmd_text = match app.mode {
        Mode::Command => format!(":{}", app.command_buf),
        Mode::Normal | Mode::Insert => String::new(),
    };
    frame.render_widget(Paragraph::new(cmd_text), cmd);

    // Show cursor in command mode
    if app.mode == Mode::Command {
        frame.set_cursor_position(Position::new(
            cmd.x + 1 + app.command_buf.len() as u16,
            cmd.y,
        ));
    }
}
