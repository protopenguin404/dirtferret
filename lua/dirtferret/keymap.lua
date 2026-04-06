-- lua/dirtferret/keymap.lua
-- Default keybindings. Override individual bindings in user config.

local keymap = dirtferret.keymap

-- ── Normal mode ──

-- Scrolling
keymap.set("n", "j",     function() dirtferret.scroll.down() end)
keymap.set("n", "k",     function() dirtferret.scroll.up() end)
keymap.set("n", "gg",    function() dirtferret.scroll.top() end)
keymap.set("n", "G",     function() dirtferret.scroll.bottom() end)
keymap.set("n", "<C-d>", function() dirtferret.scroll.half_page_down() end)
keymap.set("n", "<C-u>", function() dirtferret.scroll.half_page_up() end)

-- Mode switching
keymap.set("n", "i",     function() dirtferret.mode.set("insert") end)
keymap.set("n", ":",     function() dirtferret.mode.set("command") end)

-- Navigation
keymap.set("n", "o",     function() dirtferret.command.prompt("open ") end)
keymap.set("n", "O",     function() dirtferret.command.prompt("tabopen ") end)
keymap.set("n", "H",     function() dirtferret.buffer.go_back() end)
keymap.set("n", "L",     function() dirtferret.buffer.go_forward() end)
keymap.set("n", "r",     function() dirtferret.buffer.reload() end)

-- Tabs
keymap.set("n", "gt",    function() dirtferret.buffer.next() end)
keymap.set("n", "gT",    function() dirtferret.buffer.prev() end)
keymap.set("n", "x",     function() dirtferret.buffer.close() end)

-- Clipboard
keymap.set("n", "yy",    function() dirtferret.clipboard.yank_url() end)
keymap.set("n", "p",     function() dirtferret.open(dirtferret.clipboard.get()) end)

-- Find
keymap.set("n", "/",     function() dirtferret.find.prompt() end)
keymap.set("n", "n",     function() dirtferret.find.next() end)
keymap.set("n", "N",     function() dirtferret.find.prev() end)

-- ── Insert mode ──

keymap.set("i", "<Esc>", function() dirtferret.mode.set("normal") end)

-- ── Command mode ──

keymap.set("c", "<Esc>", function() dirtferret.mode.set("normal") end)
keymap.set("c", "<CR>",  function() dirtferret.command.execute() end)
