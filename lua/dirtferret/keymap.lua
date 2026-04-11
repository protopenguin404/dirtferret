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

-- Passthrough mode
-- (Escape is handled client-side for zero-latency exit)
