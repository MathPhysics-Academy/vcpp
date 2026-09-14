# How to audit this catalogue without trusting whoever built it

`METHOD.md` says how the catalogue is built. This file says how to **check that it is true**, and what
it costs to implement. It assumes you trust nothing here and want to falsify it cheaply.

Every claim below is checkable with a command. Where something is *not* checkable, it says so.

---

## 1. The thirty-second version

```bash
export VPYTHON_REF=~/workspace/math-physics-academy/vpython-reference   # the glowscript clone; not in this repo
cd <your vcpp checkout>
python3 spec/vpython/tools/catalog_runtime.py && python3 spec/vpython/tools/check_catalog.py
```

Expected: `36/36 checks passed`, `owners: 48   members: 1207   globals: 98`, exit status 0.

If those numbers differ from what a document claims, **the document is wrong, not the tool**. The CSVs
are generated; regenerate before believing any prose.

## 2. Why you should believe the shape of it

Three structural arguments, in descending strength.

### 2.1 Two independent extractions agree
`catalog/` is built from GlowScript's **runtime JavaScript**. `parity/` is built from its **HTML
documentation**. Different sources, different code, written to disagree. The cross-check:
**409 of the docs table's 411 (object, attribute) pairs appear in the runtime catalogue.**

Verify the check is not circular — `parity.py` must not read `catalog/`:
```bash
grep -n "catalog" tools/parity.py          # expect: no hits
```

The two exceptions are hand-verified docs-vs-runtime discrepancies, not misses: `canvas.visible`
(documented, absent from `canvas.js`) and `triangle.bumpmap` (really a `texture` sub-key).

This cross-check is what caught four real extractor bugs historically: missing `cone`/`pyramid`/
`group`, a lost `sphere.canvas`, an empty `texture` block, and a broken `inherited_from` column.

### 2.2 Every row cites its source line
Sample rows at random and open what they cite. This is the single highest-value audit move:
```bash
python3 - <<'PY'
import csv, random
rows = list(csv.DictReader(open('catalog/members.csv')))
for r in random.sample(rows, 15):
    print(f"{r['owner']}.{r['member']:20} {r['kind']:11} {r['runtime_source']}")
PY
```
Then open each `file:line` under `glowscript/lib/glow/`. A citation that doesn't show the member is a
defect. (Two such defects existed in the `event` rows and were only found by a human asking; see §4.1.)

### 2.3 The checks include structural guards, not just content
Content checks pass happily while the output is structurally broken — that actually happened: an edit
moved four base classes into the owner list, owners went 47→49 and members 1,194→1,241, and **every
content check still passed**. Hence the guards on owner count, member count, and internal bases.
If you change the tool, those guards are what tell you that you broke it.

## 3. Falsification tests — run these before believing anything

### 3.1 Recall: does it miss documented API?
```bash
python3 - <<'PY'
import csv, random
docs = {(r['object'], r['attr']) for r in csv.DictReader(open('parity/objects.csv'))}
have = {(r['owner'], r['member']) for r in csv.DictReader(open('catalog/members.csv'))}
missing = sorted(docs - have)
print(f"documented pairs: {len(docs)}   missing from catalogue: {len(missing)}")
print(missing)
PY
```
Expect exactly the two known exceptions. **Anything else is a real miss.**

### 3.2 Precision: does it invent API?
Sample 15 rows (§2.2) and confirm each exists in the runtime. A row whose cited line doesn't define it
is an invention.

### 3.3 End-to-end: take a real program
Pick any file in `corpus/glowscript-official/`, list every constructor keyword and attribute it
touches, and confirm each appears in `members.csv` for the right owner. Anything a working program
uses that the catalogue lacks is a hole.

### 3.4 Determinism
```bash
cp catalog/members.csv /tmp/a.csv && python3 tools/catalog_runtime.py && diff /tmp/a.csv catalog/members.csv
```
Expect no output. A diff means the extraction is order-dependent — a defect in itself.

## 4. Where to distrust it, ranked

### 4.1 `event` — hand-typed, no extractor (highest risk)
`canvas.trigger()` builds event objects as literals, so nothing generates these 13 rows; they were
read off `canvas.js:556-590` by a human. **This is the only owner that cannot re-derive itself**, so
it is the only one that goes stale silently when GlowScript changes.

Two of its citations were wrong when first written (off by one line) and every check still passed,
because the check tested that a citation *started with* `canvas.js:` rather than what it pointed at.
The check now opens every cited line and requires the member's name on it. Treat this as a warning
about the whole class: **a guard proves only exactly what it tests.**

### 4.2 The `documented` column is a global text match
It greps all docs HTML for the member name with word boundaries — not per-owner, not per-context. So
`event.press` and `event.release` show `documented=yes` while being absent from `mouse.html`'s
attribute list. **Do not read this column as "documented as an attribute of this owner."**

### 4.3 `user_facing` is a heuristic
Documented, or used by a corpus program, or a compiler built-in. A real API that is undocumented and
unused by these 45 programs is misfiled as an internal helper. It is *marked*, never dropped — check
`runtime_source` before acting.

