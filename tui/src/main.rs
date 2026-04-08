mod app;
mod layout;
mod shm;
mod viewport;

mod core_capnp {
    include!(concat!(env!("OUT_DIR"), "/core_capnp.rs"));
}
mod types_capnp {
    include!(concat!(env!("OUT_DIR"), "/types_capnp.rs"));
}

use app::{Action, App, FrameData};
use shm::ShmReader;

use std::io::Write;
use std::time::Duration;

use capnp_rpc::{rpc_twoparty_capnp, twoparty};
use crossterm::event::{Event, EventStream};
use futures_util::io::AsyncReadExt;
use futures_util::StreamExt;
use ratatui::prelude::*;
use tokio_util::compat::TokioAsyncReadCompatExt;

// Frame notification from core -> main loop
struct FrameNotification {
    shm_name: String,
    width: u32,
    height: u32,
}

enum StateUpdate {
    Title { buffer_id: i32, title: String },
    Url { buffer_id: i32, url: String },
    Loading { buffer_id: i32, loading: bool, can_go_back: bool, can_go_forward: bool },
    Progress { buffer_id: i32, progress: f64 },
    BufferCreated { id: i32, url: String, title: String },
    BufferClosed { buffer_id: i32 },
}

// Ui callback implementation - receives RPC calls from core
struct UiImpl {
    frame_tx: tokio::sync::mpsc::Sender<FrameNotification>,
    state_tx: tokio::sync::mpsc::Sender<StateUpdate>,
}

impl core_capnp::ui::Server for UiImpl {
    fn on_frame(
        &mut self,
        params: core_capnp::ui::OnFrameParams,
        _results: core_capnp::ui::OnFrameResults,
    ) -> capnp::capability::Promise<(), capnp::Error> {
        let Ok(params) = params.get() else {
            return capnp::capability::Promise::ok(());
        };
        let shm_name = match params.get_shm_name() {
            Ok(s) => match s.to_string() {
                Ok(s) => s,
                Err(_) => return capnp::capability::Promise::ok(()),
            },
            Err(_) => return capnp::capability::Promise::ok(()),
        };
        let width = params.get_width();
        let height = params.get_height();

        let _ = self.frame_tx.try_send(FrameNotification {
            shm_name,
            width,
            height,
        });
        capnp::capability::Promise::ok(())
    }

    fn on_title_changed(
        &mut self,
        params: core_capnp::ui::OnTitleChangedParams,
        _results: core_capnp::ui::OnTitleChangedResults,
    ) -> capnp::capability::Promise<(), capnp::Error> {
        if let Ok(p) = params.get() {
            if let Ok(title) = p.get_title() {
                if let Ok(title) = title.to_string() {
                    let _ = self.state_tx.try_send(StateUpdate::Title {
                        buffer_id: p.get_buffer_id(),
                        title,
                    });
                }
            }
        }
        capnp::capability::Promise::ok(())
    }

    fn on_url_changed(
        &mut self,
        params: core_capnp::ui::OnUrlChangedParams,
        _results: core_capnp::ui::OnUrlChangedResults,
    ) -> capnp::capability::Promise<(), capnp::Error> {
        if let Ok(p) = params.get() {
            if let Ok(url) = p.get_url() {
                if let Ok(url) = url.to_string() {
                    let _ = self.state_tx.try_send(StateUpdate::Url {
                        buffer_id: p.get_buffer_id(),
                        url,
                    });
                }
            }
        }
        capnp::capability::Promise::ok(())
    }

    fn on_loading_state_changed(
        &mut self,
        params: core_capnp::ui::OnLoadingStateChangedParams,
        _results: core_capnp::ui::OnLoadingStateChangedResults,
    ) -> capnp::capability::Promise<(), capnp::Error> {
        if let Ok(p) = params.get() {
            let _ = self.state_tx.try_send(StateUpdate::Loading {
                buffer_id: p.get_buffer_id(),
                loading: p.get_loading(),
                can_go_back: p.get_can_go_back(),
                can_go_forward: p.get_can_go_forward(),
            });
        }
        capnp::capability::Promise::ok(())
    }

    fn on_load_progress(
        &mut self,
        params: core_capnp::ui::OnLoadProgressParams,
        _results: core_capnp::ui::OnLoadProgressResults,
    ) -> capnp::capability::Promise<(), capnp::Error> {
        if let Ok(p) = params.get() {
            let _ = self.state_tx.try_send(StateUpdate::Progress {
                buffer_id: p.get_buffer_id(),
                progress: p.get_progress(),
            });
        }
        capnp::capability::Promise::ok(())
    }

    fn on_buffer_created(
        &mut self,
        params: core_capnp::ui::OnBufferCreatedParams,
        _results: core_capnp::ui::OnBufferCreatedResults,
    ) -> capnp::capability::Promise<(), capnp::Error> {
        if let Ok(p) = params.get() {
            if let Ok(info) = p.get_info() {
                let url = info.get_url().ok()
                    .and_then(|s| s.to_string().ok())
                    .unwrap_or_default();
                let title = info.get_title().ok()
                    .and_then(|s| s.to_string().ok())
                    .unwrap_or_default();
                let _ = self.state_tx.try_send(StateUpdate::BufferCreated {
                    id: info.get_id(),
                    url,
                    title,
                });
            }
        }
        capnp::capability::Promise::ok(())
    }

