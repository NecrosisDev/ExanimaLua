-- A first-visit journal for zone triggers. The game fires a zone on every entry, by any character, with no
-- "once" flag; this keeps the "once" itself. A level's zone scripts can also call LuaEvent(zone, ZoneID);
-- with events.script those arrive here too, with the zone's number in event.number.
local config = require("config")
if not config.enabled then return end

local visited = {}   -- ["exanima01:3"] = true
local loaded = false

local function level_name()
    if api.has_capability("world.read") then
        local level = world.level()
        if level then return level.name end
    end
    return "level"
end

-- save_data holds "exanima01:3,exanima01:4,..." when storage.save is granted.
local function restore()
    visited, loaded = {}, true
    if not api.has_capability("storage.save") then return end
    for key in string.gmatch(save_data.get() or "", "[^,]+") do visited[key] = true end
end

local function remember(key)
    visited[key] = true
    if not api.has_capability("storage.save") then return end
    local keys = {}
    for k in pairs(visited) do keys[#keys + 1] = k end
    table.sort(keys)
    local text = table.concat(keys, ",")
    if #text <= 4096 then save_data.set(text) end
end

local function entered(zone_id, who)
    if not loaded then restore() end
    local key = level_name() .. ":" .. zone_id
    if visited[key] then return end
    remember(key)
    log.info("new zone", key, "entered by", who)
end

events.on("runtime.phase", function(event)
    if event.phase ~= "session" then loaded = false end   -- reread the save's list after the next load
end)

events.on("zone.enter", function(zone)
    if zone.is_player then entered(zone.zone_id, zone.name) end
end)

if api.has_capability("events.script") then
    events.on("script.event", function(event)
        if event.name == "zone" and event.number then entered(event.number, "script") end
    end)
end
