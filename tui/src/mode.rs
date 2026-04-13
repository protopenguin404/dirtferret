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
pub const MODE_CURSOR: &str = "cursor";
pub const MODE_COMMAND: &str = "command";

/// Resolve a keypress in the given mode.
pub fn resolve(mode: &str, key: &KeyEvent) -> Action {
    match mode {
        MODE_PASSTHROUGH => resolve_passthrough(key),
        MODE_NORMAL => resolve_normal(key),
        MODE_CURSOR => resolve_cursor(key),
        MODE_COMMAND => resolve_command(key),
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

fn resolve_cursor(key: &KeyEvent) -> Action {
    // Ctrl+Q is always quit (hardcoded safety hatch)
    if key.modifiers.contains(KeyModifiers::CONTROL) && key.code == KeyCode::Char('q') {
        return Action::Execute("quit".into(), String::new());
    }
    match key.code {
        KeyCode::Esc => Action::Execute("cursor-clear".into(), String::new()),
        // Everything else goes through Lua via RPC (cursor mode = "c")
        _ => Action::ResolveViaRpc,
    }
}

fn resolve_command(key: &KeyEvent) -> Action {
    match key.code {
        KeyCode::Esc => Action::Execute("command-cancel".into(), String::new()),
        KeyCode::Enter => Action::Execute("command-execute".into(), String::new()),
        KeyCode::Backspace => Action::Execute("command-backspace".into(), String::new()),
        KeyCode::Char(c) => Action::Execute("command-char".into(), c.to_string()),
        _ => Action::Noop,
    }
}

/// Map a TUI mode name to the Lua mode abbreviation used in keymap lookups.
/// Lua keymaps use "n" for normal, "c" for cursor, "v" for visual, etc.
pub fn lua_mode_key(mode: &str) -> &str {
    match mode {
        MODE_NORMAL => "n",
        MODE_CURSOR => "c",
        "visual" => "v",
        _ => "n",
    }
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

    #[test]
    fn test_cursor_escape_clears() {
        match resolve(MODE_CURSOR, &key(KeyCode::Esc)) {
            Action::Execute(name, _) => assert_eq!(name, "cursor-clear"),
            _ => panic!("expected Execute(cursor-clear)"),
        }
    }

    #[test]
    fn test_cursor_goes_to_rpc() {
        assert!(matches!(resolve(MODE_CURSOR, &key(KeyCode::Char('j'))), Action::ResolveViaRpc));
    }

    #[test]
    fn test_cursor_ctrl_q_quits() {
        match resolve(MODE_CURSOR, &key_with(KeyCode::Char('q'), KeyModifiers::CONTROL)) {
            Action::Execute(name, _) => assert_eq!(name, "quit"),
            _ => panic!("expected Execute(quit)"),
        }
    }

    #[test]
    fn test_command_escape_cancels() {
        match resolve(MODE_COMMAND, &key(KeyCode::Esc)) {
            Action::Execute(name, _) => assert_eq!(name, "command-cancel"),
            _ => panic!("expected Execute(command-cancel)"),
        }
    }

    #[test]
    fn test_command_enter_executes() {
        match resolve(MODE_COMMAND, &key(KeyCode::Enter)) {
            Action::Execute(name, _) => assert_eq!(name, "command-execute"),
            _ => panic!("expected Execute(command-execute)"),
        }
    }

    #[test]
    fn test_command_char_passes_through() {
        match resolve(MODE_COMMAND, &key(KeyCode::Char('a'))) {
            Action::Execute(name, arg) => {
                assert_eq!(name, "command-char");
                assert_eq!(arg, "a");
            }
            _ => panic!("expected Execute(command-char)"),
        }
    }

    #[test]
    fn test_command_backspace() {
        match resolve(MODE_COMMAND, &key(KeyCode::Backspace)) {
            Action::Execute(name, _) => assert_eq!(name, "command-backspace"),
            _ => panic!("expected Execute(command-backspace)"),
        }
    }

    #[test]
    fn test_lua_mode_key_mapping() {
        assert_eq!(lua_mode_key("normal"), "n");
        assert_eq!(lua_mode_key("cursor"), "c");
        assert_eq!(lua_mode_key("visual"), "v");
        assert_eq!(lua_mode_key("unknown"), "n");
    }
}
