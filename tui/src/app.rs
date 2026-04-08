use crossterm::event::{KeyCode, KeyEvent, KeyEventKind};
use ratatui::prelude::*;
use ratatui::widgets::Paragraph;

#[derive(Debug, Clone, Copy, PartialEq)]
pub enum Mode {
    Normal,
    Command,
}

pub struct App {
    pub mode: Mode,
    pub command_buf: String,
    pub url: String,
    pub title: String,
    pub loading: bool,
    pub frame: Option<FrameData>,
    pub should_quit: bool,
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
                KeyCode::Char('r') => Some(Action::Reload),
                KeyCode::Char('H') => Some(Action::GoBack),
                KeyCode::Char('L') => Some(Action::GoForward),
                _ => None,
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
}

pub enum Action {
    Navigate(String),
    GoBack,
    GoForward,
    Reload,
    Command(String), // parsed in the event loop
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
    frame.render_widget(
        Paragraph::new(format!(" {} ", app.title))
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
    let status_text = if app.loading {
        format!(" Loading... {}", app.url)
    } else {
        format!(" {}", app.url)
    };
    frame.render_widget(
        Paragraph::new(status_text).style(Style::new().bg(Color::Blue).fg(Color::White)),
        status,
    );

    // Command line
    let cmd_text = match app.mode {
        Mode::Command => format!(":{}", app.command_buf),
        Mode::Normal => String::new(),
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
