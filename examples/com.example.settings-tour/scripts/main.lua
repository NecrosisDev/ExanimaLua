-- Settings Tour: one row of each kind in Settings > Mod Settings, read at load, followed live with
-- settings.changed, and shown with ui.notify and ui.sound when you enter a level.
local has = api.has_capability

if has("ui.settings") then
    settings.define("Settings Tour", {
        {key = "greet", label = "Greet on arrival", type = "toggle", default = true},
        {key = "delay", label = "Greeting delay (s)", type = "number", default = 2, min = 0, max = 10, step = 0.5},
        {key = "tone", label = "Greeting", type = "choice", default = "Plain", choices = {"Plain", "Grim", "Cheerful"}},
        {key = "volume", label = "Chime volume", type = "number", default = 0.15, min = 0, max = 1, step = 0.05},
        {key = "spare_key", label = "Spare key (unused)", type = "key", default = 0},   -- keys ship unbound
    })
end

local function value(key, default)
    if has("ui.settings") then return settings.get(key) end
    return default
end

local lines = {Plain = "You arrive.", Grim = "The dark waits.", Cheerful = "Off we go!"}

if has("ui.settings") then
    events.on("settings.changed", function(e)
        log.info("setting", e.key, "is now", tostring(e.value))
    end)
end

events.on("runtime.phase", function(e)
    if e.phase ~= "session" or not value("greet", true) then return end
    timers.after(value("delay", 2), function()
        if has("ui.notify") then ui.notify(lines[value("tone", "Plain")] or lines.Plain) end
        if has("ui.sound") then ui.sound("CleanItem01", value("volume", 0.15)) end
    end)
end)

log.info("Settings Tour loaded")
