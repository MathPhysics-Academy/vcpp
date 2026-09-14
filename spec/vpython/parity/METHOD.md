# How the VPython → vcpp parity table is built

This explains exactly how `parity/` is produced, so a person or an agent can
rebuild it, check it, and extend it. The results are in `SUMMARY.md` and the
CSVs; this file is the method.

**Ground rules**
- **Third-party material is never committed.** GlowScript's source, its docs
  and the fetched corpora live in a separate `vpython-reference/` folder
  outside every repo; set `VPYTHON_REF` to it (§3). Only what we wrote — this
  table, its tool and this method — lives here.
- The table is only as good as its extraction. After any change to the tool,
  re-run the spot checks in §7 before trusting a number.

## 1. Question the table answers

For every object and attribute Web VPython documents: does vcpp have it, can
it be set by name the way VPython code sets it, and how much do real programs
use it? The same question is asked for built-in functions and vector
functions.

## 2. Inputs

| input | location | pinned at |
|---|---|---|
| Web VPython spec | `glowscript/docs/VPythonDocs/*.html` (76 Sphinx pages, "VPython 3.2 documentation", byte-identical to glowscript.org's live docs on 2026-09-11) | glowscript `744de98` |
| built-in names | `glowscript/lib/compiling/GScompiler.js`, the `vp_primitives` array | same |
| vcpp | this repo's `src/*.cppm` (read-only) | whatever is checked out; record the commit when publishing numbers |
| corpus, official | `$VPYTHON_REF/corpus/glowscript-official/*.vpy`, 34 programs | fetched 2026-09-11 |
| corpus, student | `~/workspace/pycode_similar/vpython_codes/**/*.py`, 11 PHYS151 labs (2022), used in place because they carry students' names | — |

`vpython-jupyter/` is **not** an input. Its API differs from Web VPython, which
is what students actually run.

## 3. Reproduce from scratch

```bash
mkdir -p ~/workspace/math-physics-academy/vpython-reference && cd $_   # outside any repo
git clone --depth 1 https://github.com/vpython/glowscript        # then: git -C glowscript checkout 744de98 for identical results
export VPYTHON_REF=$PWD

cd <your vcpp checkout>
python3 spec/vpython/tools/fetch_corpus.py   # official examples via glowscript.org's public API
python3 spec/vpython/tools/parity.py         # rewrites spec/vpython/parity/*, prints diagnostics
```

The clone and the corpus stay under `$VPYTHON_REF`; only the generated tables live in the repo.

Python 3 standard library only; no packages needed.

## 4. Extraction rules

### 4a. The spec, from GlowScript's docs (`parse_docs`)

The Sphinx pages have two structures that matter:
- **Signatures**, `<dt class="sig sig-object py">`. For example
  `sphere(pos=vec(0,0,0), radius=2, color=color.cyan)`,
  `gc = gcurve(color=color.red)`, `gc.plot(x, y)`.
- **Parameter lists**, a `Parameters:` field whose `<li>` items read
  `name (type) – description… Default …`. The `(type)` part is sometimes
  absent — `label.html` has `background – Color of…`.

Rules, applied in document order:
1. **Which object a signature belongs to.** A signature whose leading name is
   a known VPython object (`KNOWN` in the tool) opens that object's context.
   `var = obj(...)` also records `var → obj`, so a later `var.method(...)`
   resolves to `obj`.
2. **Methods.** A signature of the form `x.method(...)` opens a method
   context, and a `Parameters` block that follows belongs to the method, not
   the object. That's why `gc.plot(x, y)` doesn't add `firstargument` to
   gcurve. The receiver `x` is resolved from evidence only:
   - a variable assigned earlier on the same page (`gc = gcurve(...)`)
   - `scene`, which is VPython's default canvas (`RECEIVERS`)
   - the docs' `my<object>` naming convention (`mygraph.select()`,
     `myslider.delete()`), accepted only when `<object>` is a known object
   - an object name used directly (`graph.get_selected()`)

   Anything else is skipped and listed in `parity/skipped_signatures.txt` for
   review, never guessed. An earlier version fell back to
   "the last object seen", and that attached `scene.waitfor("textures")` on
   `texture.html` to `box`. An item written `name()` (e.g. `delete()` on
   widgets) is also a method.
