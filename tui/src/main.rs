mod app;
mod buffer;
mod input;
mod rpc;
mod viewport;

// Cap'n Proto generated code
mod types_capnp {
    include!(concat!(env!("OUT_DIR"), "/types_capnp.rs"));
}
mod core_capnp {
    include!(concat!(env!("OUT_DIR"), "/core_capnp.rs"));
}

use anyhow::Result;
use crossterm::event::EventStream;
use std::time::Duration;
use tokio_stream::StreamExt;

use app::App;

#[tokio::main]
async fn main() -> Result<()> {
    // Initialize terminal
    let mut terminal = ratatui::init();
    let result = run(&mut terminal).await;
    ratatui::restore();
    result
}

async fn run(terminal: &mut ratatui::DefaultTerminal) -> Result<()> {
    let mut app = App::new();
    let fps = Duration::from_secs_f32(1.0 / 60.0);
    let mut render_interval = tokio::time::interval(fps);
    let mut events = EventStream::new();

    // TODO: Connect to dirtferret-core via Cap'n Proto RPC
    // TODO: Call core.attachUi() with our Ui callback capability

    loop {
        tokio::select! {
            // Render at target FPS
            _ = render_interval.tick() => {
                terminal.draw(|frame| app.render(frame))?;
            }

            // Terminal input events
            Some(Ok(event)) = events.next() => {
                app.handle_event(event);
            }

            // TODO: RPC messages from core
            // msg = rpc_receiver.recv() => { app.handle_rpc(msg); }
        }

        if app.should_quit() {
            break;
        }
    }

    Ok(())
}
