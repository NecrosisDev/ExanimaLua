-- Forge Keeper: double-click a weapon, then click a placed object such as a furnace. Cold: it lights a fire
-- there (event:ignite_target). Warm: the light you carry gains a small glow while you stand by it.
-- Shows gear.use target fields, forge lighting, a light glow and, optionally, levels.add_placement.
local config = require("config")
local has = api.has_capability

if config.enabled and config.placement and has("levels.serve") then
    local p = config.placement
    levels.add_placement(p.level, p.item, p.position, p.yaw or 0)
end

local function say(text)
    if has("ui.notify") then ui.notify(text) else log.info(text) end
end

events.on("gear.use", function(event)
    if not config.enabled or event.target.kind ~= "object" then return false end
    if (event.target.distance or 1e9) > 250 then say("Too far away.") return true end
    if not event.target.heat then
        if has("world.devices") and event:ignite_target() then say("You coax the fire alight.") end
        return true
    end
    if has("world.lights") then
        lights.set("forge_glow", {color = {1, 0.55, 0.2}, range = 600, intensity = 1.5,
            follow = {forward = 30, up = 40}, glow = {size = 8}})
    end
    say("The forge is already warm.")
    return true
end)

log.info("Forge Keeper loaded", config.enabled and "(enabled)" or "(disabled in config.lua)")
