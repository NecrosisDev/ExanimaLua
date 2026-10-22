events.on("runtime.phase", function(event)
    if event.phase ~= "session" then return end
    timers.every(2, function()
        local snapshot = characters.snapshot()
        if not snapshot then return end
        local dead = 0
        for _, character in ipairs(snapshot.characters) do
            if character.dead == true then dead = dead + 1 end
            if character.is_player and character.health and character.health_max
                    and character.health_max > 0 then
                log.info("Player health:", character.health / character.health_max)
            end
        end
        log.info("Character census:", #snapshot.characters, "observed,", dead,
            "dead. Partial:", snapshot.truncated)
    end)
end)
