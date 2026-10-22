events.on("runtime.phase", function(event)
    if event.phase ~= "session" then return end
    timers.after(3.5, function()
        local snapshot = characters.awareness()
        if not snapshot then return end
        local staged = 0
        for _, character in ipairs(snapshot.characters) do
            local brain = character.awareness
            if character.set_attitude and character.dead == false and brain
                    and brain.toward_player and staged < 8 then
                character:set_attitude {hostility = 0, concern = 0}
                staged = staged + 1
            end
        end
        log.info("Encounter Truce attitude changes staged:", staged,
            "Check the commit log. NPCs can react again after this one-time edit.")
    end)
end)
