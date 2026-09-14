# Event objects — the hand-enumerated part of the catalogue

Everything else in `catalog/` is extracted from GlowScript's runtime by `tools/catalog_runtime.py`.
Event objects cannot be: `canvas.trigger()` builds them as **ad-hoc object literals**
(`canvas.js:556-590`), so there is no `property.declare()` block, no prototype and no `exports`
entry to read. The rows under owner `event` in `members.csv` are therefore typed in by hand, from
the table below, and every one cites the line that assigns it.

**Re-check this file after any change to `canvas.js`.** Nothing detects drift here automatically —
this is the only part of the catalogue with no extractor behind it.

---

## 1. Where an event comes from

Three ways a program gets one, all producing the same object:

| how | returns | source |
|---|---|---|
| `scene.bind('click', f)` → `f(ev)` | the event | `canvas.js:517` (bind), `597` (the call) |
| `ev = scene.waitfor('click')` | the event | `canvas.js:370-399` |
| `ev = scene.pause()` | a `click` event | `canvas.js:401-455` |

**Widget callbacks are not events.** Every widget hands its own closure object to `bind` —
`attrs.bind(cbutton)` and friends at `primitives.js:2934, 2943, 3149, 3323, 3333, 3392, 3546, 3550,
3667` — so `evt.text`, `evt.checked`, `evt.index`, `evt.selected`, `evt.value`, `evt.disabled` are
the *widget's* own properties, catalogued under `button`, `checkbox`, `radio`, `menu`, `slider`,
`winput`. The docs' "Button/Menu/Checkbox Event Attributes" sections describe that same object.
This is why `winput`'s `ev.text` (used in `Examples__ScrollingText.vpy`) needs no event row.

## 2. The members

Assigned in `canvas.trigger(type, evarg)`, `canvas.js:555-590`. Which fields are present depends
entirely on the event type — there is no common shape beyond `type`/`event`/`canvas`.

| member | present on | meaning | line |
|---|---|---|---|
| `type` | all | the event type string | 556, 559, 580 |
| `event` | all | mouse/key: the same string as `type`. Custom: the raw trigger argument object | 556, 561, 580 |
| `canvas` | all but `textures` | the canvas the event came from | 588 |
| `pageX`, `pageY` | mouse | pixels from the left/top of the **page**, not the canvas | 559 |
| `which` | mouse, key | mouse: always `1`. key: the raw keycode | 559, 580 |
| `pos` | mouse | mouse position in world coordinates, in the plane through `scene.center` parallel to the screen | 562 |
| `press` | mouse | `'left'` on `mousedown`, else `None` | 564, 567, 570, 573, 576 |
| `release` | mouse | `'left'` on `mouseup` and on `click`, else `None` | 565, 568, 571, 574, 577 |
| `key` | key | key name — see §4 | 581 |
| `alt`, `ctrl`, `shift` | key | modifier state when the event fired | 582-584 |

`ev.pos` is computed by `mouse.__update(ev)` (`canvas.js:809-830`), which also refreshes
`scene.mouse.pos` / `.ray` — so a mouse event updates `scene.mouse` as a side effect.

## 3. Event types, and what each carries

From every `trigger()` call site in the runtime:

| type | carries | triggered at |
|---|---|---|
| `mousedown` `mouseup` `mousemove` `mouseenter` `mouseleave` `click` | the mouse fields | `canvas.js:307`, `orbital_camera.js:171, 215, 218, 247, 303, 349, 357, 360`, `WebGLRenderer.js:1712` |
| `keydown` `keyup` | the key fields | `canvas.js:334` |
| `redraw` | `ev.event.dt` — seconds since the last redraw | `WebGLRenderer.js:1680` |
| `draw_complete` | `ev.event.dt` | `WebGLRenderer.js:1706` |
| `textures` | nothing; `ev.event` is `null` and **`ev.canvas` is not set** | `WebGLRenderer.js:435` |
| `resize` | `ev.event` is `{event:'resize'}` | `canvas.js:271` |

