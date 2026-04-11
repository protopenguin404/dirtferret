mod input;
mod mode;
mod mux;
mod ui;
mod shm;
mod viewport;

pub mod core_capnp {
    include!(concat!(env!("OUT_DIR"), "/core_capnp.rs"));
}
pub mod types_capnp {
    include!(concat!(env!("OUT_DIR"), "/types_capnp.rs"));
}

use capnp_rpc::{rpc_twoparty_capnp, twoparty, RpcSystem};
use crossterm::event::{Event, EventStream, KeyEventKind, MouseButton, MouseEventKind};
use futures_util::{AsyncReadExt, StreamExt};
use std::collections::VecDeque;
use std::io::Write;
use std::time::{Duration, Instant};
use base64::Engine as _;

const IMAGE_ID: u32 = 1;

fn main() -> anyhow::Result<()> {
    let args: Vec<String> = std::env::args().collect();
    if args.iter().any(|a| a == "--test") {
        return run_test();
    }
    if args.iter().any(|a| a == "--test-loop") {
        return run_test_loop();
    }
    let rt = tokio::runtime::Builder::new_current_thread()
        .enable_all()
        .build()?;
    let local = tokio::task::LocalSet::new();
    local.block_on(&rt, async_main())
}

fn run_test() -> anyhow::Result<()> {
    let mut stdout = std::io::stdout();
    let tiny: [u8; 16] = [255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 0, 255];
    let tiny_b64 = base64::engine::general_purpose::STANDARD.encode(&tiny);
    write!(stdout, "\x1b[H")?;
    stdout.write_all(format!("\x1b_Ga=T,t=d,f=32,s=2,v=2,i=1;{}\x1b\\", tiny_b64).as_bytes())?;
    write!(stdout, "\n\n")?;
    let pattern = viewport::test_pattern(100, 100);
    let escape = viewport::kitty_display_direct(&pattern, 100, 100, 2);
    stdout.write_all(&escape)?;
    write!(stdout, "\n\n")?;
    stdout.flush()?;
    eprintln!("[test] Press Enter to exit.");
    let mut buf = String::new();
    std::io::stdin().read_line(&mut buf)?;
    Ok(())
}

fn run_test_loop() -> anyhow::Result<()> {
    let mut stdout = std::io::stdout();
    write!(stdout, "\x1b[?25l\x1b[2J\x1b[H")?;
    stdout.flush()?;
    let pattern = viewport::test_pattern(200, 200);
    loop {
        let escape = viewport::kitty_display_direct(&pattern, 200, 200, IMAGE_ID);
        write!(stdout, "\x1b[H")?;
        stdout.write_all(&escape)?;
        stdout.flush()?;
        std::thread::sleep(Duration::from_secs(1));
    }
}

// --- RPC callback events ---

enum MuxEvent {
    BufferCreated { id: i32 },
    BufferClosed { id: i32 },
    TitleChanged { id: i32, title: String },
    UrlChanged { id: i32, url: String },
}

// --- RPC callback implementation ---

struct UiImpl {
    mux_tx: tokio::sync::mpsc::Sender<MuxEvent>,
}

