# Static safety review: characters.play_motion (runtime 0.28.0-r53)

Evidence: Ghidra decompile of Exanima.exe 0.9.5.2g `0x179ee0` (the motion player) and its callers in the character
update `0x169200` (fall 0x10/0x11, get up 0xe/0xf, 3, 0x21). Not run in game.

## The game's motion player

`0x179ee0(character, motion, blend, force)`: unless `motion` is already playing (`+0x9b8`) and `force` is 0, it
stops the current one (`0x179ab0`), reads the motion record `(*(character +0x9b0))[motion]` (no bounds check),
resets the motion progress fields (`+0xa20`, `+0xa28`, `+0xd1c`, `+0x1208`), records previous/current motion
(`+0x9c4`, `+0x9b8`, `+0x9c0`), starts it (`0x1547d0`), sets the blend (`+0xa24` from `blend`) and clears bit
0x10000 on the character's body lists `+0x928` and `+0x978`.

## The runtime

- One staged call per job on the game ring `0x499990`, `0x179ee0(character, motion, 0x1e, 0)` (blend 0x1e as the
  request), under SEH, only after: the character is not ragdolled (`+0xdf4 & 0x800`), its motion table `+0x9b0` is a
  dynamic array whose length (high index at `-8`, at most 4096) covers `motion`, and that entry is a readable record.
- Entry bytes checked: `53 57 56 48 8d 64 24 e0 48 89 cb 89 d6 44 89 c7` (identical in Hellmode's EXE).

## Risks left

- The character update `0x169200` chooses its own motions each tick and may replace one started here (for a held
  down character it plays the lying path).
- A spliced motion's own content (bones, length) is the addon's; a malformed clip is outside this check.
- Not observed in game.
