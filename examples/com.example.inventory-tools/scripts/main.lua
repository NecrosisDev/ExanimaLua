local config = require("config")

local function operate()
    local inventory_copy = inventory.snapshot()
    if not inventory_copy then return end
    if config.action == "use" then
        for _, row in ipairs(inventory_copy.carried) do
            if row.item_id == config.item_id and row.use then
                row:use()
                return
            end
        end
        return
    end
    if config.action == "use_item" or config.action == "world_use" or config.action == "set_pressed" or config.action == "set_position" or config.action == "set_open" or config.action == "set_active" or config.action == "extinguish" or config.action == "ignite" then
        if not api.has_capability("world.read") then return end
        local scene = world.snapshot()
        if not scene then return end
        for _, object in ipairs(scene.objects) do
            if object.item_id == config.world_item_record then
                if config.action == "world_use" and object.use then
                    object:use()
                elseif config.action == "extinguish" and object.extinguish then
                    object:extinguish()
                elseif config.action == "ignite" and object.ignite then
                    object:ignite()
                elseif config.action == "set_pressed" and object.set_pressed then
                    object:set_pressed(config.pressed == true)
                elseif config.action == "set_position" and object.set_position then
                    object:set_position(config.position)
                elseif config.action == "set_open" and object.set_open then
                    object:set_open(config.open == true)
                elseif config.action == "set_active" and object.set_active then
                    object:set_active(config.active == true)
                elseif config.action == "use_item" and object.use_item then
                    for _, row in ipairs(inventory_copy.carried) do
                        if row.item_id == config.item_id then
                            object:use_item(row)
                            return
                        end
                    end
                end
                return
            end
        end
        return
    end
    if config.action == "unequip_weapon" then
        local slot = config.hand == "left" and 2 or 1
        local row = inventory_copy.weapons[slot]
        if row and row.unequip then row:unequip() end
        return
    end
    if config.action == "equip_shield" then
        for _, row in ipairs(inventory_copy.carried) do
            if row.item_id == config.item_id and row.equip_shield then
                row:equip_shield()
                return
            end
        end
        return
    end
    if config.action == "bag_transfer" then
        if not api.has_capability("inventory.bags") then return end
        local source, destination
        for _, row in ipairs(inventory_copy.carried) do
            if row.item_id == config.bag_item_id then source = source or row end
            if row.item_id == config.destination_bag_item_id then destination = destination or row end
        end
        if not source or not destination or not source.transfer_to then return end
        local contents = source:contents()
        if not contents then return end
        for index, row in ipairs(contents) do
            if row.item_id == config.item_id and not row.removed then
                source:transfer_to(index, destination)
                return
            end
        end
        return
    end
    if config.action == "equip_weapon" then
        for _, row in ipairs(inventory_copy.carried) do
            if row.item_id == config.item_id and row.equip_weapon then
                row:equip_weapon(config.hand or "right")
                return
            end
        end
        return
    end
    if config.action == "bag_store" or config.action == "bag_take" then
        if not api.has_capability("inventory.bags") then return end
        for _, bag in ipairs(inventory_copy.carried) do
            if bag.item_id == config.bag_item_id and bag.contents then
                local contents = bag:contents()
                if not contents then return end
                if config.action == "bag_take" then
                    for index, row in ipairs(contents) do
                        if row.item_id == config.item_id and not row.removed then
                            bag:take_item(index)
                            return
                        end
                    end
                else
                    for _, row in ipairs(inventory_copy.carried) do
                        if row.item_id == config.item_id and row ~= bag and row.store_in then
                            row:store_in(bag)
                            return
                        end
                    end
                end
                return
            end
        end
        return
    end
    if config.action == "switch" then
        if inventory_copy.switch_weapons then inventory_copy:switch_weapons() end
        return
    end
    if config.action == "drop" then
        local offset = config.drop_offset or {50, 0, 0}
        local p = inventory_copy.position
        for _, row in ipairs(inventory_copy.carried) do
            if row.item_id == config.item_id and row.drop then
                row:drop({p[1] + offset[1], p[2] + offset[2], p[3] + offset[3]})
                return
            end
        end
        return
    end
    if config.action == "equip" or config.action == "unequip" then
        local rows = config.action == "equip" and inventory_copy.carried or inventory_copy.worn
        for _, row in ipairs(rows) do
            if row.item_id == config.item_id and row[config.action] then
                row[config.action](row)
                return
            end
        end
        return
    end
    if not api.has_capability("world.read") then return end
    if config.action == "pickup" then
        if not api.has_capability("world.pickup") then return end
        local snapshot = world.snapshot()
        if not snapshot then return end
        for _, object in ipairs(snapshot.objects) do
            if object.item_id == config.world_item_record and object.pickup then
                object:pickup()
                return
            end
        end
        return
    end
    if not api.has_capability("inventory.transfer") then return end
    local scene = world.snapshot()
    if not scene then return end
    for _, object in ipairs(scene.objects) do
        if object.item_id == config.container_record and object.take_item then
            if config.action == "take" then
                local contents = object:contents()
                if not contents then return end
                for index, row in ipairs(contents) do
                    if row.item_id == config.item_id and not row.removed then
                        object:take_item(index)
                        return
                    end
                end
            elseif config.action == "store" then
                for _, row in ipairs(inventory_copy.carried) do
                    if row.item_id == config.item_id and row.store_in then
                        row:store_in(object)
                        return
                    end
                end
            end
            return
        end
    end
end

events.on("runtime.phase", function(event)
    if event.phase == "session" and config.enabled then
        timers.after(3.5, operate)
    end
end)
