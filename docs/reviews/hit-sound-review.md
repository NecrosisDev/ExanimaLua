# Static safety review: characters.set_hit_sound and character_hits (runtime 0.28.0-r34)

Evidence: the knowledgebase's "Hit sounds: flesh, armour and golem", the 0x159a60 decompile, and the runtime's own
combat detour. Not run in game.

## set_hit_sound("player", material)

- Source object: the game's material sound table `0x4f0940` (8 bytes per entry; 6 = HitGlm at `0x4f09a0`). The
  runtime only copies a pointer the game built; it creates no sound object.
- Written while held, at the session pulse on the game thread: the player's wound sound `character+0xec0` (read only
  by 0x159a60) and each body part's impact sound `part+0x470` (parts at `character+0x928`, count `+0x920`, at most
  64). A value that is not ours (the equipment refresh ECA40 rewrote it) is remembered as the latest original before
  ours is put back.
- Restored on release, a phase change or unload, only into the same live player (`[0x4f0830]` unchanged), and only
  where our pointer is still in place. Writes go through a writable-page check and an exception guard.
- Layer-row materials (`+0x14`) are not touched, so armour keeps its own per-layer sounds.

## character_hits

- Recorded inside the existing 0x159700 detour after the original call, when the hit came through the existing
  impact detour (attacker known) and struck a character part (`attacker+0xc20`, part index `+0x468` 0..17).
- Copies only, with the runtime's guarded reader: contact point `attacker+0xc28`, part pose `part+0x2a0` (48 bytes),
  the weapon body's motion from `attacker+0xc38` (`+0xf0` and `+0x120`, 48 bytes each), motion `+0x9b8`.
- No new hook and no write. A 64-entry ring under its own SRW lock; the reader copies records by serial.

## r53b: the part list

The character's body parts are the array at `*(character +0x918)`, count `+0x910` (ExCamera's reviewed poseBuilder
contract, instructions 0x1768f1 / 0x17691c). Earlier builds read `character +0x928` (count `+0x920`), which is the
held-item list (`0x155a20` removes released items from it; the death routine `0x156480` drops every entry), so no
body part was ever found there. Now the parts array is read.
