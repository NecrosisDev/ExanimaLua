-- Logs what the player's worn armour stops, per body part, from the item database's own numbers.
-- Read-only: it reads the inventory and item stats once per session and changes nothing.
local config = require("config")
local layers = require("layers")
if not config.enabled then return end

local function percent(value) return string.format("%d%%", math.floor(value * 100 + 0.5)) end

local function report()
    local player = inventory.snapshot()
    if not player then
        log.warn("no player inventory yet")
        return
    end
    -- Worn rows are taken in list order as wear order (innermost first); that order is inferred, not traced.
    local pieces = {}
    for _, row in ipairs(player.worn) do
        local stats = items.stats(row.item_id)
        if stats and stats.armour then pieces[#pieces + 1] = stats.armour end
    end
    log.info(#pieces .. " worn armour pieces")
    for _, part in ipairs(config.parts) do
        local outer, under = layers.rows(pieces, part)
        if not outer then
            log.info(part, "bare")
        else
            local e = layers.expected(outer, under or {chance = 0, impact = 0, cut = 0, crush = 0, pierce = 0})
            log.info(part, "outer covers " .. percent(outer.chance), "average stop: cut " .. percent(e.cut),
                "crush " .. percent(e.crush), "pierce " .. percent(e.pierce), string.format("impact %.2f", e.impact))
        end
    end
end

events.on("runtime.phase", function(event)
    if event.phase == "session" then timers.after(1, report) end
end)
