# Static safety review: powers.wheel (runtime 0.28.0-r30)

Evidence: Ghidra decompile and disassembly of Exanima.exe 0.9.5.2g (read-only project). Not run in game.

## What the game does around a cast (0x1d8340(slot))

- `0x13dcf0(1000)`: `[0x4f0710] = [0x4997d0] + 1000`; keeps the power wheel shown for a second.
- After a successful dispatch, if the byte `[0x56a570]` is set: `0x1d7df0(1)` (rewrites the prepared slots at
  stats `+0x198..+0x1bc` from the slot table), clears the byte, then updates the slot's 20-byte flash row at
  `0x2e8668 + slot*20` (`+0` flags, `+4` start tick, `+8` length):
  - flags & 2: flags = 0xc, start = now, length = 200;
  - otherwise f = float `[0x32d248]` (0.0); flags & 1: f = (now - start) * double `[0x32d2b0]` (0.005);
    else flags & 4: f = double `[0x32d3a0]` (1.0) - (now - start) / length; then flags |= 9 and
    start = now - round(f * float `[0x32d550]` (200.0)) (MULSS, CVTSS2SI in the disassembly).
- A failed dispatch plays `ThaumFail01` through `0xcc810`/`0xcd130`.

## What the runtime does

- `powers.wheel(power, "cast" | "fail")` needs `powers.cast`; only in a session; up to 8 requests per pulse.
- "fail": the sound goes through the runtime's existing ui.sound path (the same two game functions).
- On the game thread (the runtime's work-ring job, after its play gate), for the controlled character's stats
  (`player+0x10a0`): the slot whose `+0x198 + slot*4` value has `& 0x3ffff == power` (slots 0..9). None: nothing.
- `0x13dcf0(1000)` for both outcomes; for "cast", only when `[0x56a570]` is set, `0x1d7df0(1)`, the byte cleared and
  the flash row updated as above, with the constants read from the game.
- Entry bytes checked: `0x13dcf0` `89 c8 03 05`, `0x1d7df0` `8b 05 4a 27 39 00 83 e0 02 83 c8 01`; the table, the
  byte, the tick and the four constants must be readable.

## Risks left

- `0x1d7df0(1)` rewrites the prepared slots as the game does after its own casts; called only when the game's own
  flag asks for it, and only for a power the player has prepared.
- Not observed in game.
