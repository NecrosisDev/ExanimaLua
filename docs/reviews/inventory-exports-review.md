# Static safety review: inventory exports, contract 1 (runtime 0.28.0-r39)

Evidence: reading of `inventory_exports.h`, `inventory_bridge.h` and `world_item_bridge.h` in this source, against the
reviews of `row:drop` and `object:pickup` (API 0.13). No new game function, address or memory field. Not run in game.

## What is new

- Five `extern "C"` exports (`inventory_exports_version`, `inventory_list`, `inventory_drop_row`,
  `inventory_pickup_node`, `inventory_request_state`) for another mod's native code (ExCamera VR).
- No new memory operation: a drop or pickup request runs `inventorybridge::commit` with an `InventoryChange` built as
  `row:drop(position)` / `object:pickup()` build it, through the same `worlditems::commit` (the same signature checks
  of 0x102700, 0x11cee0, 0x11d0e0, 0x5a420, 0x117000 and 0x50a60, the same identity, quiet-player, reach, queue,
  tree, holder and fit checks, the same fault path that disables mutations).
- One recorded value: `worlditems::dropped`, the node the drop branch already created and published, kept for the
  caller's result. It is never dereferenced by the runtime after the commit.

## Threads

- Callers run on any thread. `inventory_list` copies a seqlock-published table of at most 256 rows (no lock, no
  allocation, no game memory); a torn read is retried and gives up as -1.
- `inventory_drop_row` / `inventory_pickup_node` only validate their arguments against the published table and queue
  a request (at most 4 waiting, SRW lock); `inventory_request_state` reads one of the last 32 results under the lock.
- Every game read and write happens on the runtime's session pulse (game thread, `pumpPresentation`), after the Lua
  addons' own operations, at most one request per pulse.

## Gates

- Table published only in a session, with the native session active and the inventory bridge possible, and only
  while a caller has used the exports in the last 5 s (no idle cost otherwise); withdrawn (-1) on runtime disable.
- Drop: refused unless drop is possible, the ticket is the current one, the carried rows are unchanged since the
  table was published, and the row is droppable (ordinary item, not worn, not a weapon slot). The position check
  (within 200 cm of the body) is the commit's own.
- Pickup: the node must pass `worlditems::capture` (an eligible loose world item in the player's sector, in reach,
  in the world snapshot, not held by the drag manager or a hand: `body+0x420` and `[0x4f07c0]`).
- A request waits while the player is not quiet (UI, combat, drag, pending action) or another operation ran this
  pulse; after 2 s of waiting it is refused. A fault disables all inventory mutations as for Lua.

## Risks left

- `name` in each row is empty: the runtime has no reviewed item-name read; callers match rows by item id.
- A node passed to `inventory_pickup_node` is only compared and read through the guarded memory reads of
  `worlditems::capture`; a stale pointer is refused, never written.
- Not observed in game.
