// tui/src/mode.rs
use crossterm::event::{KeyCode, KeyEvent, KeyModifiers};

/// What the coordinator should do in response to a keypress.
#[derive(Debug, Clone)]
pub enum Action {
    /// Forward this key to CEF (passthrough).
    SendToCef,
    /// Execute a named action with optional argument.
    Execute(String, String),
    /// Switch to a different mode.
    SwitchMode(String),
    /// Normal mode: coordinator should call resolveKeybind RPC.
    ResolveViaRpc,
    /// Key consumed, nothing to do.
    Noop,
}

pub const MODE_NORMAL: &str = "normal";
pub const MODE_PASSTHROUGH: &str = "passthrough";

/// Resolve a keypress in the given mode.
pub fn resolve(mode: &str, key: &KeyEvent) -> Action {
    match mode {
        MODE_PASSTHROUGH => resolve_passthrough(key),
        MODE_NORMAL => resolve_normal(key),
        _ => Action::Noop,
    }
}

fn resolve_passthrough(key: &KeyEvent) -> Action {
    if key.code == KeyCode::Esc && key.modifiers.is_empty() {
        return Action::SwitchMode(MODE_NORMAL.into());
    }
    Action::SendToCef
}

fn resolve_normal(key: &KeyEvent) -> Action {
    // Ctrl+Q is always quit (hardcoded safety hatch)
    if key.modifiers.contains(KeyModifiers::CONTROL) && key.code == KeyCode::Char('q') {
        return Action::Execute("quit".into(), String::new());
    }
    // Everything else goes through Lua via RPC
    Action::ResolveViaRpc
}

#[cfg(test)]
mod tests {
    use super::*;

    fn key(code: KeyCode) -> KeyEvent {
        KeyEvent::new(code, KeyModifiers::empty())
    }

    fn key_with(code: KeyCode, mods: KeyModifiers) -> KeyEvent {
        KeyEvent::new(code, mods)
    }

    #[test]
    fn test_passthrough_escape_exits() {
        match resolve(MODE_PASSTHROUGH, &key(KeyCode::Esc)) {
            Action::SwitchMode(m) => assert_eq!(m, MODE_NORMAL),
            _ => panic!("expected SwitchMode"),
        }
    }

    #[test]
    fn test_passthrough_sends_to_cef() {
        assert!(matches!(resolve(MODE_PASSTHROUGH, &key(KeyCode::Char('a'))), Action::SendToCef));
    }

    #[test]
    fn test_normal_goes_to_rpc() {
        assert!(matches!(resolve(MODE_NORMAL, &key(KeyCode::Char('j'))), Action::ResolveViaRpc));
    }

    #[test]
    fn test_normal_ctrl_q_hardcoded() {
        match resolve(MODE_NORMAL, &key_with(KeyCode::Char('q'), KeyModifiers::CONTROL)) {
            Action::Execute(name, _) => assert_eq!(name, "quit"),
            _ => panic!("expected Execute"),
        }
    }

    #[test]
    fn test_normal_unbound_goes_to_rpc() {
        assert!(matches!(resolve(MODE_NORMAL, &key(KeyCode::Char('z'))), Action::ResolveViaRpc));
    }
}
