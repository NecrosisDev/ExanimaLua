-- Read-only: it only listens and reads, so it never changes the game or the save.
local config = require("config")
if not config.enabled then return end

local seen = {hits = 0, with_attacker = 0, downed = 0, script_events = 0, zones = 0, messages = 0}
local function note(...) log.info("self-test", ...) end

local function clock()
    if api.has_capability("world.read") then return tostring(world.time()) end
    return "no world.read"
end

if api.has_capability("events.combat") then
    events.on("character.hit", function(hit)
        seen.hits = seen.hits + 1
        if hit.attacker then seen.with_attacker = seen.with_attacker + 1 end
        if hit.downed then seen.downed = seen.downed + 1 end
        if seen.hits <= 3 then
            note("hit", hit.name, hit.health_damage, hit.balance_damage, hit.source or "no source",
                hit.attacker and hit.attacker.name or "no attacker", hit.downed,
                "weapon", hit.weapon_id or "?", "part", hit.part_name or "?")
        end
    end)
end

if api.has_capability("events.script") then
    events.on("script.event", function(event)
        seen.script_events = seen.script_events + 1
        note("script event", event.name, event.value or "", event.number or "")
    end)
end

if api.has_capability("events.zone") then
    events.on("zone.enter", function(zone)
        seen.zones = seen.zones + 1
        if seen.zones <= 5 then note("zone", zone.zone_id, zone.name, zone.is_player) end
    end)
end

if api.has_capability("events.messages") then
    events.on("game.message", function() seen.messages = seen.messages + 1 end)
end

-- One line on each 0.24.0 read, from a timer in the session (where world reads answer).
local function details()
    note("runtime", api.runtime, "revision", api.revision, "capabilities", table.concat(api.capabilities(), " "))
    if api.has_capability("world.read") and world.level then
        local level = world.level()
        note("level", level and level.name or "unavailable", level and level.mode or "", level and level.loaded)
    end
    if api.has_capability("characters.read") then
        local snapshot = characters.snapshot({details = true})
        local player
        for _, c in ipairs(snapshot and snapshot.characters or {}) do if c.is_player then player = c end end
        if player and player.skills then
            note("player close combat", player.skills.close_combat.rating, "roles", player.roles and #player.roles or "?")
        end
    end
end

events.on("runtime.phase", function(event)
    note("phase", event.phase)
    if event.phase ~= "session" then return end
    -- An optional capability is granted only when its native hook installed on this executable; the
    -- runtime log has one "ready" line per hook for the ones this addon does not declare.
    local granted = {}
    for _, name in ipairs({"events.combat", "events.script", "world.read", "characters.read"}) do
        granted[#granted + 1] = name .. "=" .. tostring(api.has_capability(name))
    end
    note("granted", table.concat(granted, " "))
    note("world time", clock())
    if api.has_capability("characters.read") then
        local characters = characters.snapshot()
        note("characters", characters and #characters.characters or "unavailable")
    end
    timers.after(0, details)
    timers.every(config.every, function()
        note("hits", seen.hits, "with attacker", seen.with_attacker, "downed", seen.downed,
            "script events", seen.script_events, "zones", seen.zones, "game messages", seen.messages,
            "world time", clock())
    end)
end)