impl core_capnp::ui::Server for UiImpl {
    fn on_frame(
        &mut self, _: core_capnp::ui::OnFrameParams, _: core_capnp::ui::OnFrameResults,
    ) -> capnp::capability::Promise<(), capnp::Error> {
        capnp::capability::Promise::ok(())
    }
    fn on_buffer_created(
        &mut self, params: core_capnp::ui::OnBufferCreatedParams, _: core_capnp::ui::OnBufferCreatedResults,
    ) -> capnp::capability::Promise<(), capnp::Error> {
        if let Ok(p) = params.get() {
            if let Ok(info) = p.get_info() {
                let _ = self.mux_tx.try_send(MuxEvent::BufferCreated { id: info.get_id() });
            }
        }
        capnp::capability::Promise::ok(())
    }
    fn on_buffer_closed(
        &mut self, params: core_capnp::ui::OnBufferClosedParams, _: core_capnp::ui::OnBufferClosedResults,
    ) -> capnp::capability::Promise<(), capnp::Error> {
        if let Ok(p) = params.get() {
            let _ = self.mux_tx.try_send(MuxEvent::BufferClosed { id: p.get_buffer_id() });
        }
        capnp::capability::Promise::ok(())
    }
    fn on_title_changed(
        &mut self, params: core_capnp::ui::OnTitleChangedParams, _: core_capnp::ui::OnTitleChangedResults,
    ) -> capnp::capability::Promise<(), capnp::Error> {
        if let Ok(p) = params.get() {
            if let Ok(title) = p.get_title() {
                if let Ok(t) = title.to_str() {
                    let _ = self.mux_tx.try_send(MuxEvent::TitleChanged { id: p.get_buffer_id(), title: t.to_string() });
                }
            }
        }
        capnp::capability::Promise::ok(())
    }
    fn on_url_changed(
        &mut self, params: core_capnp::ui::OnUrlChangedParams, _: core_capnp::ui::OnUrlChangedResults,
    ) -> capnp::capability::Promise<(), capnp::Error> {
        if let Ok(p) = params.get() {
            if let Ok(url) = p.get_url() {
                if let Ok(u) = url.to_str() {
                    let _ = self.mux_tx.try_send(MuxEvent::UrlChanged { id: p.get_buffer_id(), url: u.to_string() });
                }
            }
        }
        capnp::capability::Promise::ok(())
    }
    fn on_loading_state_changed(
        &mut self, _: core_capnp::ui::OnLoadingStateChangedParams, _: core_capnp::ui::OnLoadingStateChangedResults,
    ) -> capnp::capability::Promise<(), capnp::Error> { capnp::capability::Promise::ok(()) }
    fn on_load_progress(
        &mut self, _: core_capnp::ui::OnLoadProgressParams, _: core_capnp::ui::OnLoadProgressResults,
    ) -> capnp::capability::Promise<(), capnp::Error> { capnp::capability::Promise::ok(()) }
    fn on_focused_field_changed(
        &mut self, _: core_capnp::ui::OnFocusedFieldChangedParams, _: core_capnp::ui::OnFocusedFieldChangedResults,
    ) -> capnp::capability::Promise<(), capnp::Error> { capnp::capability::Promise::ok(()) }
    fn on_cursor_changed(
        &mut self, _: core_capnp::ui::OnCursorChangedParams, _: core_capnp::ui::OnCursorChangedResults,
    ) -> capnp::capability::Promise<(), capnp::Error> { capnp::capability::Promise::ok(()) }
    fn on_console_message(
        &mut self, _: core_capnp::ui::OnConsoleMessageParams, _: core_capnp::ui::OnConsoleMessageResults,
    ) -> capnp::capability::Promise<(), capnp::Error> { capnp::capability::Promise::ok(()) }
}

// --- Action dispatch helper ---

fn dispatch_action(
    name: &str,
    _arg: &str,
    buf_id: i32,
    core: &core_capnp::core::Client,
    mux_state: &mut mux::Mux,
) -> bool {
    match name {
        "quit" => return true,
        "scroll-down" => {
            let mut r = core.send_scroll_event_request();
            r.get().set_buffer_id(buf_id);
            r.get().set_delta_y(-120);
            let _ = r.send();
        }
        "scroll-up" => {
            let mut r = core.send_scroll_event_request();
            r.get().set_buffer_id(buf_id);
            r.get().set_delta_y(120);
            let _ = r.send();
        }
        "scroll-half-down" => {
            let mut r = core.send_scroll_event_request();
            r.get().set_buffer_id(buf_id);
            r.get().set_delta_y(-600);
            let _ = r.send();
        }
        "scroll-half-up" => {
            let mut r = core.send_scroll_event_request();
            r.get().set_buffer_id(buf_id);
            r.get().set_delta_y(600);
            let _ = r.send();
        }
        "go-back" => {
            let mut r = core.go_back_request();
            r.get().set_buffer_id(buf_id);
            let _ = r.send();
        }
        "go-forward" => {
            let mut r = core.go_forward_request();
            r.get().set_buffer_id(buf_id);
            let _ = r.send();
        }
        "reload" => {
            let mut r = core.reload_request();
            r.get().set_buffer_id(buf_id);
            let _ = r.send();
        }
        "tab-next" => { mux_state.next_tab(); }
        "tab-prev" => { mux_state.prev_tab(); }
        other => {
            eprintln!("[tui] Unknown action: {}", other);
        }
    }
    false // not quit
}

