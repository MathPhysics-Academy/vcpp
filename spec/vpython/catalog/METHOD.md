# How the runtime catalogue is built

`catalog/` is a catalogue of Web VPython's API taken from **GlowScript's runtime source**, matched
against vcpp. `parity/` is the older, docs-based table; both are kept on purpose (see §8).

**Auditing this, or deciding whether to trust it: `catalog/AUDIT.md`** — falsification tests, where to
distrust it ranked by risk, and the practical implementation challenges the numbers imply.

**Ground rules**
- **Third-party material is never committed.** GlowScript's source, its docs and the fetched corpora
  live in a separate `vpython-reference/` folder outside every repo. What lives here is only what we
  wrote: these tables, the tools that generate them, and this method. Set `VPYTHON_REF` to that folder
  to regenerate anything (§3).
- Every row cites the runtime file and line it came from (`runtime_source`). If a number looks wrong,
  open that line.
- After any change to the tool, re-run the checks in §7 — including the regression guards. They exist
  because a set-literal edit once silently put `Primitive` back in the owner list, and every content
  check still passed.

## 1. Why the runtime and not the docs

GlowScript's code states its own API exactly: each `lib/glow/*.js` ends with an `exports` block, each
object declares members through `property.declare()`, and inheritance is explicit in `subclass()`.
The docs describe only part of it, often in prose, so a docs-only extractor misses whole areas —
curve's `append`/`modify`/`clear`/`splice`, the generic `rotate`/`clone`, the `shapes.*` library,
`mouse`, `camera`.

Measured: the docs table has 411 (object, attribute) pairs. This one has **1,207 members over 48
owners**, of which **639 user-facing members never appear in the docs table**.

**The reference docs are demonstrably incomplete**, which is the whole reason for preferring the
runtime. Evidence, all from the shipped `docs/VPythonDocs` (byte-identical to glowscript.org's live
docs):
- 639 user-facing runtime members are undocumented — more than half the user-facing API. Whole areas
  are missing: `curve.append/modify/clear/splice`, the generic `rotate`/`clone`, the `shapes.*`
  library, `mouse`, `camera`.
- Undocumented even where a page exists: `box.red/.green/.blue` (real accessors onto `color`'s
  components, `primitives.js:755,762,769`), the event object's `press`/`release`, and the
  `mouseleave` event.
- Documented but wrong: `canvas.visible` has no runtime counterpart; `triangle.bumpmap` is really a
  `texture` sub-key; `key.html` lists `f1`–`f10` as ordinary key names, but they exist only in the
  shifted keycode table, so F1 alone yields `''`.
- **Unfinished pages are shipped live.** `speed.html`'s entire body is "Web VPython nodict / rate() /
  other stuff"; `package.html` reads "thing A / AAAAA / thing B / BBBBB / thing C / CCCCCCC".

This is not a criticism of the docs so much as the reason the docs cannot be the spec. `parity/` keeps
them as an independent cross-check (§8), where they do earn their keep.

One owner — `event` — is the exception to "from the runtime": it is hand-enumerated, because the
runtime builds it as a literal rather than declaring it. See §4g.

## 2. Inputs

| input | location | pinned at |
|---|---|---|
| GlowScript runtime | `glowscript/lib/glow/*.js`, `lib/glow/extrude.js` | glowscript `744de98` |
| compiler built-ins | `glowscript/lib/compiling/GScompiler.js` (`vp_primitives`, `vpwaits`) | same |
| docs (only to decide "documented?") | `glowscript/docs/VPythonDocs/*.html` | same |
| vcpp | this repo's `src/*.cppm`, read-only | record the commit when publishing numbers |
| corpus | `$VPYTHON_REF/corpus/glowscript-official/*.vpy` (34) + `~/workspace/pycode_similar/vpython_codes/**/*.py` (11) | fetched 2026-09-11 |

All four inputs live **outside this repo**, under `$VPYTHON_REF`, except vcpp itself.

## 3. Reproduce

From the root of a vcpp checkout:

