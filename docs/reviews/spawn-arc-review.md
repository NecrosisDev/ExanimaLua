# Static safety review: effects.spawn_arc (runtime 0.28.0-r37)

Evidence: Ghidra decompile of the Energy Shock charge `0x28f120` and the arc builder `0x1a7460` in Exanima.exe
0.9.5.2g (read-only project), with the knowledgebase's "Native power visuals". Not run in game.

## The game's own sequence (0x28f120)

1. hand = character `+0xe78`; scene = hand `+0x18` -> `+0x130` -> `+0x220`.
2. A zeroed 0x54-byte spec: `[0]` 0x201, `[1]` strands 20, `[2]` segments 5, `[3]` life 10, `[4]` 2.0, `[5]` 0.25,
   `[6]` 0.25, `[8]` 48.0 (constants read from 0x3510f8, 0x351110, 0x351158).
3. node = `0x1a7460(scene, spec)` (it builds the `_ElecMat01` tendril emitter; flag bit 1 selects the timed life).
4. node `+0x94..+0x9c` = (-15, -5, 0); controller = node `+0x160`; controller `+0x188` = hand, so the arc follows it.
5. `0x5a420(scene, &node, 0)` publishes it. The arc removes itself when its life ends (`0x1a3890` -> `0x1f1bb0`,
   `0x5bed0`).

## The runtime

- The same steps, on the runtime's game-thread job after its play gate, for the controlled player only; strands,
  segments, life and length come from Lua within bounds (1..40, 2..16, 1..200 ticks, 0.5..8); the other words are the
  game's constants. Entry bytes checked: `0x1a7460` `55 48 89 e5 48 8d 64 24 90 48 89 5d`, `0x5a420`
  `55 48 89 e5 48 8d 64 24 a0 48 89 5d`. Every pointer in the scene chain and the node/controller are checked
  before the writes.
- At most 2 requests per addon per pulse, one arc per job.

## Risks left

- `0x1a6e40` recolours the shared `_ElecMat01` material (as the game's own casts do).
- Not observed in game.