    fn on_buffer_closed(
        &mut self,
        params: core_capnp::ui::OnBufferClosedParams,
        _results: core_capnp::ui::OnBufferClosedResults,
    ) -> capnp::capability::Promise<(), capnp::Error> {
        if let Ok(p) = params.get() {
            let _ = self.state_tx.try_send(StateUpdate::BufferClosed {
                buffer_id: p.get_buffer_id(),
            });
        }
        capnp::capability::Promise::ok(())
    }

    fn on_focused_field_changed(
        &mut self,
        _params: core_capnp::ui::OnFocusedFieldChangedParams,
        _results: core_capnp::ui::OnFocusedFieldChangedResults,
    ) -> capnp::capability::Promise<(), capnp::Error> {
        capnp::capability::Promise::ok(())
    }

    fn on_cursor_changed(
        &mut self,
        _params: core_capnp::ui::OnCursorChangedParams,
        _results: core_capnp::ui::OnCursorChangedResults,
    ) -> capnp::capability::Promise<(), capnp::Error> {
        capnp::capability::Promise::ok(())
    }

    fn on_console_message(
        &mut self,
        _params: core_capnp::ui::OnConsoleMessageParams,
        _results: core_capnp::ui::OnConsoleMessageResults,
    ) -> capnp::capability::Promise<(), capnp::Error> {
        capnp::capability::Promise::ok(())
    }
}

fn viewport_pixel_size() -> anyhow::Result<(u32, u32)> {
    let win = crossterm::terminal::window_size()?;

    // If the terminal reports pixel dimensions, use them directly.
    // Otherwise fall back to assuming 8x16 cells (common monospace).
    let (pw, ph) = if win.width > 0 && win.height > 0 {
        let viewport_rows = win.rows.saturating_sub(3); // 3 rows chrome
        let cell_h = win.height / win.rows.max(1);
        (win.width as u32, viewport_rows as u32 * cell_h as u32)
    } else {
        let viewport_rows = win.rows.saturating_sub(3);
        (win.columns as u32 * 8, viewport_rows as u32 * 16)
    };

    Ok((pw.max(1), ph.max(1)))
}

#[tokio::main]
async fn main() -> anyhow::Result<()> {
    let local = tokio::task::LocalSet::new();
    local
        .run_until(async move {
            if let Err(e) = run().await {
                // Restore terminal before printing error
                let _ = crossterm::terminal::disable_raw_mode();
                let _ = crossterm::execute!(
                    std::io::stdout(),
                    crossterm::terminal::LeaveAlternateScreen
                );
                eprintln!("Error: {e:#}");
                std::process::exit(1);
            }
        })
        .await;
    Ok(())
}

