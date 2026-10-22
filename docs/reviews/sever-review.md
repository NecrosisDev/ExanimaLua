# Static safety review: characters.sever (runtime 0.28.0-r50)

Evidence: Ghidra decompile of Exanima.exe 0.9.5.2g: the character solver `0x15da80` (joint rows at part `+0x320`,
stride 0x68, count `+0x31c`; row `+0x30` the other body, `+0x38` flags; a row with `0x100000` is skipped: no
constraint, no muscle), ExCamera's reviewed poseBuilder contract (body parts: the array at `*(character +0x918)`,
count `+0x910`; `+0x928` is the held-item list and `+0x9e0` the support legs; part parent `+0x428`), and the Wounds thread's request (E:\Wounds\RUNTIME-REQUESTS.md section 6: `0x89d80` sets the same flag
when an object's joint breaks, `0x152a30` toggles it on grip rows). Not run in game.

## What the runtime writes

- Only bit `0x100000` of a joint row's `+0x38` flags, on the row of the cut part whose `+0x30` is its parent and on
  the parent's row whose `+0x30` is the part. Nothing is freed, moved or re-linked; the solver simply stops pulling
  the two bodies together, the way a broken object joint behaves.
- Only arm parts: `character.hit` parts 12/15 (shoulder) and 13/16 (elbow), found in the character's own part list
  by their index `+0x468`. Legs are refused until `characters.keep_down` exists.
- As a job on the game ring `0x499990` (never inside a pulse), under SEH, after checking again that the part is in
  that character's list and has a parent other than itself; the row arrays are bounds-checked (`count` 1..64, the
  whole array readable).

## A cut arm's hand

- Before the rows are flagged, the job drops what that arm's hand holds (right arm parts 12/13: hand 0, left 15/16:
  hand 1) through the same death-path release as `characters.drop_held` (drop-held-review.md), so no held item stays
  tied by its grip rows to a limb the solver no longer holds.

## Hits on the cut limb

- The part pointer is kept (64 at most; cleared at a level change or load). The impact detour already reads the
  attacker; it now also copies the struck body (`attacker +0xc20`) while any part is cut. In the apply detour
  (`0x159700`), when that body is a cut part or hangs below one (walking `+0x428`, 8 steps at most), the original
  apply is not called: no damage, no stagger, no `character.hit`. The impact's contact sound has already played.

## Risks left

- Falls of the severed limb itself (`0x157ff0`) are not gated yet.
- The pose builder `0x1760f0` still targets the cut part against its parent; with the joint skipped it has no force
  to apply, but the animation may still try to move it.
- A save or reload rebuilds the joints (the flag is not saved); Injuries cuts again at session start.
- Not observed in game.