```bash
export VPYTHON_REF=~/workspace/math-physics-academy/vpython-reference   # holds glowscript/ + corpus/
python3 spec/vpython/tools/fetch_corpus.py        # only if the corpus is missing; writes under $VPYTHON_REF
python3 spec/vpython/tools/catalog_runtime.py     # rewrites spec/vpython/catalog/*, prints diagnostics
python3 spec/vpython/tools/check_catalog.py       # 36 checks; non-zero exit on any failure
python3 spec/vpython/tools/invariants.py          # convention-free ground truth, for independent reproduction
```

**Inputs and outputs use separate roots.** `VPYTHON_REF` locates the third-party source and is
required — the tools exit with that message if it doesn't point at a `glowscript/` checkout. Output
always lands next to the tools, in `spec/vpython/`, so a run rewrites the committed tables and
`git diff` shows exactly what changed.

`VCPP_SRC` and `VPYTHON_CORPUS` are optional. Without a vcpp checkout the run still succeeds, leaving
the two vcpp columns blank; without the private corpus it warns that usage counts are undercounted.
See `catalog/AUDIT.md` §7.

Python 3 standard library only.

## 4. Extraction rules

### 4a. Reading JavaScript

A string- and comment-aware scanner, not line-anchored regexes:
- `block_end(s, i)` — index past the `}` matching the `{` at `i`.
- `function_body(s, m)` — a function's body by brace matching. **Required**: GlowScript writes both
  `function sphere(args) { return initObject(this, sphere, args) }` on one line and multi-line
  constructors. An earlier line-anchored regex silently missed the one-line form and lost
  `sphere.canvas`.
- `scan_members(s, i)` — top-level members of an object literal: `name: value`, ES6 `get name()` /
  `set name()`, and method shorthand.

### 4b. What each member is

Mirrors what `property.js`'s `declare()` actually does:

| source form | recorded as |
|---|---|
| name starting `__` | skipped (internal) |
| `name: function …` | **method** |
| `{ get: … }` with no `set` | **attribute**, read-only (declare installs a setter that throws) |
| `{ get: … }` whose getter throws | **unsupported** — e.g. `object.x`, which tells the user to write `object.pos.x` |
| `{ value: v, … }` | **attribute**, default `v`, read-only if `readonly` |
| `new attributeVector*(null, a,b,c)` | **attribute**, default `<a,b,c>` |
| anything else | **attribute**, default = the expression |

### 4c. Where members come from

1. **`property.declare(X.prototype, {…})`** — the main source, ~30 blocks.
   `property.declare(canvas, …)` gives canvas's statics. `Mouse.prototype` and
   `orbital_camera.prototype` are recorded under the names a program writes: **`mouse`** and
   **`camera`** (reached via `scene.mouse`, `scene.camera`).
2. **`subclass(child, base)`** — inheritance, resolved base-first so a child's own definition wins.
   `inherited_from` names the declaring class. This is how `cone` gets `radius` from `cylinder`;
   `cone` and `pyramid` have no declare block of their own.
3. **Constructor options** — constructors read `args.X` / `options.X` / `parameters.X` directly. The
   only source for `graph.fast`, `graph.scroll` and the `attach_arrow`/`attach_light` options.
4. **The shared `init()` path** — `init(obj, args)` accepts
   `axis, canvas, color, display, make_trail, radius, size, size_units, up, visible` for every object
   whose constructor visibly calls `init()`/`initObject()`. Membership is read from the code.
5. **Widgets** are closures, not prototypes: options from `var attrs = {…}` and
   `args.X !== undefined`, properties from `get X()`/`set X()`, methods from `X: function`.
6. **`texture` sub-options** — `texture` is set as a dict (`texture={file:…, bumpmap:…, place:…}`);
   those sub-keys are recorded under a pseudo-owner `texture`. The docs instead list `bumpmap` on
   `triangle`.
7. **Namespaces** — `color`, `textures`, `bumpmaps` (object literals); `shapes`, `paths`
   (`shape_object`/`path_object` prototype methods); `vec` (declare block plus
   `vec.prototype.X = function`).
8. **Owners** are every exported constructor plus anything with a declare block, plus the graph kinds
   and `extrusion`. Taking owners from declare blocks alone loses `cone`, `pyramid` and `group`.
   Export blocks are matched as `var|let|const exports = {` — `extrude.js` uses `let`, and matching
   only `var` hid `extrusion`.

**`delete` vs `remove`:** `GScompiler.js` rewrites `.delete` to `.remove`, so the runtime's `remove`
is recorded as VPython's `delete`.

