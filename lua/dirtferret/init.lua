-- lua/dirtferret/init.lua
-- Default configuration for dirtferret.
-- Loaded before user config. Everything here is overridable.
--
-- User config: ~/.config/dirtferret/init.lua
-- User plugins: ~/.config/dirtferret/lua/

-- Apply default settings
local defaults = require("dirtferret.defaults")

for key, value in pairs(defaults.settings) do
    dirtferret.opt[key] = value
end

-- Apply default keybindings
require("dirtferret.keymap")

-- Default event handlers (policy)

dirtferret.on("FocusedFieldChanged", function(event)
    if dirtferret.opt.auto_insert_on_focus and event.editable then
        dirtferret.mode.set("insert")
    end
    if dirtferret.opt.restore_normal_on_blur and not event.editable then
        dirtferret.mode.set("normal")
    end
end)

dirtferret.on("BufferCreated", function(event)
    dirtferret.buffer.set_active(event.id)
end)
