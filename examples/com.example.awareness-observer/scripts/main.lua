events.on("runtime.phase", function(event)
    if event.phase ~= "session" then return end
    timers.every(2, function()
        local snapshot = characters.awareness()
        if not snapshot then return end
        local fighting, fleeing, remembering = 0, 0, 0
        for _, character in ipairs(snapshot.characters) do
            local brain = character.awareness
            if brain then
                if brain.engagement == "fight" then fighting = fighting + 1 end
                if brain.engagement == "flee" then fleeing = fleeing + 1 end
                if brain.toward_player then remembering = remembering + 1 end
            end
        end
        log.info("NPC awareness:", fighting, "fighting,", fleeing, "fleeing,",
            remembering, "with readable player memories. Partial:", snapshot.truncated)
    end)
end)
