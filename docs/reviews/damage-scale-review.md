# Static safety review: characters.set_damage_scale (runtime 0.28.0-r32)

Evidence: Ghidra decompile and disassembly of `0x159a60` in Exanima.exe 0.9.5.2g (read-only project), and the
knowledgebase's "Where the damage kinds are still separate". Not run in game.

## Where the kinds are separate

In `0x159a60` (frame on RBP; entry `55 48 89 e5` checked):

| Local | Frame slot | Evidence |
|---|---|---|
| victim character | `[rbp-0x68]` | passed to `0x159700(victim, …)` and as the `0x4f0900` event's `+8` |
| impact energy | `[rbp-0x70]` | divided by the impact protection sum, feeds the balance/health accumulators |
| cut | `[rbp-0x78]` | reduced with the cut protection sum (layer row `+8`) |
| crush | `[rbp-0x80]` | reduced with the crush protection sum (`+C`) |
| point / pierce | `[rbp-0x7c]` | reduced with the pierce protection sum (`+10`); the thrust branch sets it from the weapon's point |

## Hook sites (each an exact 5-byte instruction, a jump target only at its start)

- `0x15b0fc` `f3 0f 10 45 90` (`movss xmm0,[rbp-0x70]`): the join after the armour layer loop, which both the body-hit
  path and the jump that skips the loop reach, before the reduction block. All four kinds are scaled here.
- `0x15b2be` `f3 0f 10 45 84` (`movss xmm0,[rbp-0x7c]`): the thrust branch's join, after it has recomputed point from
  the weapon (three branches jump here). Only point is scaled here; the non-thrust path never passes it.
- No branch in the function targets the four bytes after either site's first byte (checked in the disassembly).
- Next instruction at both: `f3 0f 59 05` (`mulss xmm0,[rip+…]`), also checked.

## The stub

MinHook relocates the one original instruction into its trampoline. The runtime's stub saves RFLAGS, RAX, RCX, RDX,
R8–R11 and XMM0–XMM5, aligns RSP to 16, calls `onKinds(rbp, site)` (a C function, which keeps XMM6–15 by the
Windows x64 ABI), restores everything and jumps to the trampoline. `onKinds` reads the victim from the frame and looks
it up in a published copy of at most 64 entries (seqlock; no lock, no allocation); a miss returns with nothing
changed, so an unset scale is an exact no-op. Faults are caught and counted.

## Targets

At each session pulse, the addons' targets (record ids, or the player) are resolved against that pulse's roster; live
characters only. Several addons' scales on one character multiply. Everything is dropped at a non-session phase.

## Risks left

- A mid-function hook in the game's core damage computation; any change to `0x159a60` disables the feature (all byte
  checks must pass), but a wrong frame slot would mis-scale damage. The slots come from the decompile's data flow,
  cross-checked against the instructions above.
- Thread: `0x159a60` runs on whichever thread resolves contacts; the stub uses no lock and no game state besides the
  frame.
- Not observed in game.
