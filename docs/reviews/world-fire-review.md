# Static safety review: world.light_fire / world.put_out_fire, Energy casts (runtime 0.28.0-r43)

Evidence: the existing reviewed fire paths in this source (`gear.use` ignite_target, the fireplace extinguish in
`world_emitter_bridge.h`) and Fire's RUNTIME-REQUESTS.md (decompile notes of Exanima.exe 0.9.5.2g). Not run in game.

## Lighting

- Game call: `0x1494d0(node, size)` (entry bytes already checked as `availableFire`), which builds the fire through
  the factory `0xf2a70` at the node's position, size u16 x 0.01. The runtime calls it only from a job on the game's
  ring `0x499990` (`nativecalls::enqueue`), never inside the pulse, as ignite_target does.
- Target: the placed node nearest the requested position within 60 cm in the player's sector (a node with an item
  id at `+0x180`, not a fire itself), resolved at the pulse with guarded reads.
- Refused when a fire node (class `0x30e9e8` body pointing back at its node, a free child of the sector) is within
  50 cm, checked at the pulse and again inside the job; at most 24 lit per addon; one job at a time.
- The job compares the sector's fire nodes before and after; exactly one new node is recorded under the addon's
  name, else nothing is recorded.

## Putting out

- `0x5bed0(world, node, 1)` through the world removal queue (entry bytes checked as `availableFlame`), only when
  `removalReady` (the queue at `world+0x3d0..0x3d8` has room), only for a node the job recorded, still a fire child
  of the same sector in the same world.

## Saves

- A fire body is rebuilt at load (`0xf2ed0`), so a saved mod fire would burn for ever. The positions of the lit
  mod fires are kept in the runtime's own save entry (`#exanimalua`, key `#fires`, 12 bytes each). When the world
  changes (a load or level change), that list is read; each fire node within 30 cm of a listed position, in any
  sector of the new world, is removed through the same queue, one per job. A level change simply drops the names.

## Energy casts (power.cast)

- The cast detour on `0x1d8340` already takes Light (0x5NN) and Body (0x3NN) slots. It now also takes Energy
  (0x4NN) slots whose `id & 0xe0fffff` is neither `0x401` nor `0x2040b` (the two rows of the Energy table at
  `0x2f1030`); those two stay the game's. Same slot checks as before; the event's `school` reads "energy".

## Risks left

- A fire rebuilt at a load of a save made by a runtime without this change is not removed (no list).
- A fire node positioned within 30 cm of a listed position that the level itself placed (a hearth next to a
  burning prop) would also be removed after such a load.
- Not observed in game.

## levels.read (r44)

- Reads `<game folder>/<package>` with ordinary file IO (the mods folder's parent; the package name may not hold a
  path separator or a colon), parses the .rpk index (magic `01 0c bf af`, 32-byte rows) and returns one uncompressed
  member, 8 MiB at most. No game memory, no hook. Checked against the installed Resource.rpk (pwr_energy.pwr row:
  offset 0x19968084, size 0x92f, type 0; data at 8 + table length + offset).

## effects.attach_fire, world.fires, fire_nodes (r45)

Evidence: Ghidra decompile of `0xf2a70` (the factory: builds the small fire from preset `0x2b5a70` via `0xf1920`,
tags the controller `+0x158 = 0x3dd0e002`, writes the size at controller `+0x12c`/`+0x118`, builds a light node
and parents it with `0x50170`, returns the fire unattached), `0x149740` (a torch: `0x50170(node, fire)`) and
`0x1494d0` (a placed fire: parent to the world, `0x50eb0`, `0x5c030` to the node's sector, `0x5a420`).

- Attach (a ring job): `fire = 0xf2a70(zero offset, size 0.1..1, world)`; controller `+0x158` cleared so `0xf2ed0`
  never rebuilds it after a load; node `+8 |= 0x9000` as both game callers do; `0x50170(target node, fire)`;
  `0x5c030(fire, target sector)` when its sector differs (entry `53 57 56 41 54 48 8d a4 24 c8 fe ff ff`);
  `0x50eb0`; `0x5a420(world, &fire, 0)`. The target is the player's or a live roster character's scene node
  (character `+0x18`), checked in the same world before the job and again inside it.
- Remove (a ring job): `0x50a60(fire, 1)` only while the fire's parent (`+0x128`) is still that target node; a level
  change forgets them (they go with the scene).
- Scan (every 10 pulses, guarded reads only): the player's sector's fire nodes with tag `0x3dd0e002` and the attached
  flames, size from controller `+0x12c`, sorted by distance; published as a seqlock table of 64 for the
  `fire_nodes` export and kept for `world.fires`.
- Weapon targets are refused: the held-weapon scene node is not traced yet.

## assets.write (r48)

- Ordinary file IO into `mods/<id>/cache/` only: the path is canonicalised and must stay inside that folder (no
  `..` escape), 8 MiB at most, written to `<name>.new` and renamed over the old file. No game memory. Paired with
  the existing `[script.overrides]` (read at addon submission, before the game's first read) it lets an addon serve
  a member it computed last session from the next game start.
