-- The ring lying in level 1 is added by the runtime when the game reads the level (built by assets.py).
local built, placement = pcall(require, "placement")   -- absent when the script runs without its build (tests)
if built and api.has_capability("levels.serve") then
    levels.add_placement(placement.level, placement.item, placement.position, placement.yaw or 0)
elseif built then
    log.warn("levels.serve is unavailable: the keyring is not placed in level 1 (it still drops as loot)")
end

-- Decorative mesh keys never grant access; only this ring's committed contents count.
events.on("item.use", "keyring_a", function(event)
    if event.target.kind == "inventory" then
        if event.target.item_id == event.item.id then
            event:open_storage()
        end
        return true
    end
    if event.storage_open or event.target.kind ~= "door" or not event.target.locked then
        return true
    end
    if event.target.required_key == event.item.id then
        return true
    end
    for _, key in ipairs(event.item.contents) do
        if key.id == event.target.required_key and key.is_key and not key.removed then
            event:unlock()
            return true
        end
    end
    return false
end)
