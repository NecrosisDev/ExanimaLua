local format = require("util.format")
log.info(format.describe("loaded"))
if api.has_capability("items.read") then
    log.info("Item API is available")
end

if api.has_capability("events.lifecycle") then
    events.on("runtime.phase", function(event)
        log.info("Phase:", event.phase, "Generation:", event.generation)
        if api.has_capability("timers") then
            timers.after(0.25, function() log.info("Deferred phase callback") end)
        end
    end)
end

if api.has_capability("storage.settings") then
    local options = storage.get("options", {verbose = false})
    storage.set("options", options)
end