async fn run() -> anyhow::Result<()> {
    // Connect to core via RPC
    let stream = tokio::net::TcpStream::connect("127.0.0.1:5000").await?;
    let (reader, writer) = TokioAsyncReadCompatExt::compat(stream).split();

    let rpc_network = Box::new(twoparty::VatNetwork::new(
        reader,
        writer,
        rpc_twoparty_capnp::Side::Client,
        Default::default(),
    ));
    let mut rpc_system = capnp_rpc::RpcSystem::new(rpc_network, None);
    let core: core_capnp::core::Client =
        rpc_system.bootstrap(rpc_twoparty_capnp::Side::Server);

    tokio::task::spawn_local(rpc_system);

    // Frame notification channel
    let (frame_tx, mut frame_rx) = tokio::sync::mpsc::channel::<FrameNotification>(4);
    let (state_tx, mut state_rx) = tokio::sync::mpsc::channel::<StateUpdate>(32);

    // Attach UI to core
    let (pw, ph) = viewport_pixel_size()?;
    let ui_impl = UiImpl { frame_tx, state_tx };
    let ui_client: core_capnp::ui::Client = capnp_rpc::new_client(ui_impl);

    let mut req = core.attach_ui_request();
    req.get().set_ui(ui_client);
    req.get().set_width(pw);
    req.get().set_height(ph);
    req.send().promise.await?;

    // Setup terminal
    crossterm::terminal::enable_raw_mode()?;
    crossterm::execute!(
        std::io::stdout(),
        crossterm::terminal::EnterAlternateScreen,
        crossterm::cursor::Hide
    )?;
    let backend = CrosstermBackend::new(std::io::stdout());
    let mut terminal = ratatui::Terminal::new(backend)?;

    let mut app = App::new();
    let mut events = EventStream::new();
    let mut render_interval = tokio::time::interval(Duration::from_millis(33));
    let mut current_shm: Option<ShmReader> = None;
    let mut dirty = true; // redraw on first frame

    loop {
        tokio::select! {
            _ = render_interval.tick() => {
                if !dirty {
                    continue;
                }
                dirty = false;

                let mut viewport_area = Rect::default();
                terminal.draw(|f| app::render(f, &app, &mut viewport_area))?;

                // Write kitty graphics AFTER ratatui draw
                if let Some(ref shm) = current_shm {
                    if let Some(ref frame_data) = app.frame {
                        let mut pixels = shm.as_bytes().to_vec();
                        viewport::bgra_to_rgba(&mut pixels);
                        let escape = viewport::kitty_display(
                            &pixels, frame_data.width, frame_data.height, 1,
                        );

                        let mut stdout = std::io::stdout().lock();
                        write!(stdout, "\x1b[{};{}H",
                               viewport_area.y + 1, viewport_area.x + 1)?;
                        stdout.write_all(&escape)?;
                        stdout.flush()?;
                    }
                }
            }

            Some(Ok(event)) = events.next() => {
                match event {
                    Event::Key(key) => {
                        dirty = true;
                        if let Some(action) = app.handle_key(key) {
                            match action {
                                Action::GoBack => {
                                    let mut req = core.go_back_request();
                                    req.get().set_buffer_id(0);
                                    req.send().promise.await?;
                                }
                                Action::GoForward => {
                                    let mut req = core.go_forward_request();
                                    req.get().set_buffer_id(0);
                                    req.send().promise.await?;
                                }
                                Action::Reload => {
                                    let mut req = core.reload_request();
                                    req.get().set_buffer_id(0);
                                    req.send().promise.await?;
                                }
                                Action::Command(cmd) => {
                                    let url = if cmd.contains('.') || cmd.starts_with("http") {
                                        if !cmd.starts_with("http") {
                                            format!("https://{}", cmd)
                                        } else {
                                            cmd
                                        }
                                    } else {
                                        cmd
                                    };
                                    let mut req = core.navigate_request();
                                    req.get().set_buffer_id(0);
                                    req.get().set_url(&url);
                                    req.send().promise.await?;
                                }
                                Action::Navigate(url) => {
                                    let mut req = core.navigate_request();
                                    req.get().set_buffer_id(0);
                                    req.get().set_url(&url);
                                    req.send().promise.await?;
                                }
                                Action::SendKey { character, modifiers } => {
                                    // Send three events: RAWKEYDOWN, CHAR, KEYUP
                                    for key_type in [0u32, 3, 2] {
                                        let mut req = core.send_key_event_request();
                                        req.get().set_buffer_id(0);
                                        let mut event = req.get().init_event();
                                        event.set_type(match key_type {
                                            0 => types_capnp::KeyEventType::RawKeyDown,
                                            2 => types_capnp::KeyEventType::KeyUp,
                                            3 => types_capnp::KeyEventType::Char,
                                            _ => types_capnp::KeyEventType::RawKeyDown,
                                        });
                                        event.set_key_code(character);
                                        event.set_character(character);
                                        event.set_modifiers(modifiers);
                                        let _ = req.send().promise.await;
                                    }
                                }
                            }
                        }
                        if app.should_quit {
                            break;
                        }
                    }
                    Event::Resize(_cols, _rows) => {
                        dirty = true;
                        if let Ok((pw, ph)) = viewport_pixel_size() {
                            let mut req = core.resize_request();
                            req.get().set_buffer_id(0);
                            req.get().set_width(pw);
                            req.get().set_height(ph);
                            let _ = req.send().promise.await;
                        }
                    }
                    _ => {}
                }
            }

            Some(notif) = frame_rx.recv() => {
                dirty = true;
                let size = (notif.width * notif.height * 4) as usize;
                if current_shm.as_ref().map_or(true, |s| s.len != size) {
                    current_shm = Some(ShmReader::open(&notif.shm_name, size)?);
                }
                app.frame = Some(FrameData {
                    shm_name: notif.shm_name,
                    width: notif.width,
                    height: notif.height,
                });
            }

            Some(update) = state_rx.recv() => {
                dirty = true;
                match update {
                    StateUpdate::Title { buffer_id: _, title } => {
                        app.title = title;
                    }
                    StateUpdate::Url { buffer_id: _, url } => {
                        app.url = url;
                    }
                    StateUpdate::Loading { buffer_id: _, loading, .. } => {
                        app.loading = loading;
                    }
                    StateUpdate::Progress { .. } => {}
                    StateUpdate::BufferCreated { .. } => {}
                    StateUpdate::BufferClosed { .. } => {}
                }
            }
        }
    }

    // Cleanup: delete kitty images, restore terminal
    let mut stdout = std::io::stdout().lock();
    write!(stdout, "\x1b_Ga=d,d=a\x1b\\")?;
    stdout.flush()?;
    drop(stdout);

    crossterm::terminal::disable_raw_mode()?;
    crossterm::execute!(
        std::io::stdout(),
        crossterm::cursor::Show,
        crossterm::terminal::LeaveAlternateScreen
    )?;

    Ok(())
}
