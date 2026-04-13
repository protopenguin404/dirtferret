-- lua/dirtferret/keymap.lua
-- Default keybindings using action strings.
-- Future: will support function values for context-aware bindings.

-- Ensure keymap table has _maps
if not dirtferret.keymap._maps then
    dirtferret.keymap._maps = {}
end

local function set(mode, key, action)
    dirtferret.keymap._maps[mode] = dirtferret.keymap._maps[mode] or {}
    dirtferret.keymap._maps[mode][key] = action
end

-- Keep the set function available for user config
dirtferret.keymap.set = function(mode, key, action)
    set(mode, key, action)
end

-- Normal mode
set("n", "j", "scroll-down")
set("n", "k", "scroll-up")
set("n", "i", "switch-mode:passthrough")
set("n", "q", "quit")
set("n", "H", "go-back")
set("n", "L", "go-forward")
set("n", "r", "reload")
set("n", "J", "tab-next")
set("n", "K", "tab-prev")
set("n", "\t", "cursor-init")
set("n", ":", "command-enter")
set("n", "o", "command-enter:goto ")
set("n", "/", "command-enter:query ")
set("n", "f", "follow-init")

-- Cursor mode: element navigation with keyboard
set("c", "h", "cursor-left")
set("c", "j", "cursor-down")
set("c", "k", "cursor-up")
set("c", "l", "cursor-right")
set("c", "\t", "cursor-next")
set("c", "\r", "cursor-activate")
set("c", "n", "match-next")
set("c", "N", "match-prev")
set("c", "v", "switch-mode:visual")
set("c", "q", "quit")
set("c", "H", "go-back")
set("c", "L", "go-forward")

-- Visual mode: extend selection with movement
set("v", "h", "visual-left")
set("v", "j", "visual-down")
set("v", "k", "visual-up")
set("v", "l", "visual-right")
set("v", "\t", "visual-next")

-- Passthrough mode
-- (Escape is handled client-side for zero-latency exit)
