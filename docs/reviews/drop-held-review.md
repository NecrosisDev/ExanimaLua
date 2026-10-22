# Static safety review: characters.drop_held (runtime 0.28.0-r49)

Evidence: Ghidra decompile of Exanima.exe 0.9.5.2g: `0x155a20` (hand release), `0x155bb0` (shield release), their
callers `0x156480` (death / ragdoll: drops every listed weapon) and `0x135d50` (weapon switch), and the DispScan of
every store to character `+0xe88`/`+0xe90`. ExCamera's reviewed held-item contract (heldItems `+0xe88`, written by
`0x155c00`; item owner `+0x580`; node `+0x18` whose `+0x160` is the item). Not run in game.

## The game's own release

- `0x155a20(character, item, hand)`: optional owner callback `+0x1268`; `heldItems[hand] = 0` (and the other hand
  when grip flag `+0x1068 & 0x20`, two hands); grip reset (`+0x1068 = 0`, `+0x1070` default); removes the item from
  the weapon list `+0x928` (count `+0x920`); clears `+0x988` when empty; drops its grip joint (`+0x614`, `+0x31c`);
  `item+0x580 = item` (no owner); `0x1529e0`, `0x1551b0`, `0x155880` give it back its own physics; `0x152100`
  refreshes the character.
- `0x156480` (death) runs, per listed weapon: `item+0xc &= ~0x80000`, the item's release callback `+0x720(item+0x18)`
  when set, then `0x155a20`. The weapon switch `0x135d50` sets `item+8 |= 0x800` (stow) before releasing, which is
  why the runtime follows the death path instead: the item falls loose.

## The runtime

- Lua stages `(target, hand)`; at the session pulse the target is resolved against this pulse's roster (the player,
  a record id, or a live character's handle; dead characters skipped), the item read through the reviewed contract
  (`heldItem`: owner and back-links checked), and one job per pulse is queued on the game ring `0x499990`.
- Inside the job, again: the character is not dying (`+0xdf4 & 0x800`), the item is still in that hand and owned by
  that character, and it is in the weapon list (or is the shield `+0xbf8`, which goes through `0x155bb0`). Then the
  death path's three steps for that one item. The whole job is under SEH.
- Entry bytes checked: `0x155a20` `53 57 56 48 8d 64 24 c0 48 89 cb 48 89 d6 44 89`, `0x155bb0`
  `53 56 48 8d 64 24 d8 48 89 cb 48 c7 83 90 0e 00` (identical in Hellmode's Exanima.exe).

## Risks left

- The character's AI and pose react to an empty hand as after a disarm; an NPC may pick the item up again.
- The `+0x1068 & 0x20` two-hand case clears both hands, as the game does.
- Not observed in game.