3. **Positional arguments.** `firstargument`, `secondargument` and
   `first_argument` describe positional arguments (`attach_arrow`,
   `compound`'s object list), so they're recorded separately and aren't
   attributes.
4. **Signature keywords.** Keywords shown in a signature are attributes too,
   except on `trail.html`, whose examples use `sphere` only to illustrate
   trails.
5. **Defaults** come from the first `Default …` phrase in the description,
   and **read-only** is flagged when the description says so (e.g.
   `canvas.pixel_to_world`).
6. **Trail attributes** (`make_trail`, `trail_type`, `trail_radius`,
   `interval`, `retain`, `pps`) are copied only to the objects whose own page
   links `trail.html`: arrow, box, compound, cone, cylinder, ellipsoid,
   pyramid, ring, sphere. That's evidence from the pages, not a guess.
7. **`standardAttributes.html` is read but not copied onto objects.** No
   object page links it, and every object page already lists its own
   parameters. So the spec is exactly what each object's page documents.

**Known consequence:** attributes that exist in the GlowScript runtime but
aren't documented are absent from the spec, `trail_color` included. vcpp
names that fall in this gap show up in SUMMARY's "vcpp names the GlowScript
docs don't have".

### 4b. vcpp's side, from `vcpp/src` (`parse_vcpp`)

- **Declared names:** `inline constexpr symbol<> NAME{}` in
  `vcpp-props.cppm`, including the `vcpp::prop` namespace (`box`, `points`,
  `label`).
- **Objects:** factory functions `T_object NAME(...)` map a VPython name to a
  struct. One rename: VPython `text` is vcpp `text3d`.
- **Members:** `m_NAME` fields of each struct, plus its base structs
  (`object_base`, `graph_base`, `plot_base`).
- **Settable by name ("wired"):**
  - symbols in `object_params<T>`
  - plus `common_params`, for structs deriving directly from `object_base`
    (`make<T>()` applies them)
  - plus any `is_bound<decltype(NAME)>` handled by hand inside a factory body
    (e.g. `graph()`'s string titles)
- **Verified renames** (`ALIASES`), each confirmed in vcpp source:
  - `gcurve.graph` → `graph_ref`
  - `gcurve.dot` → `show_dot`
  - `text.depth` → `thickness`

  Add to this map only with a source citation.

### 4c. Status of each (object, attribute) pair

| status | meaning |
|---|---|
| wired | settable by name, e.g. `box(pos=…)` |
| field-only | vcpp has the member, but `obj(name=…)` silently ignores it, e.g. `box.size` |
| declared-only | the symbol exists, but nothing backs it on that object, e.g. `velocity` |
| missing | no symbol and no member |
| no-object | vcpp has no factory for the object (widgets, canvas, lights, vertex…) |

### 4d. Usage, from the corpus (`parse_corpus`)

- Strip a first line matching `Web VPython …` / `#Web VPython …`, which isn't
  Python, then parse with Python's `ast`.
- Skip programs containing `$(` (jQuery calls into the web page — browser-only
  and out of scope). Report syntax errors rather than hiding them.
- **Type inference is deliberately simple:** `x = sphere(...)` makes `x` a
  sphere for the whole program. The tool counts constructor keywords, then
  `x.attr = …` / `x.attr += …` as assigns and `x.attr` reads, per object
  type.
- Objects stored in lists or returned from functions aren't typed, so their
  uses are undercounted.
- A keyword or assignment whose name isn't in that object's spec counts as a
  **user-defined attribute** (`p`, `v`, `m` …). That's the evidence for an
  attribute bag in the transpiler.

### 4e. Globals (`parse_globals`)

- **Names:** the `vp_primitives` array in `GScompiler.js` (the names a program
  can use without importing), plus the signatures on `vector.html`.
- **"vcpp has":** a **heuristic** — some function, object, constant or
  namespace of that name is declared in `vcpp/src`. Treat a "yes" as "a
  same-named thing exists", not "same behavior".

## 5. Outputs

| file | one row per |
|---|---|
| `objects.csv` | (object, attribute): doc type, default, page, read-only, vcpp struct, vcpp name, status, kwarg/assign/read counts, programs using it |
| `methods.csv` | documented method, with its documented parameters and the signature it was read from (`doc_sig`) |
| `skipped_signatures.txt` | page and callee of a method signature whose receiver couldn't be resolved — review by hand. The documented signature text is third-party, so it is deliberately not copied into the repo |
| `user_attrs.csv` | user-defined attribute seen in the corpus |
| `globals.csv` | built-in or vector function: vcpp presence heuristic, corpus calls |
| `SUMMARY.md` | totals, gaps ranked by real usage, per-object coverage, vcpp-only names, user attributes, globals, methods |

## 6. Known limitations

- The spec is the docs. Undocumented runtime attributes are missing (§4a).
  Documented defaults can differ from the runtime; spot-check those against
  `glowscript/lib/glow/primitives.js`.
- "wired" means settable by name. It does not mean the renderer honors the
  value.
- Corpus counts are lower bounds (untyped list elements, §4d). The corpus is
  45 programs: official examples plus one course's labs, so it's weighted
  towards those.
- The globals "vcpp has" column is a name match only.
- **Not yet tabulated:** the 20 signatures in `skipped_signatures.txt` (as of
  2026-09-11):
  - 16 `shapes.*` library functions (`circle`, `rectangle`, `arc`, `gear` …;
    `shapes_and_paths.html`)
  - `color.rgb_to_hsv` / `color.hsv_to_rgb`
  - the generic 3D-object methods `myobject.rotate(...)` and
    `myobject.clone(...)` (`rotation.html`, `clone.html`)

  These are library functions and methods every object has, not per-object
  methods, so rule 2 correctly refuses to attach them to an object. They need
  their own categories. Until then, the corpus undercounts them: the table
  counts only bare calls like `rotate(v, …)`, while a raw scan finds
  `obj.rotate(...)` in 12 programs and `clone` in 3.
- Extraction is regex over Sphinx HTML. A different Sphinx version or theme
  can break it; the diagnostics printed by `parity.py` are there to catch
  that.

## 7. Spot checks — run after every change

| check | expected | why |
|---|---|---|
| `box.size` status | field-only | `common_params` lacks `size`; `object_base` has `m_size` |
| `gcurve.graph` | wired, via `graph_ref` | alias + manual `is_bound` in `gcurve()` |
| `label.background` | present in the spec | typeless item on `label.html` |
| no object has `firstargument`/`secondargument`/`first_argument` as an attribute | none | positional arguments, rule 3 |
| widgets' `delete` | methods.csv, not objects.csv | `delete()` items, rule 2 |
| `canvas.pixel_to_world` | read_only = yes | "Read-only" in its description |
| trail attributes on `curve`, `helix`, `label` | absent | those pages don't link `trail.html` |
| `waitfor` in methods.csv | on `canvas` only, not `box` | `scene.waitfor(...)` resolves via `RECEIVERS`, rule 2 |
| `graph` methods | include `select`, `delete`, `get_selected` | `mygraph.…` resolves via the `my<object>` rule |
| every `methods.csv` row | its `doc_sig` really is that object's method | read the column; no fallback attribution exists any more |
| corpus parse line | 41 of 45 parsed; 3 jQuery; 1 syntax error (a student file) | as of 2026-09-11 |
