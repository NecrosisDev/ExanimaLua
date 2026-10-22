return {
    enabled = false,
    action = "take", -- also ignite, extinguish, unequip_weapon, equip_shield, bag_transfer; see API.md for all actions
    hand = "right", -- right or left, for equip_weapon
    bag_item_id = 0, -- raw carried ID, for bag_store or bag_take
    destination_bag_item_id = 0, -- raw carried ID, for bag_transfer
    container_record = 0,
    item_id = 0,
    world_item_record = 0,
    pressed = false, -- for set_pressed; use/world_use/use_item are also supported actions
    position = 0, -- endpoint 0 or 1 for set_position; world_use toggles a lever
    open = false, -- for set_open, supported unlocked hinged/sliding/paired doors
    active = false, -- for set_active, native electrical emitters
    drop_offset = {50, 0, 0}, -- relative to copied player-sector position
}
