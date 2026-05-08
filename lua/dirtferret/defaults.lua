-- lua/dirtferret/defaults.lua
-- Default settings. Override in ~/.config/dirtferret/init.lua.

local M = {}

M.settings = {
    -- Display
    frame_rate = 60,
    scroll_lines = 3,

    -- Behavior
    auto_insert_on_focus = true,
    restore_normal_on_blur = true,
    confirm_close_last = true,

    -- URLs
    home_page = "about:blank",
    search_engine = "https://duckduckgo.com/?q=%s",

    -- Ad blocking
    adblock_enabled = true,
    adblock_lists = {
        "https://easylist.to/easylist/easylist.txt",
    },
}

return M