Note the asymmetry: for mouse and key events `ev.event` is a *string* (equal to `type`), but for
the four custom events it is the *object* passed to `trigger`. So elapsed time is `ev.event.dt`,
not `ev.dt`.

`click` is synthesised, not native: `orbital_camera.js:216-218` emits it on mouse-up when the
pointer moved ≤ 5 px in x and y. Touch input feeds the same path (`orbital_camera.js:349-360`), so
touch events arrive as ordinary mouse events.

## 4. `ev.key` — the key-name vocabulary

`ev.key` is looked up in one of two keycode tables (`canvas.js:13-61`), chosen by shift state
(`canvas.js:320-322`): `_shifted` when SHIFT is down or shift-lock is on, `_unshifted` otherwise.
`keysdown()` (`canvas.js:6-10`) returns a list of these same names.

- letters `a`–`z`, or `A`–`Z` when shifted
- digits `0`–`9`, and their shifted punctuation
- punctuation `` ` `` `-` `=` `[` `]` `\` `;` `'` `,` `.` `/` and the shifted forms `~ _ + { } | : " < > ?`
- `'\n'` (Enter), `' '` (space), `tab`, `backspace`, `delete`, `insert`, `esc`
- `shift`, `ctrl`, `alt`, `caps lock`
- `left`, `right`, `up`, `down`, `pageup`, `pagedown`, `home`, `end`
- `f1`–`f10`, `break` — **shifted table only** (see §5)

## 5. Discrepancies found while enumerating

Same category as the two already recorded in `METHOD.md` §6: the docs and the runtime disagree, and
the runtime wins. Each is a reading of the source, **not runtime-tested**.

1. **`f1`–`f10` and `break` are unreachable without SHIFT.** `key.html` lists them as ordinary key
   names, but they appear only in `_shifted` (indices 112-121, 19). The matching slots in
   `_unshifted` are `''`, so pressing F1 alone sets `ev.key = ''` and pushes `''` onto the
   `keysdown()` list (`canvas.js:320, 323-326`).
2. **`mouseleave` is undocumented.** The runtime binds and dispatches it (`canvas.js:307`);
   `mouse.html` lists only click/mousedown/mouseup/mousemove/mouseenter.
3. **`ev.press` and `ev.release` are undocumented** — assigned at `canvas.js:564-577`, absent from
   `mouse.html`'s attribute list.
4. **`scene.pause()` appears to leak its binding.** It binds `'click'` (`canvas.js:424, 441`) but
   unbinds `__waitfor` (`canvas.js:452`) — the module-level variable at `canvas.js:65`, which
   `pause` never assigns. (`waitfor`'s reset at `canvas.js:397` writes to its own local shadow,
   declared `var __waitfor` at `canvas.js:374`; the module-level value is `''` from the canvas
   constructor, `canvas.js:217`.) `unbind('')` matches nothing, so the handler stays on
   `canvas.events` after every `pause()`.
5. **The `_shifted` table has apparent typos**: index 35 is `'//'` (two characters), and indices 34
   and 39 are both `'"'`. Programs comparing `ev.key` against shifted punctuation may not get what
   the keyboard says.
6. **`waitfor('textures')` returns an event with no `canvas`** — `trigger` skips the block that
   assigns it when the argument is `null` (`canvas.js:586-590`), which is exactly the
   `textures` case.

## 6. vcpp status

All 13 members are `no-object`: vcpp has no event type at all. `vcpp-input.cppm` has a
`mouse_state` struct (x, y, last_x, last_y, button flags, scroll_delta) and an `input_state`
(modifier flags, `key_down_events`), but these are polled by `process_camera_input()` for camera
control — there is no `bind`, no `waitfor`, no `pause`, and no object handed to a callback. A
transpiler will need this type built before any program that handles input can be translated; it is
the input half of the `scene`/`canvas` gap recorded in `state_of_play.md`.
