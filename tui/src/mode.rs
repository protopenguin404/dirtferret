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
    /// Key consumed, nothing to do.
    Noop,
}

pub const MODE_NORMAL: &str = "normal";
pub const MODE_PASSTHROUGH: &str = "passthrough";

/// Resolve a keypress in the given mode.
/// For now: hardcoded keybinds. Will be replaced with Lua RPC query.
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
    // Ctrl combos
    if key.modifiers.contains(KeyModifiers::CONTROL) {
        return match key.code {
            KeyCode::Char('q') => Action::Execute("quit".into(), String::new()),
            KeyCode::Char('d') => Action::Execute("scroll-half-down".into(), String::new()),
            KeyCode::Char('u') => Action::Execute("scroll-half-up".into(), String::new()),
            _ => Action::Noop,
        };
    }

    match key.code {
        KeyCode::Char('i') => Action::SwitchMode(MODE_PASSTHROUGH.into()),
        KeyCode::Char('j') => Action::Execute("scroll-down".into(), String::new()),
        KeyCode::Char('k') => Action::Execute("scroll-up".into(), String::new()),
        KeyCode::Char('H') => Action::Execute("go-back".into(), String::new()),
        KeyCode::Char('L') => Action::Execute("go-forward".into(), String::new()),
        KeyCode::Char('r') => Action::Execute("reload".into(), String::new()),
        KeyCode::Char('J') => Action::Execute("tab-next".into(), String::new()),
        KeyCode::Char('K') => Action::Execute("tab-prev".into(), String::new()),
        KeyCode::Char('q') => Action::Execute("quit".into(), String::new()),
        _ => Action::Noop,
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
        assert!(matches!(resolve(MODE_PASSTHROUGH, &key(KeyCode::Enter)), Action::SendToCef));
    }

    #[test]
    fn test_normal_mode_switch() {
        match resolve(MODE_NORMAL, &key(KeyCode::Char('i'))) {
            Action::SwitchMode(m) => assert_eq!(m, MODE_PASSTHROUGH),
            _ => panic!("expected SwitchMode"),
        }
    }

    #[test]
    fn test_normal_scroll() {
        match resolve(MODE_NORMAL, &key(KeyCode::Char('j'))) {
            Action::Execute(name, _) => assert_eq!(name, "scroll-down"),
            _ => panic!("expected Execute"),
        }
    }

    #[test]
    fn test_normal_ctrl_q_quits() {
        match resolve(MODE_NORMAL, &key_with(KeyCode::Char('q'), KeyModifiers::CONTROL)) {
            Action::Execute(name, _) => assert_eq!(name, "quit"),
            _ => panic!("expected Execute"),
        }
    }

    #[test]
    fn test_normal_unbound_is_noop() {
        assert!(matches!(resolve(MODE_NORMAL, &key(KeyCode::Char('z'))), Action::Noop));
    }
}
