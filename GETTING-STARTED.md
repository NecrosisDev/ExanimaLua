# Getting started

A five-minute first mod. The full reference is [API.md](API.md).

## 1. The folder

    Exanima/mods/com.you.hello/
      manifest.toml
      scripts/main.lua

`manifest.toml`:

```toml
[plugin]
name = "Hello"
version = "1.0.0"

[script]
runtime = "com.necrosis.script-runtime"
entry = "scripts/main.lua"
requires = ["events.lifecycle", "timers"]   # the mod does not load without these
optional = ["ui.notify", "ui.settings"]     # used when available
```

Enable `com.necrosis.script-runtime` and `com.you.hello` in your EMTK profile.

## 2. The script

```lua
settings.define("Hello", {
  {key = "greet", label = "Greet on arrival", type = "toggle", default = true},
})

events.on("runtime.phase", function(e)
  if e.phase == "session" and settings.get("greet") then
    timers.after(1, function() ui.notify("Hello from Lua") end)
  end
end)
```

Everything a mod registers (`events.on`, `settings.define`, title rows, level
placements) happens at the top level while the file loads; game reads and
writes happen later, in events and timers.

## 3. Run it

Start the game through EMTK. At the title menu the runtime backs up your saves
and loads the addons; `ScriptRuntime.log` beside the runtime DLL says
`hello: loaded`. Your section appears in Settings > Mod Settings.

## 4. Iterate

Edit the script, return to the title menu and write `com.you.hello` (or `all`)
into `reload.request` beside `ScriptRuntime.dll`. A script error disables only
that addon and shows in the console and the log with its traceback.

## Next

- `examples/`: armour check, zone journal, reward chest, practice AI and more.
- API.md "Reference": every capability, call and event, with its limits.
- Items in levels: `levels.add_placement`; new items: `[script.records]`.
