# Static safety review: powers.teach (runtime 0.28.0-r24)

Evidence: Ghidra decompile of Exanima.exe 0.9.5.2g (FocusedExport, read-only project), plus the runtime's
own established offsets. Not run in game.

## The game function

`0x1d3950(stats, id, state)` (prologue `53 57 56 48 8d 64 24 e0 48 89 cb 89 d6 44 89 c7`, checked before use):

1. If `state & 0x2000`, clears the 0x2000 bit of the first known entry that has it.
2. If an entry has `id` exactly, overwrites its state and returns.
3. Otherwise grows the array at `stats+0x190` with the game's allocator `0x10000ee10` (count at
   `stats+0x188`, 16-byte entries `{u32 id, u32 state, u64 0}`) and appends `{id, state, 0}`.

The school wheel (`0x1dd440(wheel, stats)`) marks school `((id & 0xff00) >> 8)` usable for every known
entry, so a known Light power (school 5) opens the Light page.

## What the runtime does

- Lua: `powers.teach(power)` needs `powers.cast`. It accepts only `0 < power <= 0x3ffff` with
  `(power & 0xff00) == 0x500` (Light school), is refused while the addon loads, and returns false outside
  a session. Up to 8 ids per addon are queued per pulse.
- Game thread only: the runtime's existing light/focus job on the game work ring (`0x1a890` →
  `0x499990`), after its play gate (world loaded, state 3, not loading, controlled character present).
- `stats = *(player + 0x10a0)` (the same read the inventory, gear and combat bridges use). The count must be
  0..4096, and the entry array must be readable for `count * 16` bytes.
- A power is skipped when any entry has `(id & 0x3ffff) == power`, so step 2 (overwrite) never runs: progress
  is never reset. The state passed is `0x1000` (no `0x2000` bit), so step 1 never runs either.
- After each append the count and array are re-read, because the array may have moved.

## Risks left

- Saves: the entry is saved with the character (actor chunk 0xcdec01/02). It is a permanent grant for
  that character. The runtime's save backup runs at the settled title menu, before any session.
- Unverified in game: that state `0x1000` behaves for a Light power as it does for the seeded Sense Minds.
