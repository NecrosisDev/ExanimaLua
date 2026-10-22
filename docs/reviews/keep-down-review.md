# Static safety review: characters.keep_down (runtime 0.28.0-r52)

Evidence: Ghidra decompile of Exanima.exe 0.9.5.2g: the character update `0x169200` (its stand/fall block), the
footing update `0x1603f0`, and a DispScan of every store to `+0x12b0`; constants read from the EXE. Not run in game.

## The game's own rule

- `+0x12b0` is a footing value 0..1. `0x1603f0` rebuilds it every tick from the support and foot contacts and lets it
  recover by `0x31c318` a tick up to 1.0 (`0x31be70`). Other writers: `0x16e1d0`, `0x1700f0`, `0x171cb0` (set it
  outright on state changes).
- In `0x169200`, for a character not ragdolled (`+0xdf4 & 0x800` clear): footing below 0.6 (`0x31c438`) or state
  0x300 enters the block. Footing below 0.3 (`0x31c230`) sets state 0x200 (down) and takes the lying path
  (`0x162c10` / `0x1794f0`). Only with footing 0.3 or more (or `+0x1038` >= 25) is the choice made between the fall
  motions (0x10/0x11) and Get up (0xf/0xe, blend 0x32), after which state becomes 0x300.
- 0x200 is "down"; 0x800 is the limp ragdoll (dead, unconscious): the think loop `0xb4570` returns only on 0x800.

## The runtime

- A MinHook detour on `0x1603f0` (entry bytes `53 57 56 41 54 41 55 48 8d a4 24 c0 fe ff ff 66 0f 7f b4 24` checked
  untouched, identical in Hellmode's EXE) calls the original, then, only for a character in the published hold
  table, caps `+0x12b0` at 0.05 (finite values only, under SEH): below 0.1 even an impact entering the choice picks a fall motion, never Get up. The game then keeps the character down by its own
  branch; nothing else is written. The hold table is a seqlock copy of at most 64 live characters, rebuilt each
  pulse from the addons' targets (dead characters left out); releasing a target drops the cap and the footing
  recovers on the game's schedule.
- Holds end at a phase change (level change, menu) and when the addon unloads.

## Risks left

- A held character's brain still runs while down (state 0x200); what its attacks do from the ground is unknown
  (section 9 of the Wounds requests).
- `+0x1038` >= 25 (an impact) still enters the choice: with footing at 0.05 it plays a fall motion again.
- Not observed in game.
