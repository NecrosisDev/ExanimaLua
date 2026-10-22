events.on("runtime.phase", function(event)
    if event.phase ~= "session" then return end
    timers.after(3.5, function()
        local snapshot = characters.snapshot()
        if not snapshot then return end
        local staged = 0
        for _, character in ipairs(snapshot.characters) do
            if character.set_ai and character.dead == false and staged < 8 then
                character:set_ai {skill = 40, attribute2 = 0, attribute3 = 0}
                staged = staged + 1
            end
        end
        log.info("Practice AI changes staged:", staged,
            "Check the runtime log for the commit result. Partial:", snapshot.truncated)
    end)
end)
