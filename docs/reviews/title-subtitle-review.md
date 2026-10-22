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

## r39d: text shader and placement

Hunter saw a pale box over MANUAL (the Endless records line). The menu draw `0x128f30` opens with shader mode
`0x2143b0(2)` (the text program) but, for a hovered row, switches to `0x2143b0(0)` plus the highlight program
`[0x4f02d0]` and leaves it bound; the subtitle glyphs then drew through the highlight program as an opaque quad.
The game's own small-font draws select their shader mode before drawing (e.g. `0x13afa0` calls `0x2143b0(2)`).
Now: the current mode `[0x5b21b0]` is read, `0x2143b0(2)` is called before the text (entry bytes checked:
`48 8d 64 24 d8 89 c8 3b 05`), and the previous mode is passed back afterwards (mode 0 only records itself, as in
the menu draw). The lines are now drawn centred below the menu's last row, stacked half a row apart, instead of
0.72 rows under their own row, where they reached into the next row.

## r39e: placement with its own row

Below the whole menu put the Endless line under EXIT, away from ENDLESS. From the decompile of `0x27700` (title
font) and `0x27180` (current font), a glyph quad spans `y - height .. y`: text is drawn upward from y, so row k's
label ends at its row y and the next label starts at `y + row - [0x49a68c]` (`[0x49a68c]` is the title font's glyph
height; the small font's is `font+0xc`). A subtitle is drawn centred in that gap when the gap is at least its height
plus 2 pixels; otherwise on the same line, 0.3 rows right of its label (the label width is the row record's), and not
at all if that would leave the screen. Both values are read, never written.

## r48b: the GUI font

Hunter's r47 screenshot: the line sat in the right gap but still drew as a pale box of thin strokes with no row
hovered. All the game's text routines (0x27180, 0x27310, 0x27490) lay glyphs out the same way from the current
font's metrics and texture, so the box is untextured glyph quads: the HUD's small font *[0x49a8f0] is not resident
at the title screen. The line now uses the GUI font *[0x49a8e0], which the game's own windows and buttons draw with
at the title screen (0x2f060 selects it before each control's text).