**Two different exclusion lists, deliberately:**
- `INTERNAL_OWNERS` — gets no member rows: internal plumbing, the base classes
  (`Primitive`, `gobject`, `shape_object`, `path_object`), and names that aren't objects at all
  (`keysdown`, `draw`).
- `NOT_A_GLOBAL` — excluded from `globals.csv`: plumbing, plus `fieldset`/`aria_div` (absent from the
  docs) and `ghistogram` (throws "not currently implemented"). `keysdown` and `draw` **are**
  documented and do appear here. Conflating the two lists once dropped both from globals.

### 4d. user_facing

Evidence, not taste: **yes** when the docs mention the name, a corpus program uses it, or it's in
`vp_primitives`. Otherwise **no** — internal helpers such as `shapes.roundc`, `circframe`,
`rackgear`. Helpers are marked, never dropped, and headline counts cover user-facing members only.

### 4e. Matching vcpp

Statuses as in `parity/METHOD.md` — **wired**, **field-only**, **declared-only**, **missing**,
**no-object** — plus:
- methods match member functions of the vcpp struct (with its bases);
- `canvas` matches the vcpp `canvas` class, not a factory; `camera` matches the `camera` struct;
  there is no vcpp `mouse`;
- `color` → `colors::`, `shapes`/`paths` → vcpp `shapes`, `vec` → free functions;
- verified renames (`ALIASES`): `gcurve.graph`→`graph_ref`, `gcurve.dot`→`show_dot`,
  `text.depth`→`thickness`, `curve/points.clear`→`clear_points`;
- `vec` arithmetic maps to C++ operators (`add`→`operator+` …), which lam provides.

### 4f. Usage

Header line stripped, jQuery programs skipped, Python `ast`, per-program type inference from
`x = sphere(...)`. Counts constructor keywords, attribute reads and writes, and `shapes.`/`color.`
member access.

### 4g. Event objects — the one hand-enumerated owner

`canvas.trigger()` builds the object handed to a bound callback, to `waitfor()` and to `pause()` as
ad-hoc object literals (`canvas.js:556-590`). There is no declare block, no prototype and no export,
so no extraction rule above can reach it. Its 13 members are typed into `EVENT_MEMBERS` in
`tools/catalog_runtime.py`, each citing the line that assigns it, and injected as owner `event`.
The full write-up — event types, what each one carries, the `ev.key` vocabulary, and six
docs-vs-runtime discrepancies found while enumerating — is **`catalog/EVENTS.md`**.

**Widget callbacks are not events**, and this is worth stating because the docs imply otherwise.
Every widget passes its own closure object to `bind` (`attrs.bind(cbutton)` and friends,
`primitives.js:2934-3667`), so the docs' "Button Event Attributes" are the widget's own properties,
already catalogued under `button`, `menu`, `checkbox`, `radio`, `slider`, `winput`.

Because `event` is hand-maintained, it is **the one part of the catalogue that can drift silently**
when GlowScript changes. Re-read `canvas.js:556-590` when updating the pinned commit.

## 5. Outputs

| file | contents |
|---|---|
| `members.csv` | one row per (owner, member): kind, default, read-only, note, `runtime_source`, `inherited_from`, documented, user_facing, vcpp status and name, corpus uses, programs |
| `globals.csv` | exported globals: source, whether it's an object, compiler built-in, whether it waits, vcpp presence, corpus calls |
| `SUMMARY.md` | totals, gaps ranked by real usage, per-owner coverage, runtime members absent from the docs, probable helpers |
| `diagnostics.txt` | the shared `init()` option set, which constructors use it, mouse/camera/event member counts — **regenerated every run and not committed** (gitignored); the same content is printed to stdout |
| `EVENTS.md` | hand-written: the event object, event types, the `ev.key` vocabulary, discrepancies (§4g) |

## 6. Known limitations

- **Event objects are hand-enumerated, not extracted** (§4g), so they are the one owner that can go
  stale without any check noticing a *content* change. The count and the citations are guarded
  (§7); the meanings are not.