### 4.4 The vcpp columns are regex, not compilation
`vcpp_status` comes from regexes over `.cppm` text — struct bodies, `param_spec` entries, `is_bound`
calls. It does not parse C++. An unusual formatting or an unanticipated template form reads as a false
"missing". A clang AST dump would be authoritative; it is not used.

Also: **"wired" means settable by name, not that the renderer honours the value.** Nothing here tests
rendering.

### 4.5 `globals.csv` is derived from `exports` blocks — and that rule is incomplete
Confirmed gap: `input` and `round` are **injected by the compiler as generated source**
(`GScompiler.js:1254` and `:1268`), never appear in an `exports` block, and are therefore **missing
from the 98 globals**. Any other compiler-injected name would be missed the same way.

### 4.6 Corpus counts are lower bounds
Type inference is `x = sphere(...)` only (plus, for events, a bound callback's first parameter).
Objects held in lists or returned from functions are untyped, so their uses go uncounted. `0` in
`corpus_uses` means "not counted", never "unused".

### 4.7 Defaults follow the runtime
Where the docs state a different default (e.g. sphere `radius`), the runtime wins. If you are
implementing to the docs, you will disagree with this table on purpose.

## 5. What the catalogue does *not* cover

- **The Python language itself.** This catalogues VPython's API. Web VPython also implements a
  *subset of Python* via RapydScript-NG 0.7.22, which is a separate surface and is **not catalogued**
  (measured but not written down as of 2026-09-12). Verified absent from that subset:
  `sum sorted tuple zip map filter any all`, plus `lambda`, `while/else`, `for/else`.
- **Runtime behaviour.** Nothing here executes GlowScript. Every "unsupported"/"throws"/"leaks"
  finding is a reading of source code.
- **Rendering correctness.** See §4.4.

## 6. Practical implementation challenges

What the numbers imply for actually building this in vcpp. Current state: **297 of 1,061 user-facing
members wired (28%)**; of the 237 members real programs use, **128 wired (54%)**.

### 6.1 The silent-no-op trap — the most dangerous category
48 members are **field-only**: vcpp *has* the field, but the constructor ignores the keyword. So
`box(size=vec(2,1,1))` compiles, runs, and silently does nothing. This fails the worst way possible —
it looks like it works. `box.size` alone appears 11 programs / 40 uses. Any parity metric that counts
"has a field" as support will overstate readiness.

### 6.2 Viral async
`rate()`, `sleep()`, `scene.pause()`, `scene.waitfor()`, `scene.capture()`, `input()`, `winput()`,
`get_library()`, `read_local_file()` all suspend. GlowScript solves this by transpiling the whole
program into an `async function` and inserting `await`, then propagating `async` to any user function
that transitively waits. vcpp's equivalent is C++20 coroutines plus Asyncify — and the transpiler must
perform the same transitive analysis (`vpfcts`/`vpwaits` in `GScompiler.js`). **This is a whole-program
pass, not a local translation.**

### 6.3 Dynamic typing meeting a static target
`texture=textures.wood` passes a name where vcpp wants a handle; `pos` accepts vectors and lists;
`color` accepts named constants and vectors. Each polymorphic parameter is a design decision, not a
mapping.

### 6.4 Python runtime semantics in C++
Programs rely on Python `list`/`dict`/`set` semantics, negative indexing, slicing, string methods and
`.format`. RapydScript ships ~90 runtime helpers to provide these in JavaScript. A C++ target needs the
equivalent, or the transpiler must restrict the accepted subset and say so.

### 6.5 Assignment is observable
Setting `obj.pos` must trigger a re-render; VPython attributes are properties with side effects, not
plain fields. Plain C++ members will not do.

### 6.6 Whole categories with no vcpp object at all
215 members are `no-object`: all seven widgets (`button`, `slider`, `menu`, `checkbox`, `radio`,
`winput`, `wtext`), `event`, `mouse`, `vertex`, `group`, `distant_light`. Widgets are DOM elements in
the browser; a native target needs a UI story before they exist at all.

### 6.7 Input is polled, not delivered
vcpp's `vcpp-input.cppm` polls a `mouse_state` for camera control. VPython delivers events to bound
callbacks and blocks in `waitfor`/`pause`. Notably the corpus leans on `scene.mouse.*` (9 uses) more
than on event attributes (0), so `scene.mouse` may be the cheaper first target.

### 6.8 Errors must match
Some members exist specifically to throw — `curve.pos`'s getter tells you to use `curve.point()`.
65 members are `kind=unsupported`. Silently succeeding where VPython raises is its own divergence.

### 6.9 You cannot implement from the documentation
639 user-facing members never appear in the docs table — `curve.append/modify/clear/splice`, the
generic `rotate`/`clone`, the whole `shapes.*` library. Docs-driven implementation will miss more than
half the surface.

### 6.10 Priority is measurable, so use it
Sort by real usage rather than by object: `color.gray` (14 programs), `box.size` (11),
`button.text`/`bind` (6), `extrusion.path`/`shape` (5), `text.text` (5), `sphere.trail_type` (5),
named `textures.*` (5). `SUMMARY.md` regenerates this ranking on every run.

## 7. Reproducing it independently

Three levels, from strictest to most useful.

### 7.1 Re-run our tools (bit-exact)
```bash
python3 tools/catalog_runtime.py && python3 tools/check_catalog.py
```
Verified byte-identical across reruns (§3.4). This proves the tool is deterministic; it proves
nothing about whether the tool is *right*.

### 7.2 Run our tools on your own machine
Output always resolves from the tool's own location (`spec/vpython/`); the inputs are overridable:

| variable | default | if missing |
|---|---|---|
| `VPYTHON_REF` | `~/workspace/math-physics-academy/vpython-reference` — the third-party clone, outside the repo | **required**: the tool exits naming the path it wanted |
| `VCPP_SRC` | `~/workspace/math-physics-academy/vcpp/src` | prints a NOTE, leaves the two vcpp columns blank, **does not fail** |
| `VPYTHON_CORPUS` | private student labs (not redistributable) | prints a WARNING that usage counts are undercounted |

**9 of the 14 columns in `members.csv` are a pure function of glowscript@744de98** and should
reproduce exactly: `owner, member, kind, default, read_only, note, runtime_source, inherited_from,
documented`. The other five depend on inputs you may not have: `vcpp_status`, `vcpp_name` (need the
vcpp repo), `corpus_uses`, `programs` (need the corpus), and `user_facing` (derived from both).

Measured, so you know the size of the difference: running **without** the private student labs leaves
`user_facing` unchanged at 1,061 and moves members-used-by-a-program from 237 to **234**. The private
half of the corpus affects almost nothing.

### 7.3 Write your own extractor (the real test)
You will **not** get 1,207 members over 48 owners, and that is not a disagreement. Those totals encode
choices: what counts as an owner, `INTERNAL_OWNERS`, `NOT_A_GLOBAL`, the pseudo-owner `texture`,
naming `Mouse.prototype` as `mouse` and `orbital_camera.prototype` as `camera`, the `delete`↔`remove`
rename, and the `user_facing` heuristic. All are documented in `METHOD.md` §4, which is what makes a
divergence diagnosable instead of mysterious.

Check yourself against facts that survive any convention instead:

```bash
python3 tools/invariants.py          # reads ONLY the glowscript clone — no vcpp, corpus, docs or CSVs
```

At glowscript `744de98` these are stable:

| invariant | value |
|---|---|
| `property.declare()` blocks | 32 |
| `subclass(child, base)` edges | 17 |
| `exports` entries across all blocks / distinct names | 132 / 117 |
| constructors taking `args`/`options`/`parameters` | 51 |
| shared `init()` options | 10 |
| `GScompiler` `vp_primitives` | 58 |
| `canvas.trigger()` | `canvas.js:555` |

**If your extraction cannot account for every declare block, subclass edge and exported name above,
it has a gap.** Two worked reconciliations you can reproduce:

- **117 distinct exported names → 98 globals**: drop 14 `NOT_A_GLOBAL` (`Autoscale`, `Mesh`,
  `WebGLRenderer`, `series`, `vp_graph`, `gdisplay`, `ghistogram`, `fieldset`, `aria_div`, `text3D`,
  `orbital_camera`, `glowVersion`, `set_crosshairs_disabled`, `__array_times_number`) and 5
  `attributeVector*`. Nothing unexplained, nothing added.
- **Recall**: ≥409 of the docs table's 411 pairs, the two exceptions being §2.1's.

Note `vectors.js` and `vectors_no_overload.js` each contain two `exports` blocks and are alternative
builds — only one is loaded at runtime, which is why the raw entry count (132) exceeds the distinct
name count (117).

### 7.4 What counts as "approximately reproduced"
You have reproduced this work if you independently:
1. account for all 32 declare blocks, 17 subclass edges and 117 exported names;
2. cover at least 409 of the 411 documented pairs;
3. find `cone`, `pyramid` and `group` (they have no declare block — reachable only via `subclass()`);
4. discover that **event objects have no declaration at all** and must be enumerated by hand
   (`invariants.py` §7 points at the line that proves it).

Totals within a few percent of ours, with the differences traceable to §7.3's conventions, is a
successful reproduction. An identical total is not required and should make you suspicious that the
two extractions are not actually independent.

## 8. Environment caveats an auditor will hit

- The reference material is **pinned**: glowscript `744de98`, vpython-jupyter `3254f6e`. Line numbers
  in citations are only valid at those commits.
- The tools and tables are version-controlled in vcpp, so you can `git log` them. The third-party
  material they read (`vpython-reference/`) is **not** under version control — it is pinned only by
  the commit hashes above, so verify those before trusting a line number.
- The student-lab half of the corpus is referenced in place at
  `~/workspace/pycode_similar/vpython_codes/`. If that path is absent, counts silently shrink to the
  34 official programs — no error.
- Python 3 standard library only; no network access needed except to re-fetch the corpus.
