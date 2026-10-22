# Static safety review: characters.set_strength and characters.set_anim_rate (runtime 0.28.0-r36)

Evidence: the knowledgebase's "Strength, speed and native power visuals" (decompile, with a spot check of the +0x998
writers). Not run in game.

## Fields

- `character+0x998` motor strength = 1.25 x body size; written on a character only by setup `0x173230`; read every tick
  (`0x169200` -> `0x1688c0` drives the arm bodies' `+0x594` from it).
- `character+0xdcc` base animation rate (template `+0x5c`, setup `0x173230`; knockdown `0x156b20` lowers it and ramps
  it back to at most 1; player mode `0x125490` writes it). `0x169200` derives the per-tick `+0xdc8` from it; the
  runtime never writes `+0xdc8`.
- Not touched: `+0x410` (body size, also geometry), `+0xfd0` (push cap), `+0x12c0` (shared gait template).

## The runtime

- Player only (`[0x4f0830]`), at the session pulse on the game thread. Each pulse: if the field holds something other
  than the runtime's last write, the game wrote it, and that value becomes the new base; then base x factor is
  written (writable-page check, exception guard).
- Release (factor 1), a phase change or unload writes the latest base back, only into the same live player and only
  where the runtime's value is still in place.
- Factors: strength 0.25..3; animation 0.5..2 for the player, 0.02..2 for other characters (r38); several addons
  multiply.
- r38, animation floor 0.02 for non-player targets (decompile of `0x169200`, `0x156b20`; constants read from the EXE):
  `+0xdcc` is only ever multiplied (`+0xdc8 = +0xdcc x k` at several states; `+0x288 += +0xdc8`); nothing divides by
  it. The knockdown sets it to 0.4 (`0x31c3e0`) and adds 0.01 (`0x31c248`, double) a tick until it reaches 1.0
  (`0x31be70`), then calls the wake-up `0x1660a0`. While a hold is below 1, a small rise (< 0.3) of the animation rate
  is taken as that ramp and added to the base, so the base reaches the game's own value and release restores it; a
  held foe that is down stays down until release, then the game's next ramp step wakes it.

## Risks left

- A knockdown's ramp is followed as a new base each pulse, so the scale stays applied through it.
- Ground speed is not changed (the gait target uses `+0xfb4`, not touched); a real speed-up needs a hook on that
  product, scoped separately.
- Not observed in game.
