mod input;
mod mux;
mod shm;
mod viewport;

pub mod core_capnp {
    include!(concat!(env!("OUT_DIR"), "/core_capnp.rs"));
}
pub mod types_capnp {
    include!(concat!(env!("OUT_DIR"), "/types_capnp.rs"));
}

use base64::Engine as _;
use capnp_rpc::{rpc_twoparty_capnp, twoparty, RpcSystem};
use crossterm::event::{
    Event, EventStream, KeyCode, KeyEventKind, KeyModifiers, MouseButton, MouseEventKind,
};
use futures_util::{AsyncReadExt, StreamExt};
use std::collections::VecDeque;
use std::io::{Seek, SeekFrom, Write};
use std::time::{Duration, Instant};

const IMAGE_ID: u32 = 1;
const DISPLAY_PATH: &str = "/dev/shm/dirtferret-display";

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
    let tiny: [u8; 16] = [
        255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 0, 255,
    ];
    let tiny_b64 = base64::engine::general_purpose::STANDARD.encode(&tiny);
    let esc = format!("\x1b_Ga=T,t=d,f=32,s=2,v=2,i=1;{}\x1b\\", tiny_b64);
    write!(stdout, "\x1b[H")?;
    stdout.write_all(esc.as_bytes())?;
    write!(stdout, "\n\n")?;

    let pattern = viewport::test_pattern(100, 100);
    let escape = viewport::kitty_display_direct(&pattern, 100, 100, 2);
    stdout.write_all(&escape)?;
    write!(stdout, "\n\n")?;

    let escape_file = viewport::kitty_display_file(&pattern, 100, 100, 3)?;
    stdout.write_all(&escape_file)?;
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
    let mut frame: u64 = 0;
    loop {
        frame += 1;
        let escape = viewport::kitty_display_direct(&pattern, 200, 200, IMAGE_ID);
        write!(stdout, "\x1b[H")?;
        stdout.write_all(&escape)?;
        stdout.flush()?;
        eprintln!("[test-loop] Frame {}", frame);
        std::thread::sleep(Duration::from_secs(1));
    }
}

// --- Ui RPC callback (stubs — frame delivery is via shm polling) ---

struct UiImpl;

impl core_capnp::ui::Server for UiImpl {
    fn on_frame(
        &mut self,
        _: core_capnp::ui::OnFrameParams,
        _: core_capnp::ui::OnFrameResults,
    ) -> capnp::capability::Promise<(), capnp::Error> {
        capnp::capability::Promise::ok(())
    }

    fn on_buffer_created(
        &mut self,
        _: core_capnp::ui::OnBufferCreatedParams,
        _: core_capnp::ui::OnBufferCreatedResults,
    ) -> capnp::capability::Promise<(), capnp::Error> {
        capnp::capability::Promise::ok(())
    }
    fn on_buffer_closed(
        &mut self,
        _: core_capnp::ui::OnBufferClosedParams,
        _: core_capnp::ui::OnBufferClosedResults,
    ) -> capnp::capability::Promise<(), capnp::Error> {
        capnp::capability::Promise::ok(())
    }
    fn on_title_changed(
        &mut self,
        _: core_capnp::ui::OnTitleChangedParams,
        _: core_capnp::ui::OnTitleChangedResults,
    ) -> capnp::capability::Promise<(), capnp::Error> {
        capnp::capability::Promise::ok(())
    }
    fn on_url_changed(
        &mut self,
        _: core_capnp::ui::OnUrlChangedParams,
        _: core_capnp::ui::OnUrlChangedResults,
    ) -> capnp::capability::Promise<(), capnp::Error> {
        capnp::capability::Promise::ok(())
    }
    fn on_loading_state_changed(
        &mut self,
        _: core_capnp::ui::OnLoadingStateChangedParams,
        _: core_capnp::ui::OnLoadingStateChangedResults,
    ) -> capnp::capability::Promise<(), capnp::Error> {
        capnp::capability::Promise::ok(())
    }
    fn on_load_progress(
        &mut self,
        _: core_capnp::ui::OnLoadProgressParams,
        _: core_capnp::ui::OnLoadProgressResults,
    ) -> capnp::capability::Promise<(), capnp::Error> {
        capnp::capability::Promise::ok(())
    }
    fn on_focused_field_changed(
        &mut self,
        _: core_capnp::ui::OnFocusedFieldChangedParams,
        _: core_capnp::ui::OnFocusedFieldChangedResults,
    ) -> capnp::capability::Promise<(), capnp::Error> {
        capnp::capability::Promise::ok(())
    }
    fn on_cursor_changed(
        &mut self,
        _: core_capnp::ui::OnCursorChangedParams,
        _: core_capnp::ui::OnCursorChangedResults,
    ) -> capnp::capability::Promise<(), capnp::Error> {
        capnp::capability::Promise::ok(())
    }
    fn on_console_message(
        &mut self,
        _: core_capnp::ui::OnConsoleMessageParams,
        _: core_capnp::ui::OnConsoleMessageResults,
    ) -> capnp::capability::Promise<(), capnp::Error> {
        capnp::capability::Promise::ok(())
    }
}

