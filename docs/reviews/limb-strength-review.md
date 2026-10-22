# Static safety review: characters.set_limb_strength (runtime 0.28.0-r36)

Evidence: the Wounds thread's decompile notes (E:\Wounds\RUNTIME-REQUESTS.md section 2) and the entry bytes read from
Exanima.exe 0.9.5.2g. Not run in game.

## The game

- `0x15da80(part, pass)`, the joint solver, weights each joint row by `row+0x3c * part+0x594 * sqrt(stamina) *
  character+0x58c`. `part+0x594` is the per-part muscle multiplier, rewritten often by native writers (arm chains
  `0x1688c0`, `0x163cc0`, `0x1726c0`, `0x14e980`, `0x14f250`, `0x14f9a0`, `0x1fed00`).
- Entry `55 48 89 e5 48 8d a4 24 a0 fe ff ff 48 89 9d d0 fe ff ff` (push rbp; mov rbp,rsp; lea rsp; mov [rbp-0x130],rbx),
  no RIP-relative operand in the bytes MinHook relocates. Neither the runtime nor ExCamera hooked it before.
- It recurses into child parts through vtable `+0x168`; each call carries its own part.

## The runtime

- One entry detour (MinHook), made at the first request, only on the bytes above.
- In the detour: when the part has a factor < 1 in the published table, read `v = part+0x594`, write `v * factor`, call
  the original, then write `v` back only if `+0x594` still holds `v * factor` (a native writer that ran meanwhile
  wins). Nothing persists past the call; an unknown part goes straight to the original.
- The table: at each session pulse, each held (record id or player, part 1..18, factor) is resolved against that
  pulse's roster: the character's parts at `+0x928` (count `+0x920`), matched by `part+0x468 + 1`. At most 128
  entries, published as a seqlock copy; the solver may run on the physics worker and takes no lock. Lowest factor wins
  when addons overlap. Dropped at a level change and on unload.

## Risks left

- A part freed between the pulse and a later solve keeps its pointer in the table until the next pulse; the detour
  only compares pointers and touches `+0x594` of the part the game itself is solving, so a stale entry can only match
  a reused allocation for one pulse.
- Not observed in game.

## r53b: the part list

The character's body parts are the array at `*(character +0x918)`, count `+0x910` (ExCamera's reviewed poseBuilder
contract, instructions 0x1768f1 / 0x17691c). Earlier builds read `character +0x928` (count `+0x920`), which is the
held-item list (`0x155a20` removes released items from it; the death routine `0x156480` drops every entry), so no
body part was ever found there. Now the parts array is read.
