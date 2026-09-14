# The Python subset Web VPython actually implements

`catalog/` catalogues the **VPython API** (objects, members, globals, events). This file covers the
other half a transpiler has to handle: **the Python language itself**, as Web VPython implements it.

The target is not CPython. Web VPython transpiles Python to JavaScript with **RapydScript-NG 0.7.22**
(`glowscript/lib/rapydscript/compiler.js`, `rs_version` at line 4), which implements a *subset*. That
subset is the ceiling any vcpp transpiler needs to match — and it is meaningfully smaller than Python,
which is good news.

Measured 2026-09-12/13 against glowscript `744de98`. Every claim below was produced by running the
real compiler, not by reading it.

---

## 1. How this was measured (so it can be re-checked)

The bundled compiler loads in node, exactly the way GlowScript itself calls it
(`GScompiler.js:759-762` — `RapydScript.create_embedded_compiler()` then `.compile(src, {js_version:6})`):

```bash
node tools/python_probe.js     # builtin availability + syntax matrix
python3 tools/python_census.py # what the 45 corpus programs actually use
```

A name is **available** if the runtime installs a bare alias for it (`var len = ρσ_len`,
`runtime.js:623-628, 1076, 1659, 2233, 4397`), **or** GlowScript exports it as a global
(`api_misc.js` exports block), **or** the compiler injects it into the program text.

**Caveat that matters:** a name compiling is not proof it runs. RapydScript emits unknown names
**bare** — `sum([1,2])` compiles to `sum(ρσ_list_decorate([1,2]))` exactly as `len(...)` does. The
difference is only whether an alias exists at runtime. Absences below are "defined nowhere in the
runtime, the glow exports, or the compiler injections", which is strong evidence but was **not
executed in a browser**.

## 2. Builtins

### Available
`len range print int float str bool dict set list enumerate reversed min max pow divmod type repr
chr ord hex bin getattr setattr hasattr callable iter dir id` — from RapydScript's runtime aliases.

`abs` and `round` — from GlowScript's exports (`api_misc.js`: `abs: Math.abs`, `round: Math.round`).

`input` — **injected by the compiler**, not exported: `GScompiler.js:1268` writes a `function input(arg)`
into the program when the source mentions `input`. `round` is injected the same way at
`GScompiler.js:1254`, as `function round(num, n=0)` using `toFixed` — i.e. with Python's ndigits
semantics, shadowing the `Math.round` export.

### NOT available — defined nowhere
**`sum` `sorted` `tuple` `zip` `map` `filter` `any` `all`**

This is the single most useful result here. A transpiler that accepts `sum()` or `sorted()` accepts
programs the reference implementation rejects.

Two quirks worth knowing:
- `min` is implemented as `ρσ_max.bind(Math.min)` (`runtime.js:623`) — one wrapper serves both, which
  is why `ρσ_min` does not exist as a symbol.
- `abs` is defined **twice** — `runtime.js:623` (`Math.abs`) and the glow exports. Load order decides.

## 3. Syntax

Compiles: **list / dict / generator comprehensions**, `try/except`, `try/finally`, `raise`, `with`,
f-strings, slicing, negative indexing, classes, inheritance, default arguments, `*args`/`**kwargs`,
tuple unpacking, chained comparison, ternary, `global`, `yield`, decorators, `assert`, `del`,
set literals, string methods.

Does **not** compile:
| feature | note |
|---|---|
| `lambda` | `Unexpected token: name`. RapydScript spells anonymous functions `def(a): ...` — that form does compile |
| `while/else` | `Unexpected token: punc «:»` |
| `for/else` | same |

`str.format` works, via two independent implementations: RapydScript's `ρσ_str.format`
(`runtime.js:3300`) and GlowScript's own `String.prototype.format` (`api_misc.js:385`).

## 4. What real programs actually use

From the 45-program corpus (34 official + 11 student labs); 41 parse. Of the 4 that don't, 3 use
jQuery `$(...)` (browser-only, out of scope) and 1 student file has inconsistent indentation.

Used: 61 functions, 3 classes, 67 `for`, 38 `while`, 130 `if`, 155 subscripts, 159 list literals,
45 tuples, 43 augmented assignments, 29 `global`, 10 `from vpython import *`, 5 dicts, 2 f-strings.

**Not used at all:** `lambda`, comprehensions of any kind, `try`/`except`, `with`, generators,
`assert`, `del`, slices, `Import` (only `ImportFrom`), sets, `async`.

Calls that are neither VPython nor user-defined — i.e. the entire Python-builtin demand of the corpus:
`range` (54), `int` (16), `len` (7), `isinstance` (2), `Exception` (1), `input` (1), `Date` (1).

**So the floor is very low and the ceiling is moderate.** A transpiler covering `range`, `int`, `len`,
lists, subscripting, functions, classes and `global` would handle essentially all existing programs.

## 5. Consequences for vcpp

- The **async problem** dominates, not the syntax. `rate()`, `sleep()`, `scene.pause()`,
  `scene.waitfor()`, `scene.capture()`, `input()`, `winput()`, `get_library()`, `read_local_file()`
  all suspend; GlowScript wraps the whole program in an `async function` and propagates `async`
  through any user function that transitively waits. That is a whole-program analysis
  (`vpfcts`/`vpwaits` in `GScompiler.js`), and vcpp's equivalent is C++20 coroutines + Asyncify.
- **Python container semantics** are needed in C++: list/dict/set behaviour, negative indexing,
  slicing, string methods, `.format`. RapydScript ships ~90 runtime helpers for this.
- **The corpus says which subset to build first** (§4), and it is small.

## 6. Known gap this exposed in `catalog/`

`globals.csv` derives globals from `exports` blocks, so it **misses compiler-injected names**:
`input` and `round` are absent from its 98. Recorded in `catalog/AUDIT.md` §4.5. Not fixed.

## 7. Scripts

- `tools/python_probe.js` — builtin availability + syntax matrix (node; needs the glowscript clone)
- `tools/python_census.py` — corpus census (python3; imports `catalog_runtime` for its corpus paths)

Both still contain an absolute path to this folder near the top; adjust if you move things. They were
written as throwaway probes and are kept because the claims above are only as good as the ability to
re-run them.
