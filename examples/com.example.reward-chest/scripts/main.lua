local config = require("config")
events.on("runtime.phase", function(event)
    if event.phase ~= "session" or not config.enabled then return end
    timers.after(3.5, function()
        local snapshot = world.snapshot()
        if not snapshot then return end
        for _, object in ipairs(snapshot.objects) do
            if object.kind == "item" and object.item_id == config.container_record
                    and object.add_item then
                local rows = object:contents()
                if not rows then return end
                for _, row in ipairs(rows) do
                    if row.item_id == config.item_id and not row.removed then
                        log.info("Reward already present; nothing staged")
                        return
                    end
                end
                object:add_item(config.item_id)
                log.info("Reward row staged; check the container commit log")
                return
            end
        end
    end)
end)