// --- Main ---

async fn async_main() -> anyhow::Result<()> {
    let (pw, ph) = input::viewport_pixel_size()?;
    let (cols, rows) = crossterm::terminal::size()?;
    eprintln!("[tui] Terminal: {}x{} cells, {}x{} pixels", cols, rows, pw, ph);

    // --- RPC connect ---
    let stream = tokio::net::TcpStream::connect("127.0.0.1:5000").await?;
    stream.set_nodelay(true)?;
    let (reader, writer) = tokio_util::compat::TokioAsyncReadCompatExt::compat(stream).split();
    let rpc_network = Box::new(twoparty::VatNetwork::new(
        futures_util::io::BufReader::new(reader),
        futures_util::io::BufWriter::new(writer),
        rpc_twoparty_capnp::Side::Client,
        Default::default(),
    ));
    let mut rpc_system = RpcSystem::new(rpc_network, None);
    let core: core_capnp::core::Client = rpc_system.bootstrap(rpc_twoparty_capnp::Side::Server);
    tokio::task::spawn_local(rpc_system);

    // --- RPC callbacks via channel ---
    let (mux_tx, mut mux_rx) = tokio::sync::mpsc::channel::<MuxEvent>(16);
    let ui_impl = UiImpl { mux_tx };
    let ui_client: core_capnp::ui::Client = capnp_rpc::new_client(ui_impl);

    let mut req = core.attach_ui_request();
    req.get().set_ui(ui_client);
    req.get().set_width(pw);
    req.get().set_height(ph);
    req.send().promise.await?;

    // Wait for initial buffer creation
    tokio::time::sleep(Duration::from_millis(500)).await;

    // --- Initialize components ---
    let mut mux_state = mux::Mux::new();
    let mut ui_state = ui::Ui::new();
    let mut current_mode = mode::MODE_NORMAL.to_string();
    let mut current_url = String::new();

    // Drain buffered mux events
    while let Ok(event) = mux_rx.try_recv() {
        match event {
            MuxEvent::BufferCreated { id } => { mux_state.add_tab(id, format!("Buffer {}", id)); }
            MuxEvent::TitleChanged { id, title } => { mux_state.update_title(id, &title); }
            MuxEvent::UrlChanged { url, .. } => { current_url = url; }
            MuxEvent::BufferClosed { id } => {
                if let Some(idx) = mux_state.tabs().iter().position(|t| t.buffer_id == id) {
                    mux_state.close_tab(idx);
                }
            }
        }
    }

    // Compute viewport rect (total area minus chrome)
    let vp_rect = ui_state.viewport_rect(cols, rows, pw, ph);

    // Create viewport for active buffer
    let active_id = mux_state.active_buffer_id().unwrap_or(1);
    let mut active_viewport = match viewport::Viewport::new(active_id, vp_rect, IMAGE_ID) {
        Ok(vp) => Some(vp),
        Err(e) => {
            eprintln!("[tui] Failed to create viewport: {}", e);
            None
        }
    };

    // --- Terminal setup ---
    crossterm::terminal::enable_raw_mode()?;
    let mut stdout = std::io::stdout();
    crossterm::execute!(
        stdout,
        crossterm::event::EnableMouseCapture,
        crossterm::cursor::Hide,
        crossterm::terminal::Clear(crossterm::terminal::ClearType::All),
    )?;

    // ratatui on stderr (stdout is for kitty graphics)
    let backend = ratatui::backend::CrosstermBackend::new(std::io::stderr());
    let mut terminal = ratatui::Terminal::new(backend)?;

    // --- Event loop state ---
    let mut events = EventStream::new();
    let mut render_interval = tokio::time::interval(Duration::from_millis(5));
    render_interval.set_missed_tick_behavior(tokio::time::MissedTickBehavior::Skip);

    let mut frame_times: VecDeque<Instant> = VecDeque::with_capacity(64);
    let mut total_frames: u64 = 0;
    let session_start = Instant::now();

    ui_state.set_mode(&current_mode);

    loop {
        tokio::select! {
            // --- Render tick ---
            _ = render_interval.tick() => {
                if let Some(ref mut vp) = active_viewport {
                    match vp.poll_and_render(&mut stdout) {
                        Ok(true) => {
                            total_frames += 1;
                            let now = Instant::now();
                            frame_times.push_back(now);
                            while frame_times.front().is_some_and(|t| now.duration_since(*t) > Duration::from_secs(1)) {
                                frame_times.pop_front();
                            }
                        }
                        Ok(false) => {}
                        Err(e) => { eprintln!("[tui] Render error: {}", e); }
                    }
                }

                // Update UI state
                ui_state.set_fps(frame_times.len());
                ui_state.set_url(&current_url);
                let tab_titles: Vec<String> = mux_state.tabs().iter().map(|t| t.title.clone()).collect();
                ui_state.set_tabs(tab_titles, mux_state.active_index());

                // Draw chrome
                terminal.draw(|frame| { ui_state.render(frame); })?;
                stdout.flush()?;
            }

            // --- Mux events from RPC callbacks ---
            Some(event) = mux_rx.recv() => {
                match event {
                    MuxEvent::BufferCreated { id } => { mux_state.add_tab(id, format!("Buffer {}", id)); }
                    MuxEvent::TitleChanged { id, title } => { mux_state.update_title(id, &title); }
                    MuxEvent::UrlChanged { id, url } => {
                        if Some(id) == mux_state.active_buffer_id() { current_url = url; }
                    }
                    MuxEvent::BufferClosed { id } => {
                        if let Some(idx) = mux_state.tabs().iter().position(|t| t.buffer_id == id) {
                            mux_state.close_tab(idx);
                        }
                    }
                }
            }

            // --- Terminal events ---
            Some(Ok(event)) = events.next() => {
                match event {
                    Event::Key(key) if key.kind == KeyEventKind::Press => {
                        let action = mode::resolve(&current_mode, &key);
                        match action {
                            mode::Action::SwitchMode(new_mode) => {
                                current_mode = new_mode;
                                ui_state.set_mode(&current_mode);
                            }
                            mode::Action::Execute(name, arg) => {
                                let buf_id = mux_state.active_buffer_id().unwrap_or(1);
                                if dispatch_action(&name, &arg, buf_id, &core, &mut mux_state) {
                                    break; // quit
                                }
                            }
                            mode::Action::ResolveViaRpc => {
                                let buf_id = mux_state.active_buffer_id().unwrap_or(1);
                                let (character, modifiers) = input::key_to_cef(key.code, key.modifiers);
                                if character == 0 { continue; }

                                let mut req = core.resolve_keybind_request();
                                req.get().set_mode("n"); // Lua uses "n" for normal
                                req.get().set_key_code(character);
                                req.get().set_character(character);
                                req.get().set_modifiers(modifiers);

                                if let Ok(response) = req.send().promise.await {
                                    if let Ok(results) = response.get() {
                                        let action_str = results.get_action()
                                            .map(|r| r.to_str().unwrap_or("")).unwrap_or("");
                                        let arg_str = results.get_arg()
                                            .map(|r| r.to_str().unwrap_or("")).unwrap_or("");

                                        if action_str.is_empty() {
                                            // No binding in Lua — noop
                                        } else if action_str.starts_with("switch-mode:") {
                                            let new_mode = &action_str["switch-mode:".len()..];
                                            current_mode = new_mode.to_string();
                                            ui_state.set_mode(&current_mode);
                                        } else if dispatch_action(action_str, arg_str, buf_id, &core, &mut mux_state) {
                                            break; // quit
                                        }
                                    }
                                }
                            }
                            mode::Action::SendToCef => {
                                let buf_id = mux_state.active_buffer_id().unwrap_or(1);
                                let (character, modifiers) = input::key_to_cef(key.code, key.modifiers);
                                if character != 0 {
                                    for key_type in [0u32, 3, 2] {
                                        let mut req = core.send_key_event_request();
                                        req.get().set_buffer_id(buf_id);
                                        let mut ev = req.get().init_event();
                                        ev.set_type(match key_type {
                                            0 => types_capnp::KeyEventType::RawKeyDown,
                                            2 => types_capnp::KeyEventType::KeyUp,
                                            3 => types_capnp::KeyEventType::Char,
                                            _ => types_capnp::KeyEventType::RawKeyDown,
                                        });
                                        ev.set_key_code(character);
                                        ev.set_character(character);
                                        ev.set_modifiers(modifiers);
                                        let _ = req.send();
                                    }
                                }
                            }
                            mode::Action::Noop => {}
                        }
                    }

                    Event::Mouse(mouse) => {
                        let buf_id = mux_state.active_buffer_id().unwrap_or(1);
                        let (px, py) = input::cell_to_pixel(mouse.column, mouse.row);
                        match mouse.kind {
                            MouseEventKind::Down(btn) | MouseEventKind::Up(btn) => {
                                let is_up = matches!(mouse.kind, MouseEventKind::Up(_));
                                let mut req = core.send_mouse_event_request();
                                req.get().set_buffer_id(buf_id);
                                let mut ev = req.get().init_event();
                                ev.set_type(if is_up { types_capnp::MouseEventType::Up } else { types_capnp::MouseEventType::Down });
                                ev.set_x(px); ev.set_y(py);
                                ev.set_button(match btn {
                                    MouseButton::Left => types_capnp::MouseButton::Left,
                                    MouseButton::Middle => types_capnp::MouseButton::Middle,
                                    MouseButton::Right => types_capnp::MouseButton::Right,
                                });
                                ev.set_modifiers(input::mouse_mods_to_cef(mouse.modifiers));
                                let _ = req.send();
                            }
                            MouseEventKind::Moved | MouseEventKind::Drag(_) => {
                                let mut req = core.send_mouse_event_request();
                                req.get().set_buffer_id(buf_id);
                                let mut ev = req.get().init_event();
                                ev.set_type(types_capnp::MouseEventType::Move);
                                ev.set_x(px); ev.set_y(py);
                                ev.set_button(types_capnp::MouseButton::None);
                                ev.set_modifiers(input::mouse_mods_to_cef(mouse.modifiers));
                                let _ = req.send();
                            }
                            MouseEventKind::ScrollUp | MouseEventKind::ScrollDown
                            | MouseEventKind::ScrollLeft | MouseEventKind::ScrollRight => {
                                let (dx, dy) = match mouse.kind {
                                    MouseEventKind::ScrollUp => (0, 120),
                                    MouseEventKind::ScrollDown => (0, -120),
                                    MouseEventKind::ScrollLeft => (-120, 0),
                                    MouseEventKind::ScrollRight => (120, 0),
                                    _ => (0, 0),
                                };
                                let mut req = core.send_scroll_event_request();
                                req.get().set_buffer_id(buf_id);
                                req.get().set_delta_x(dx);
                                req.get().set_delta_y(dy);
                                let _ = req.send();
                            }
                        }
                    }

                    Event::Resize(new_cols, new_rows) => {
                        if let Ok((new_pw, new_ph)) = input::viewport_pixel_size() {
                            let vp_rect = ui_state.viewport_rect(new_cols, new_rows, new_pw, new_ph);
                            if let Some(ref mut vp) = active_viewport {
                                vp.set_rect(vp_rect);
                            }
                            let buf_id = mux_state.active_buffer_id().unwrap_or(1);
                            let mut req = core.resize_request();
                            req.get().set_buffer_id(buf_id);
                            req.get().set_width(vp_rect.width);
                            req.get().set_height(vp_rect.height);
                            let _ = req.send().promise.await;
                        }
                    }
                    _ => {}
                }
            }
        }
    }

    // --- FPS stats ---
    {
        let elapsed = session_start.elapsed().as_secs_f64();
        let avg_fps = if elapsed > 0.0 { total_frames as f64 / elapsed } else { 0.0 };
        let stats = format!(
            "session: {:.1}s\ntotal_frames: {}\navg_fps: {:.1}\nlast_fps: {}\n",
            elapsed, total_frames, avg_fps, frame_times.len()
        );
        let _ = std::fs::write("/tmp/dirtferret-fps.txt", &stats);
        eprintln!("[tui] {:.1}s, {} frames, avg {:.1} fps", elapsed, total_frames, avg_fps);
    }

    // --- Cleanup ---
    write!(stdout, "\x1b_Ga=d,d=a\x1b\\")?;
    stdout.flush()?;
    drop(active_viewport);
    crossterm::execute!(
        stdout,
        crossterm::event::DisableMouseCapture,
        crossterm::cursor::Show,
        crossterm::terminal::Clear(crossterm::terminal::ClearType::All),
    )?;
    crossterm::terminal::disable_raw_mode()?;
    Ok(())
}