- **The corpus never reads an event object**, so every `event` row has `corpus_uses=0`. That is a
  real finding, not a gap in the counting: the corpus binds **15 handlers across 9 programs** and
  reads an event attribute in none of them, reaching for `scene.mouse.*` instead (9 uses across 7
  programs — `pos` 5, `pick` 3, `ray` 1). The single `ev.text` in the corpus is a `winput` widget
  callback, correctly counted under `winput`. The counting path is wired up regardless
  (`corpus_usage` types a bound callback's first parameter as an event), so real numbers will appear
  if the corpus grows.
- **Two documented pairs have no runtime counterpart**, both verified by hand: `canvas.visible`
  (documented, but `canvas.js` has no such property and never reads the option) and
  `triangle.bumpmap` (a `texture` sub-key in the runtime). Docs-vs-runtime discrepancies, not
  extraction gaps.
- "wired" means settable by name in vcpp, not that the renderer honours it.
- **The vcpp columns are a snapshot of one commit, and they read the source by regex.** `declared-only`
  comes from matching prop declarations in `vcpp-props.cppm`. vcpp spells the identical declaration
  `symbol pos{}` on `main` and `symbol<> pos{}` on a branch built for an older clang; matching only
  the second moved **96 rows** from `declared-only` to `missing` while all 35 checks then in place
  passed — the 36th exists because of it. Both
  spellings are accepted now and §7 guards that the column is populated, but the vcpp half of every
  table still describes whichever commit `VCPP_SRC` pointed at — record it when publishing numbers.
- Corpus counts are lower bounds: objects held in lists or returned from functions aren't typed.
- `globals.csv`'s vcpp column is a name match only.
- `user_facing=no` is a heuristic; check `runtime_source` before acting on it.
- Defaults are as the runtime declares them; the docs sometimes disagree (e.g. sphere `radius`).

## 7. Checks

They are **executable** — no longer a list to run by hand:

```bash
python3 tools/catalog_runtime.py && python3 tools/check_catalog.py
```

`tools/check_catalog.py` implements everything below and exits non-zero on any failure.

**Regression guards** (these catch structural breakage that content checks miss):

| check | expected |
|---|---|
| internal bases are not owners | no `Primitive`/`gobject`/`shape_object`/`path_object` |
| owner count | 48 |
| member count | 1,150–1,260 (currently 1,207) |
| no row cites a blank line | yes — catches the `^\s*`-under-`re.M` off-by-one that miscited 148 rows |

**Event object** (guards the hand-enumerated table in §4g):

| check | expected |
|---|---|
| `event` is an owner with exactly the 13 enumerated members | yes |
| every `event` row cites `canvas.js` | yes |
| every cited line, opened in `canvas.js`, actually names that member | yes |
| every `event` member is `no-object` in vcpp | yes |
| widget properties (`checked`, `index`, `selected`, `value`, `text`, `disabled`) are not on `event` | yes |

**Content:**

| check | expected |
|---|---|
| curve `append modify clear point slice splice pop shift unshift push` | present, kind=method |
| `curve.npoints` | attribute, note=accessor (not a method) |
| `curve.pos` | kind=unsupported (its getter throws) |
| `cone`, `pyramid`, `group` | >10 members each, via `subclass` |
| `cone.radius` | inherited_from=cylinder |
| `mouse`, `camera` | owners, ≥5 and ≥3 members |
| `label.canvas`, `sphere.canvas` | note="constructor option (shared init)" |
| `texture.bumpmap` | note="texture sub-option" |
| `graph.fast` | note="constructor option" |
| `canvas` | ≥34 members |
| `extrusion`, `keysdown`, `draw` | in globals.csv |
| `fieldset`, `aria_div`, `ghistogram` | not in globals.csv |
| `shapes.roundc` | user_facing=no |
| `vec.add` | vcpp_name=`operator+` |
| `box.size` | field-only |
| `gcurve.graph` | vcpp_name=`graph_ref` |

Plus the cross-check against the docs table: **409 of its 411 pairs are covered**, the two exceptions
being those in §6.

## 8. Relationship to `parity/`

`parity/` (docs-based) is kept, not replaced. It is the better source for *documented* defaults and
prose descriptions, and it is the independent check that this catalogue isn't missing documented API.
That cross-check is what found real bugs here: missing `cone`/`pyramid`/`group`, a lost
`sphere.canvas`, an empty `texture` block and a broken `inherited_from` column. Use `catalog/` as the
API list and `parity/` as the documentation view.