// --- BGRA→RGBA conversion for a sub-rectangle ---

fn convert_rect_bgra_to_rgba(
    src: &[u8],
    dst: &mut [u8],
    stride: u32,
    x: u32,
    y: u32,
    w: u32,
    h: u32,
) {
    let stride = stride as usize * 4;
    let x = x as usize;
    let w = w as usize;
    for row in y as usize..(y + h) as usize {
        let offset = row * stride + x * 4;
        let end = offset + w * 4;
        if end > src.len() || end > dst.len() {
            break;
        }
        let src_row = &src[offset..end];
        let dst_row = &mut dst[offset..end];
        for chunk in 0..w {
            let i = chunk * 4;
            dst_row[i] = src_row[i + 2]; // R ← B
            dst_row[i + 1] = src_row[i + 1]; // G
            dst_row[i + 2] = src_row[i]; // B ← R
            dst_row[i + 3] = src_row[i + 3]; // A
        }
    }
}

// --- Cached shm state: both buffers mapped, switch by name ---

struct ShmPair {
    reader_0: shm::ShmReader,
    reader_1: shm::ShmReader,
    buffer_id: i32,
    size: usize,
}

impl ShmPair {
    fn open(buffer_id: i32, width: u32, height: u32) -> anyhow::Result<Self> {
        let size = (width as usize) * (height as usize) * 4;
        let name_0 = format!("/dirtferret-{}-frame-0", buffer_id);
        let name_1 = format!("/dirtferret-{}-frame-1", buffer_id);
        Ok(Self {
            reader_0: shm::ShmReader::open(&name_0, size)?,
            reader_1: shm::ShmReader::open(&name_1, size)?,
            buffer_id,
            size,
        })
    }

    fn reader_by_slot(&self, slot: u32) -> &shm::ShmReader {
        if slot == 0 {
            &self.reader_0
        } else {
            &self.reader_1
        }
    }
}

// --- Main ---

