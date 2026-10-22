# Static safety review: title.set_subtitle (runtime 0.28.0-r29)

Evidence: Ghidra decompile of Exanima.exe 0.9.5.2g (read-only project), cross-checked by call-site scans of
the executable. Not run in game.

## Game routines used (called, not hooked)

| RVA | Use | Entry bytes checked |
|---|---|---|
| `0x27080(font)` | makes `font` current: stores it in `[0x49a670]`, binds its texture (`font+0x30`) with `0x214310` | `48 8d 64 24 d8 48 89 c8 48 89 05` |
| `0x27000(text)` | width of an AnsiString in the current font (per-byte advances at `font+0x50`) | `31 c0 48 89 ca 48 85 c9 74 04 48 8b` |
| `0x27180(x, y, text)` | draws an AnsiString in the current font; x and y are floats (XMM0, XMM1) | `53 57 56 41 54 48 8d 64 24 98 66 0f` |
| `0x1f9ad0(r, g, b, a)` | current vertex colour (bytes) | `88 0d` |
| `0x73e90(mode)` | blend mode; the menu draw opens with `0x103` and closes with `0x203` | `48 8d 64 24 d8 89 4c 24 20 89 c8 3b` |
| `0x214310(texture)` | binds a texture; the menu draw binds the title font's `[0x49a6b0]` | `53 48 8d 64 24 e0 48 89 c8 48 3b 05` |

The small font is `*[0x49a8f0]`; 7 of the game's own `0x27080` calls load it with `mov rcx, [rip+…]`.

## What the runtime does

- Only inside its own title-menu draw detour, after the game's draw, on the game's render thread with its GL
  context current, and only while an addon has set a subtitle on one of its rows.
- Position from the same layout the draw used: rows start at `round(screenH * menu+0x200) + [0x49a690]` and
  step `round(menu+0x1fc * menu+0x158)`; each subtitle is centred and drawn 0.72 rows below its row's top.
- Text: a constant AnsiString in the runtime's memory (length at -8, refcount -1), printable ASCII, at most 48
  bytes.
- Afterwards it puts back the previous `[0x49a670]`, rebinds the title font texture, resets the colour to white
  and closes the blend mode with `0x203`, as the menu draw left them.
- Any fault turns the feature off for the session (the draw is wrapped in an exception handler).

## Risks left

- Placement and size are inferred; the second line may touch the next row on small screens.
- Not observed in game.
