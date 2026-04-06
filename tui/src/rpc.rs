// RPC layer — Cap'n Proto client and Ui callback implementation.
//
// The TUI connects to dirtferret-core via Cap'n Proto RPC over a Unix socket.
// It receives events from the core via the Ui callback interface and forwards
// them to the app loop as UiEvent values through a tokio channel.

/// Events received from the core via the Ui RPC callback.
/// These are sent through a channel to the main app loop.
#[derive(Debug, Clone)]
pub enum UiEvent {
    /// New frame available in shared memory.
    Frame {
        buffer_id: i32,
        shm_name: String,
        width: u32,
        height: u32,
    },

    /// A buffer was created.
    BufferCreated {
        id: i32,
        url: String,
        title: String,
    },

    /// A buffer was closed.
    BufferClosed {
        buffer_id: i32,
    },

    /// Buffer title changed.
    TitleChanged {
        buffer_id: i32,
        title: String,
    },

    /// Buffer URL changed.
    UrlChanged {
        buffer_id: i32,
        url: String,
    },

    /// Loading state changed.
    LoadingStateChanged {
        buffer_id: i32,
        loading: bool,
        can_go_back: bool,
        can_go_forward: bool,
    },

    /// Load progress updated (0.0 to 1.0).
    LoadProgress {
        buffer_id: i32,
        progress: f64,
    },

    /// A text input field gained or lost focus.
    /// When editable=true, the TUI should enter insert mode.
    FocusedFieldChanged {
        buffer_id: i32,
        editable: bool,
    },

    /// Cursor type changed (pointer, text beam, hand, etc.)
    CursorChanged {
        buffer_id: i32,
        cursor_type: u32,
    },
}

/// Commands sent from the TUI to the core via RPC.
/// These are constructed from user input and sent through the Cap'n Proto client.
#[derive(Debug, Clone, PartialEq)]
pub enum CoreCommand {
    Navigate { buffer_id: i32, url: String },
    GoBack { buffer_id: i32 },
    GoForward { buffer_id: i32 },
    Reload { buffer_id: i32 },
    StopLoad { buffer_id: i32 },
    CreateBuffer { url: String },
    CloseBuffer { buffer_id: i32 },
    SetActiveBuffer { buffer_id: i32 },
    Resize { buffer_id: i32, width: u32, height: u32 },
    SendKey { buffer_id: i32, key_code: u32, character: u32, modifiers: u32 },
    SendMouse { buffer_id: i32, x: i32, y: i32, button: u32, modifiers: u32 },
    SendScroll { buffer_id: i32, delta_x: i32, delta_y: i32 },
}

// TODO: Implement the actual Cap'n Proto RPC client connection
// TODO: Implement the Ui::Server trait for receiving callbacks
// TODO: Wire CoreCommand dispatch to Cap'n Proto method calls

#[cfg(test)]
mod tests {
    use super::*;

    // --- UiEvent processing tests ---
    // These test how the app should handle events from the core.
    // They define the contract for event handling.

    #[test]
    fn ui_event_frame_carries_shm_info() {
        let event = UiEvent::Frame {
            buffer_id: 1,
            shm_name: "/dirtferret-frame-0".to_string(),
            width: 1920,
            height: 1080,
        };
        match event {
            UiEvent::Frame { shm_name, width, height, .. } => {
                assert!(shm_name.starts_with("/dirtferret"));
                assert_eq!(width, 1920);
                assert_eq!(height, 1080);
            }
            _ => panic!("wrong variant"),
        }
    }

    #[test]
    fn ui_event_focused_field_editable() {
        let event = UiEvent::FocusedFieldChanged {
            buffer_id: 1,
            editable: true,
        };
        match event {
            UiEvent::FocusedFieldChanged { editable, .. } => {
                assert!(editable);
            }
            _ => panic!("wrong variant"),
        }
    }

    // --- CoreCommand construction tests ---
    // These verify that user actions produce the right commands.

    #[test]
    fn navigate_command() {
        let cmd = CoreCommand::Navigate {
            buffer_id: 1,
            url: "https://example.com".to_string(),
        };
        assert_eq!(cmd, CoreCommand::Navigate {
            buffer_id: 1,
            url: "https://example.com".to_string(),
        });
    }

