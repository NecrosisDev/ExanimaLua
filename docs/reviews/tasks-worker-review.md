# Safety review: tasks.worker (runtime 0.28.0-r41)

Evidence: reading of the new code in `script_host.h` and the offline suite case in `test_timers.cpp`. No game
function, address or memory is involved. Not run in game.

## What runs off the main thread

- Only a new `lua_State` made by the worker itself (`lua_newstate` with its own allocator and memory cap = the
  addon's limit), with base/table/string/math/utf8 minus the same removed globals as the addon sandbox, a `require`
  over copies of the addon's module sources, and `print`/`log` that append to the job's own list.
- No game API, no runtime host object, no addon state, no logger: the only shared data is the `WorkerJob`, whose
  inputs are copied before the thread starts and whose outputs are read by the main thread only after
  `state` (atomic) is 2 or 3.
- The pool (at most 2 threads, more jobs wait) is guarded by its own mutex; threads are detached and end when no
  job waits.

## Crossing data

- Input and result are encoded as plain data (nil, booleans, numbers, strings, tables; 32 deep; input 8 MiB, result
  32 MiB) with the same encoder as addon messages, and decoded into the destination state, so no Lua object is
  shared between states.
- A result too large for the addon's own memory fails decoding and reaches the callback as an error.

## Cancellation

- A count hook every 100,000 instructions raises "worker cancelled" when the job's flag is set; the flag is set by
  `tasks.cancel_worker`, leaving a session for the menu, the addon failing, unloading or being disabled, and the
  runtime stopping. A cancelled job's callback is never called; its thread finishes the current state and closes it.
- The callback runs on the main thread in `runTimers` under the normal callback budget and fault handling.

## Risks left

- A worker that never reaches the hook (a single huge C call such as a string operation) finishes that call first.
- Not observed in game.