async fn async_main() -> anyhow::Result<()> {
    let (pw, ph) = input::viewport_pixel_size()?;
    eprintln!("[tui] Terminal viewport: {}x{} pixels", pw, ph);

    // --- RPC ---
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

    let ui_impl = UiImpl;
    let ui_client: core_capnp::ui::Client = capnp_rpc::new_client(ui_impl);

    let mut req = core.attach_ui_request();
    req.get().set_ui(ui_client);
    req.get().set_width(pw);
    req.get().set_height(ph);
    req.send().promise.await?;

    // --- Render state ---
    let mut shm_pair: Option<ShmPair> = None;
    let mut frame_notify: Option<shm::FrameNotify> = None;
    let mut rgba_buf: Vec<u8> = Vec::new();
    let mut display_file: Option<std::fs::File> = None;
    let mut frame_width: u32 = pw;
    let mut frame_height: u32 = ph;
    let mut active_buffer_id: i32 = 1;
    let mut pending_rects: Vec<[u32; 4]> = Vec::new();
    let mut full_redraw = true;

    // Wait briefly for the core to create the initial buffer and its shm segments.
    tokio::time::sleep(Duration::from_millis(500)).await;

    // Open the notify header for buffer 1
    let notify_name = format!("/dirtferret-{}-notify", active_buffer_id);
    frame_notify = match shm::FrameNotify::open(&notify_name) {
        Ok(n) => {
            eprintln!("[tui] Opened notify shm: {}", notify_name);
            Some(n)
        }
        Err(e) => {
            eprintln!(
                "[tui] Failed to open notify shm {}: {} (will retry on buffer event)",
                notify_name, e
            );
            None
        }
    };

    // Also open the frame pair
    match ShmPair::open(active_buffer_id, pw, ph) {
        Ok(pair) => {
            let size = (pw as usize) * (ph as usize) * 4;
            rgba_buf.resize(size, 0);
            let f = std::fs::File::create(DISPLAY_PATH)?;
            f.set_len(size as u64)?;
            display_file = Some(f);
            shm_pair = Some(pair);
            full_redraw = true;
        }
        Err(e) => {
            eprintln!(
                "[tui] Failed to open frame shm: {} (will retry on first frame)",
                e
            );
        }
    }

    // --- Terminal setup ---
    crossterm::terminal::enable_raw_mode()?;
    let mut stdout = std::io::stdout();
    crossterm::execute!(
        stdout,
        crossterm::event::EnableMouseCapture,
        crossterm::cursor::Hide,
        crossterm::terminal::Clear(crossterm::terminal::ClearType::All),
    )?;
    write!(stdout, "\x1b[H")?;
    stdout.flush()?;

    // Precompute the kitty escape (path never changes, only dims do)
    let path_b64 = base64::engine::general_purpose::STANDARD.encode(DISPLAY_PATH.as_bytes());

    let mut events = EventStream::new();
    let mut render_interval = tokio::time::interval(Duration::from_millis(5));
    render_interval.set_missed_tick_behavior(tokio::time::MissedTickBehavior::Skip);

    // --- FPS tracking ---
    let mut show_fps = false;
    let mut frame_times: VecDeque<Instant> = VecDeque::with_capacity(64);
    let mut total_frames: u64 = 0;
    let session_start = Instant::now();

    loop {
        tokio::select! {
            // --- Render tick: poll shm notify for new frames ---
            _ = render_interval.tick() => {
                let header = frame_notify.as_mut().and_then(|n| n.poll());

                if let Some(header) = &header {
                    let new_size = (header.width as usize) * (header.height as usize) * 4;

                    let need_reopen = match &shm_pair {
                        Some(p) => p.size != new_size,
                        None => true,
                    };
                    if need_reopen {
                        shm_pair = Some(ShmPair::open(
                            active_buffer_id, header.width, header.height,
                        )?);
                        rgba_buf.resize(new_size, 0);
                        let f = std::fs::File::create(DISPLAY_PATH)?;
                        f.set_len(new_size as u64)?;
                        display_file = Some(f);
                        full_redraw = true;
                    }

                    frame_width = header.width;
                    frame_height = header.height;

                    if !full_redraw {
                        pending_rects.extend_from_slice(&header.dirty_rects);
                    }
                }

                if header.is_none() && !full_redraw {
                    continue;
                }

                if let Some(ref pair) = shm_pair {
                    let slot = header.as_ref().map(|h| h.read_slot).unwrap_or(0);
                    let reader = pair.reader_by_slot(slot);
                    let src = reader.as_bytes();

                    if full_redraw || pending_rects.is_empty() {
                        convert_rect_bgra_to_rgba(
                            src, &mut rgba_buf,
                            frame_width, 0, 0, frame_width, frame_height,
                        );
                        full_redraw = false;
                    } else {
                        for rect in &pending_rects {
                            let [x, y, w, h] = *rect;
                            convert_rect_bgra_to_rgba(
                                src, &mut rgba_buf,
                                frame_width, x, y, w, h,
                            );
                        }
                    }
                    pending_rects.clear();

                    if let Some(ref mut f) = display_file {
                        f.seek(SeekFrom::Start(0))?;
                        f.write_all(&rgba_buf)?;
                    }

                    write!(
                        stdout,
                        "\x1b[H\x1b_Ga=T,t=f,f=32,s={},v={},i={},C=1,q=2;{}\x1b\\",
                        frame_width, frame_height, IMAGE_ID, path_b64
                    )?;

                    // Always track frame times for stats on exit
                    total_frames += 1;
                    let now = Instant::now();
                    frame_times.push_back(now);
                    while frame_times.front().is_some_and(|t| now.duration_since(*t) > Duration::from_secs(1)) {
                        frame_times.pop_front();
                    }

                    if show_fps {
                        write!(
                            stdout,
                            "\x1b[1;1H\x1b[48;5;0m\x1b[38;5;46m FPS: {} \x1b[0m",
                            frame_times.len()
                        )?;
                    }

                    stdout.flush()?;
                }
            }

            // --- Terminal events ---
            Some(Ok(event)) = events.next() => {
                match event {
                    Event::Key(key) if key.kind == KeyEventKind::Press => {
                        // TUI keybinds (intercepted, not sent to CEF)
                        if key.modifiers.contains(KeyModifiers::CONTROL) {
                            match key.code {
                                KeyCode::Char('q') => break,
                                KeyCode::Char('d') => {
                                    show_fps = !show_fps;
                                    if !show_fps { full_redraw = true; }
                                    continue;
                                }
                                _ => {}
                            }
                        }

                        let (character, modifiers) = input::key_to_cef(key.code, key.modifiers);
                        if character == 0 { continue; }

                        for key_type in [0u32, 3, 2] {
                            let mut req = core.send_key_event_request();
                            req.get().set_buffer_id(active_buffer_id);
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

                    Event::Mouse(mouse) => {
                        let (px, py) = input::cell_to_pixel(mouse.column, mouse.row);
                        match mouse.kind {
                            MouseEventKind::Down(btn) | MouseEventKind::Up(btn) => {
                                let is_up = matches!(mouse.kind, MouseEventKind::Up(_));
                                let mut req = core.send_mouse_event_request();
                                req.get().set_buffer_id(active_buffer_id);
                                let mut ev = req.get().init_event();
                                ev.set_type(if is_up {
                                    types_capnp::MouseEventType::Up
                                } else {
                                    types_capnp::MouseEventType::Down
                                });
                                ev.set_x(px);
                                ev.set_y(py);
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
                                req.get().set_buffer_id(active_buffer_id);
                                let mut ev = req.get().init_event();
                                ev.set_type(types_capnp::MouseEventType::Move);
                                ev.set_x(px);
                                ev.set_y(py);
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
                                req.get().set_buffer_id(active_buffer_id);
                                req.get().set_delta_x(dx);
                                req.get().set_delta_y(dy);
                                let _ = req.send();
                            }
                        }
                    }

                    Event::Resize(_, _) => {
                        if let Ok((new_pw, new_ph)) = input::viewport_pixel_size() {
                            let mut req = core.resize_request();
                            req.get().set_buffer_id(active_buffer_id);
                            req.get().set_width(new_pw);
                            req.get().set_height(new_ph);
                            let _ = req.send().promise.await;
                        }
                    }
                    _ => {}
                }
            }
        }
    }

    // --- Write FPS stats ---
    {
        let elapsed = session_start.elapsed().as_secs_f64();
        let avg_fps = if elapsed > 0.0 { total_frames as f64 / elapsed } else { 0.0 };
        let current_fps = frame_times.len();
        let stats = format!(
            "session: {:.1}s\ntotal_frames: {}\navg_fps: {:.1}\nlast_fps: {}\n",
            elapsed, total_frames, avg_fps, current_fps
        );
        let _ = std::fs::write("/tmp/dirtferret-fps.txt", &stats);
        eprintln!("[tui] FPS stats written to /tmp/dirtferret-fps.txt");
        eprintln!("[tui] {:.1}s, {} frames, avg {:.1} fps", elapsed, total_frames, avg_fps);
    }

    // --- Cleanup ---
    write!(stdout, "\x1b_Ga=d,d=a\x1b\\")?;
    stdout.flush()?;
    let _ = std::fs::remove_file(DISPLAY_PATH);
    crossterm::execute!(
        stdout,
        crossterm::event::DisableMouseCapture,
        crossterm::cursor::Show,
        crossterm::terminal::Clear(crossterm::terminal::ClearType::All),
    )?;
    crossterm::terminal::disable_raw_mode()?;
    Ok(())
}