    #[test]
    fn create_buffer_command() {
        let cmd = CoreCommand::CreateBuffer {
            url: "https://github.com".to_string(),
        };
        match cmd {
            CoreCommand::CreateBuffer { url } => {
                assert_eq!(url, "https://github.com");
            }
            _ => panic!("wrong variant"),
        }
    }

    #[test]
    fn key_event_command_with_modifiers() {
        let cmd = CoreCommand::SendKey {
            buffer_id: 1,
            key_code: 0x43,   // VK_C
            character: 3,      // Ctrl+C
            modifiers: 2,      // CTRL
        };
        match cmd {
            CoreCommand::SendKey { modifiers, character, .. } => {
                assert_eq!(modifiers, 2);
                assert_eq!(character, 3);
            }
            _ => panic!("wrong variant"),
        }
    }

    #[test]
    fn scroll_command() {
        let cmd = CoreCommand::SendScroll {
            buffer_id: 1,
            delta_x: 0,
            delta_y: -360,  // 3 notches down
        };
        match cmd {
            CoreCommand::SendScroll { delta_y, .. } => {
                assert!(delta_y < 0); // scrolling down
            }
            _ => panic!("wrong variant"),
        }
    }

    // --- Command parsing from user input ---
    // Tests for parsing ":command args" into CoreCommand.

    fn parse_command(input: &str, active_buffer_id: i32) -> Option<CoreCommand> {
        let parts: Vec<&str> = input.splitn(2, ' ').collect();
        match parts[0] {
            "open" | "o" if parts.len() > 1 => {
                let url = parts[1].to_string();
                let url = if !url.contains("://") {
                    format!("https://{}", url)
                } else {
                    url
                };
                Some(CoreCommand::Navigate { buffer_id: active_buffer_id, url })
            }
            "tabopen" | "tabnew" if parts.len() > 1 => {
                let url = parts[1].to_string();
                let url = if !url.contains("://") {
                    format!("https://{}", url)
                } else {
                    url
                };
                Some(CoreCommand::CreateBuffer { url })
            }
            "close" | "bd" => {
                Some(CoreCommand::CloseBuffer { buffer_id: active_buffer_id })
            }
            "back" => Some(CoreCommand::GoBack { buffer_id: active_buffer_id }),
            "forward" => Some(CoreCommand::GoForward { buffer_id: active_buffer_id }),
            "reload" | "r" => Some(CoreCommand::Reload { buffer_id: active_buffer_id }),
            "stop" => Some(CoreCommand::StopLoad { buffer_id: active_buffer_id }),
            _ => None,
        }
    }

    #[test]
    fn parse_open_with_scheme() {
        let cmd = parse_command("open https://example.com", 1).unwrap();
        assert_eq!(cmd, CoreCommand::Navigate {
            buffer_id: 1,
            url: "https://example.com".to_string(),
        });
    }

    #[test]
    fn parse_open_without_scheme() {
        let cmd = parse_command("open example.com", 1).unwrap();
        assert_eq!(cmd, CoreCommand::Navigate {
            buffer_id: 1,
            url: "https://example.com".to_string(),
        });
    }

    #[test]
    fn parse_short_open() {
        let cmd = parse_command("o github.com", 1).unwrap();
        assert_eq!(cmd, CoreCommand::Navigate {
            buffer_id: 1,
            url: "https://github.com".to_string(),
        });
    }

    #[test]
    fn parse_tabopen() {
        let cmd = parse_command("tabopen github.com", 1).unwrap();
        assert_eq!(cmd, CoreCommand::CreateBuffer {
            url: "https://github.com".to_string(),
        });
    }

    #[test]
    fn parse_close() {
        let cmd = parse_command("close", 5).unwrap();
        assert_eq!(cmd, CoreCommand::CloseBuffer { buffer_id: 5 });
    }

    #[test]
    fn parse_navigation() {
        assert_eq!(parse_command("back", 1), Some(CoreCommand::GoBack { buffer_id: 1 }));
        assert_eq!(parse_command("forward", 1), Some(CoreCommand::GoForward { buffer_id: 1 }));
        assert_eq!(parse_command("reload", 1), Some(CoreCommand::Reload { buffer_id: 1 }));
    }

    #[test]
    fn parse_unknown_returns_none() {
        assert!(parse_command("xyzzy", 1).is_none());
    }
}
