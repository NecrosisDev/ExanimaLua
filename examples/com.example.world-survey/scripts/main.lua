events.on("runtime.phase", function(event)
    if event.phase ~= "session" then return end
    timers.after(0.25, function()
        local snapshot = world.snapshot()
        if not snapshot then
            log.warn("Level scene is unavailable")
            return
        end
        local counts = {item = 0, object = 0, light = 0}
        for _, object in ipairs(snapshot.objects) do
            counts[object.kind] = counts[object.kind] + 1
        end
        log.info("Level survey:", counts.item, "items,", counts.object,
            "placed objects,", counts.light, "lights. Partial:", snapshot.truncated)
    end)
end)
