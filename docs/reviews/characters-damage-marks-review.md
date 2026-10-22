# Static safety review: character:damage, powers.restore_focus, characters.set_mark (runtime 0.28.0-r31)

Evidence: the runtime's existing, reviewed paths and static reading of Exanima.exe 0.9.5.2g. Not run in game.

## character:damage

- The same game call as `player.damage`: the damage applier `0x159700(character, balance, health, can_fall)`
  (entry bytes checked at startup), run on the presentation pulse (the game's window and flow thread) after the
  Lua callback returns, and only after the session's save backup.
- The method exists only on `characters.read()` entries of the same callback (an execution stamp is checked), for
  characters that are not the player. At commit the target pointer must be in this pulse's roster, not the player
  and not dead; otherwise nothing is called.
- Health and balance are each 0..1 per call, and at most 4 staged game calls per callback (all kinds).
- Goes through the runtime's combat detour, so `character.hit` reports it.

## powers.restore_focus

- On the runtime's game-thread job, after its play gate: the controlled character's current focus `+0xd74` is raised
  by at most 0.5, capped at the ceiling `+0xd78` less what is held back `+0xd84`. Plain float writes to the fields
  the runtime's focus hold already writes.

## characters.set_mark / character_mark_nodes

- Lua only stores marks in the addon; nothing is written to the game. At each session pulse the runtime resolves them
  against this pulse's roster (record id or the player, alive) and reads each character's scene node at `+0x18`
  with its guarded memory reader.
- Published as a seqlock copy of at most 64 entries. `character_mark_nodes` takes no lock, reads no game memory and
  allocates nothing; a reader that races a refresh retries up to 8 times and otherwise returns 0.
- Marks are dropped at every non-session phase (a level change passes through loading) and with the addon.
