#!/usr/bin/env python3
"""Catalogue of Web VPython's API taken from GlowScript's RUNTIME SOURCE, matched against vcpp.

Why the runtime and not the docs: GlowScript's code states its own API exactly — each lib/glow/*.js
ends with an `exports` block, each object declares its members via property.declare(), and
inheritance is explicit in subclass(). The docs (used by tools/parity.py) describe only part of it,
and in prose, so whole areas such as curve's methods are missing there.

Writes catalog/members.csv, catalog/globals.csv, catalog/SUMMARY.md, catalog/diagnostics.txt.
Does not touch parity.py or parity/. Method, rules and caveats: catalog/METHOD.md.
"""
import ast, collections, csv, glob, html, os, re, sys

HOME = os.path.expanduser("~")
# TWO ROOTS, deliberately separate:
#   REF  = the third-party material (GlowScript's source, its docs, the fetched corpus). It lives
#          OUTSIDE this repo and is never committed. Point VPYTHON_REF at that folder.
#   SELF = this tree, resolved from THIS FILE. All OUTPUT goes here, so a run always rewrites the
#          committed tables rather than a stray copy next to the clone.
# Keeping them separate is what lets the tools be version-controlled while the source they read is not.
SELF = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REF = os.environ.get("VPYTHON_REF") or os.path.join(HOME, "workspace/math-physics-academy/vpython-reference")
GLOW = os.path.join(REF, "glowscript/lib/glow")
EXTRUDE = os.path.join(GLOW, "extrude.js")
COMPILER = os.path.join(REF, "glowscript/lib/compiling/GScompiler.js")
DOCS = os.path.join(REF, "glowscript/docs/VPythonDocs")
VCPP_SRC = os.environ.get("VCPP_SRC") or os.path.join(HOME, "workspace/math-physics-academy/vcpp/src")
# The second pattern is private material (student labs, not redistributable). Anyone without it gets
# a smaller corpus — which is why main() reports the per-pattern file counts rather than failing.
CORPUS = [os.path.join(REF, "corpus/*/*.vpy"),
          os.environ.get("VPYTHON_CORPUS") or os.path.join(HOME, "workspace/pycode_similar/vpython_codes/**/*.py")]
OUT = os.path.join(SELF, "catalog")

if not os.path.isdir(GLOW):
    sys.exit(f"no glowscript checkout at {GLOW}\n"
             f"Set VPYTHON_REF to the folder holding glowscript/ — third-party material, kept outside this repo.")

WIDGETS = ["button", "slider", "menu", "checkbox", "radio", "winput", "wtext"]
GRAPH_KINDS = ["gcurve", "gdots", "gvbars", "ghbars"]          # all thin wrappers over gobject
# VPython name -> vcpp factory, where they differ
VCPP_FACTORY = {"text": "text3d"}
# (object, VPython member) -> vcpp name, each verified in vcpp/src
ALIASES = {("gcurve", "graph"): "graph_ref", ("gcurve", "dot"): "show_dot", ("text", "depth"): "thickness",
           ("curve", "clear"): "clear_points", ("points", "clear"): "clear_points"}
# GScompiler.js rewrites `.delete` -> `.remove`, so the runtime's `remove` is VPython's `delete`.
RUNTIME_RENAME = {"remove": "delete"}
# Not objects a program creates, so they get no member rows. `keysdown` and `draw` ARE documented
# (key.html, userinput.html) and still belong in globals.csv — NOT_A_GLOBAL is that separate filter.
# The last four are internal base classes: their members reach children through subclass(), but they
# are not objects a program can create.
INTERNAL_OWNERS = {"myClass", "point", "Autoscale", "Mesh",
                   "WebGLRenderer", "attributeVector", "series", "vp_graph", "gdisplay", "ghistogram",
                   "fieldset", "aria_div", "keysdown", "draw", "text3D",
                   "Primitive", "gobject", "shape_object", "path_object"}
# Excluded from globals.csv: internal plumbing, plus the two the docs never mention
# (`fieldset`, `aria_div`) and `ghistogram`, which throws "not currently implemented".
NOT_A_GLOBAL = {"myClass", "point", "Autoscale", "Mesh", "WebGLRenderer", "series", "vp_graph",
                "gdisplay", "ghistogram", "fieldset", "aria_div", "text3D", "Mouse", "orbital_camera",
                "compute_autocenter", "set_crosshairs_disabled", "glowVersion", "__array_times_number",
                "Primitive", "gobject", "shape_object", "path_object"}
# vec arithmetic is spelled as operators in C++, not named methods (lam::linalg::fixed_vector has them all)
VEC_OPERATORS = {"add": "operator+", "sub": "operator-", "multiply": "operator*", "divide": "operator/",
                 "equals": "operator==", "toString": "std::formatter"}

# ---------------------------------------------------------------- event objects (HAND-ENUMERATED)
# The one part of the API no extractor can reach. canvas.trigger() builds the object it hands to a
# bound callback, to waitfor() and to pause() as ad-hoc literals (canvas.js:556-590): there is no
# declare block, no prototype and no exports entry to read. Every row cites the line that assigns
# the field — re-check them after any canvas.js change. Full write-up: catalog/EVENTS.md.
#
# NOT an event object: widget callbacks. Each widget passes its own closure object to bind —
# attrs.bind(cbutton|cslider|cmenu|ccheckbox|cradio|cwinput) at primitives.js:2934, 2943, 3149,
# 3323, 3333, 3392, 3546, 3550, 3667 — so what the docs call "Button Event Attributes" are the
# widget's own properties, already catalogued under `button`, `menu`, `checkbox`, ...
#
# (member, note, runtime_source)
EVENT_MEMBERS = [
    ("type", "event type: mousedown mouseup mousemove mouseenter mouseleave click | keydown keyup | "
             "redraw draw_complete textures resize", "canvas.js:556"),
    ("event", "mouse and key events: the same string as type. Custom events: the raw trigger argument, "
              "so redraw/draw_complete carry elapsed seconds as ev.event.dt (WebGLRenderer.js:1680,1706) "
              "and resize carries {event:'resize'} (canvas.js:271)", "canvas.js:556,561,580"),
    ("canvas", "the canvas the event came from. NOT set for waitfor('textures'), whose trigger argument "
               "is null (WebGLRenderer.js:435)", "canvas.js:588"),
    ("pageX", "mouse events only: pixels from the left of the page", "canvas.js:559"),
    ("pageY", "mouse events only: pixels from the top of the page", "canvas.js:559"),
    ("which", "mouse events: always 1 — right and middle buttons are never reported. "
              "key events: the raw keycode", "canvas.js:559,580"),
    ("pos", "mouse events only: mouse position in world coordinates, in the plane through scene.center "
            "parallel to the screen", "canvas.js:562"),
    ("press", "mouse events only: 'left' on mousedown, otherwise None", "canvas.js:564"),
    ("release", "mouse events only: 'left' on mouseup and on click, otherwise None", "canvas.js:571,577"),
    ("key", "key events only: key name from the keycode tables at canvas.js:13-61; the vocabulary is "
            "listed in catalog/EVENTS.md", "canvas.js:581"),
    ("alt", "key events only: ALT was down when the event fired", "canvas.js:582"),
    ("ctrl", "key events only: CTRL was down when the event fired", "canvas.js:583"),
    ("shift", "key events only: SHIFT was down, or shift-lock is on", "canvas.js:584"),
]


def event_members():
    """The event object, in the shape runtime_catalogue() produces for a declared object."""
    return {name: dict(owner="event", member=name, kind="attribute", default="", read_only="",
                       note=note, source=src, inherited_from="")
            for name, note, src in EVENT_MEMBERS}


# ---------------------------------------------------------------- tiny JS scanner
def _skip_ws(s, i):
    while i < len(s):
        if s[i] in " \t\r\n":
            i += 1
        elif s.startswith("//", i):
            i = s.find("\n", i)
            i = len(s) if i < 0 else i + 1
        elif s.startswith("/*", i):
            j = s.find("*/", i)
            i = len(s) if j < 0 else j + 2
        else:
            break
    return i


def _skip_value(s, i):
    """From i, skip one value; stop at a top-level ',' or the block's closing '}'."""
    depth = 0
    while i < len(s):
        c = s[i]
        if c in "\"'`":
            q, i = c, i + 1
            while i < len(s) and s[i] != q:
                i += 2 if s[i] == "\\" else 1
            i += 1
            continue
        if s.startswith("//", i) or s.startswith("/*", i):
            i = _skip_ws(s, i)
            continue
        if c in "{[(":
            depth += 1
        elif c in ")]}":
            if depth == 0:
                return i
            depth -= 1
        elif c == "," and depth == 0:
            return i
        i += 1
    return i


def block_end(s, brace_i):
    """Index just past the '}' matching the '{' at s[brace_i]. String- and comment-aware.
    Use this instead of line-anchored regexes: GlowScript has one-line and multi-line bodies alike."""
    assert s[brace_i] == "{"
    depth, i = 0, brace_i
    while i < len(s):
        c = s[i]
        if c in "\"'`":
            q, i = c, i + 1
            while i < len(s) and s[i] != q:
                i += 2 if s[i] == "\\" else 1
            i += 1
            continue
        if s.startswith("//", i) or s.startswith("/*", i):
            j = _skip_ws(s, i)
            if j != i:
                i = j
                continue
        if c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0:
                return i + 1
        i += 1
    return len(s)


def function_body(s, m):
    """Body text of the function whose `function name(` match is m, by brace matching."""
    b = s.find("{", m.end())
    return s[b: block_end(s, b)] if b > 0 else ""


def scan_members(s, brace_i):
    """Top-level members of the object literal starting at s[brace_i] == '{'.
    Returns [(name, kind, value_text, offset)] where kind is 'pair' | 'get' | 'set' | 'shorthand'."""
    assert s[brace_i] == "{"
    out, i = [], brace_i + 1
    while True:
        i = _skip_ws(s, i)
        if i >= len(s) or s[i] == "}":
            return out
        m = re.match(r"(get|set)\s+([A-Za-z_$][\w$]*)\s*\(", s[i:])          # ES6 accessor
        if m:
            j = _skip_value(s, i + m.end() - 1)
            j = s.find("{", j)
            j = _skip_value(s, j) if j > 0 else j
            out.append((m.group(2), m.group(1), "", i))
            i = _skip_value(s, i)
            i += 1 if i < len(s) and s[i] == "," else 0
            continue
        m = re.match(r"(?:(['\"])([\w$]+)\1|([A-Za-z_$][\w$]*))\s*(:|\()", s[i:])
        if not m:
            i = _skip_value(s, i)
            if i < len(s) and s[i] == ",":
                i += 1
                continue
            return out
        name = m.group(2) or m.group(3)
        if m.group(4) == ":":
            vstart = i + m.end()
            vend = _skip_value(s, vstart)
            out.append((name, "pair", s[vstart:vend].strip(), i))
            i = vend + 1 if vend < len(s) and s[vend] == "," else vend
        else:                                                                 # method shorthand name(...) {...}
            out.append((name, "shorthand", "function", i))
            j = _skip_value(s, i + m.end() - 1)
            j = s.find("{", j)
            i = _skip_value(s, j) if j > 0 else _skip_value(s, i)
            i += 1 if i < len(s) and s[i] == "," else 0
        continue


def line_of(s, i):
    return s.count("\n", 0, i) + 1


def classify(value):
    """property.js semantics -> (kind, default, read_only, note)."""
    v = value.strip()
    if re.match(r"^(async\s+)?function\b", v) or v == "function":
        return "method", "", False, ""
    if v.startswith("{"):
        inner = {n: (k, txt) for n, k, txt, _ in scan_members(v, 0)}
        if "get" in inner or "set" in inner:
            throws = "throw new Error" in (inner.get("get", ("", ""))[1] or "")
            ro = "set" not in inner
            return ("unsupported" if throws else "attribute"), "", ro, ("getter throws" if throws else "accessor")
        if "value" in inner:
            ro = "readonly" in inner
            return "attribute", inner["value"][1], ro, "typed"
        return "attribute", v[:60], False, ""
    m = re.match(r"new\s+attributeVector\w*\s*\(\s*\w+\s*,\s*([-\d.,\s]+)\)", v)
    if m:
        return "attribute", "<" + ",".join(p.strip() for p in m.group(1).split(",")) + ">", False, "vector"
    return "attribute", v[:60], False, ""


# ---------------------------------------------------------------- runtime extraction
def runtime_catalogue(diag):
    files = {os.path.basename(p): open(p, encoding="utf-8", errors="replace").read()
             for p in glob.glob(os.path.join(GLOW, "*.js")) + [EXTRUDE, COMPILER]}
    members = collections.defaultdict(dict)       # owner -> member -> row dict
    exports, bases = {}, {}

    for fn, s in files.items():
        for m in re.finditer(r"(?:var|let|const)\s+exports\s*=\s*\{", s):   # extrude.js uses `let`
            for name, kind, val, off in scan_members(s, m.end() - 1):
                exports.setdefault(name, f"{fn}:{line_of(s, off)}")
    for fn, s in files.items():
        for m in re.finditer(r"subclass\(\s*(\w+)\s*,\s*(\w+)\s*\)", s):
            bases[m.group(1)] = m.group(2)

    def add(owner, name, kind, default="", ro=False, note="", src=""):
        name = RUNTIME_RENAME.get(name, name)
        if name.startswith("__") or name.startswith("_"):
            return
        members[owner].setdefault(name, dict(owner=owner, member=name, kind=kind, default=default,
                                             read_only="yes" if ro else "", note=note, source=src))

    # property.declare( X.prototype | X , { ... } )
    for fn, s in files.items():
        for m in re.finditer(r"property\.declare\(\s*([\w.]+)\s*,\s*\{", s):
            target = m.group(1)
            owner = target.split(".")[0]
            static = not target.endswith(".prototype")
            if owner in ("myClass",):
                continue
            # `scene.mouse` and `scene.camera` are what a program actually writes.
            owner = {"Mouse": "mouse", "orbital_camera": "camera"}.get(owner, owner)
            for name, mk, val, off in scan_members(s, m.end() - 1):
                kind, default, ro, note = classify("function" if mk in ("get", "set", "shorthand") and mk != "pair" else val)
                if mk in ("get", "set"):
                    kind, note = "attribute", "accessor"
                add(owner, name, kind, default, ro, (note + (" static" if static else "")).strip(),
                    f"{fn}:{line_of(s, off)}")

    # vec methods declared as vec.prototype.NAME = function
    s = files["vectors.js"]
    for m in re.finditer(r"vec\.prototype\.(\w+)\s*=\s*function", s):
        add("vec", m.group(1), "method", src=f"vectors.js:{line_of(s, m.start())}")

    # shapes / paths prototype methods
    s = files["shapespaths.js"]
    for owner_proto, owner in (("shape_object", "shapes"), ("path_object", "paths")):
        for m in re.finditer(rf"{owner_proto}\.prototype\.(\w+)\s*=\s*function", s):
            add(owner, m.group(1), "function", src=f"shapespaths.js:{line_of(s, m.start())}")

    # plain object namespaces: color, textures, bumpmaps
    for fn, var, owner in (("color.js", "color", "color"), ("primitives.js", "textures", "textures"),
                           ("primitives.js", "bumpmaps", "bumpmaps")):
        s = files[fn]
        m = re.search(rf"var\s+{var}\s*=\s*\{{", s)
        if m:
            for name, mk, val, off in scan_members(s, m.end() - 1):
                kind, default, ro, note = classify(val)
                add(owner, name, "function" if kind == "method" else "constant", default, ro, note,
                    f"{fn}:{line_of(s, off)}")

    # widgets: closure style — accepted options + get/set properties + methods
    s = files["primitives.js"]
    for w in WIDGETS:
        # `^\s*` is WRONG here: \s matches \n, so under re.M the match starts on the previous line's
        # terminator and every derived line number is one too low. Use [ \t] for "indentation".
        m = re.search(rf"^[ \t]*function {w}\(args\)", s, re.M)
        if not m:
            diag.append(f"widget {w}: constructor not found")
            continue
        nxt = re.search(r"^\s{0,4}function \w+\(", s[m.end():], re.M)
        body = s[m.start(): m.end() + (nxt.start() if nxt else 20000)]
        base = line_of(s, m.start())
        am = re.search(r"var\s+attrs\s*=\s*\{", body)
        if am:
            for name, mk, val, off in scan_members(body, am.end() - 1):
                add(w, name, "attribute", val[:40], note="constructor option", src=f"primitives.js:{base}")
        for name in re.findall(r"args\.(\w+)\s*!==\s*undefined", body):
            add(w, name, "attribute", note="constructor option", src=f"primitives.js:{base}")
        for g, name in re.findall(r"\b(get|set)\s+(\w+)\s*\(", body):
            add(w, name, "attribute", note="accessor", src=f"primitives.js:{base}")
            if g == "set":
                members[w][RUNTIME_RENAME.get(name, name)]["read_only"] = ""
        for name in re.findall(r"^\s+(\w+)\s*:\s*(?:async\s+)?function", body, re.M):
            add(w, name, "method", src=f"primitives.js:{base}")

    # extrusion: built from the args its constructor reads; returns a compound
    s = files["extrude.js"]
    em = re.search(r"^function extrusion\(args\)", s, re.M)
    if em:
        for name in sorted(set(re.findall(r"args\.(\w+)", s[em.start():]))):
            add("extrusion", name, "attribute", note="constructor option", src=f"extrude.js:{line_of(s, em.start())}")
        bases["extrusion"] = "compound"

    # Constructor options. Every object's constructor reads args./options./parameters. directly, and that
    # is how `graph.fast`, `canvas.visible`, `triangle.bumpmap` and attach_*'s options are accepted —
    # none of them are declared properties.
    ctors = set()
    for fn in ("primitives.js", "graph.js", "canvas.js", "extrude.js"):
        s = files[fn]
        for m in re.finditer(r"^[ \t]*function (\w+)\s*\(", s, re.M):   # [ \t], not \s — see above
            owner = m.group(1)
            if owner not in exports:
                continue
            params = s[m.end(): s.find(")", m.end())]
            if not re.search(r"\b(args|options|objects|parameters)\b", params):
                continue
            body = function_body(s, m)
            ctors.add(owner)
            for name in sorted(set(re.findall(r"\b(?:args|options|parameters)\.(\w+)", body))):
                add(owner, name, "attribute", note="constructor option", src=f"{fn}:{line_of(s, m.start())}")

    # Options handled by the shared init() path. Which objects get them is evidence, not inference:
    # an object's constructor visibly calls init(this, args) or initObject(this, ...).
    s = files["primitives.js"]
    im = re.search(r"^[ \t]*function init\(obj, args\)", s, re.M)
    shared_init, init_users = [], set()
    if im:
        shared_init = sorted({n for n in re.findall(r"\bargs\.(\w+)", function_body(s, im)) if not n.startswith("_")})
        diag.append(f"shared init() options: {shared_init}")
    for fn in ("primitives.js", "extrude.js"):
        t = files[fn]
        for m in re.finditer(r"^[ \t]*function (\w+)\s*\(", t, re.M):
            if re.search(r"\b(init|initObject)\s*\(\s*(this|obj)\b", function_body(t, m)):
                init_users.add(m.group(1))
    diag.append(f"objects whose constructor calls init()/initObject(): {sorted(init_users)}")

    # `texture` is set as a dict: texture={file:..., bumpmap:..., place:...}. Those sub-keys are real API
    # (and are what the docs call e.g. triangle.bumpmap), so record them under a pseudo-owner.
    tm = re.search(r"^[ \t]+texture:\s*\{", s, re.M)
    if tm:
        tex_body = s[tm.end() - 1: block_end(s, tm.end() - 1)]
        for name in sorted({n for n in re.findall(r"\bargs\.(\w+)", tex_body) if not n.startswith("_")}):
            add("texture", name, "attribute", note="texture sub-option", src=f"primitives.js:{line_of(s, tm.start())}")

    # graph kinds share gobject's members
    for k in GRAPH_KINDS:
        bases[k] = "gobject"

    # compiler built-ins: the names a program may use unimported, and the ones that wait
    s = files["GScompiler.js"]
    prim = re.search(r"const vp_primitives = \[(.*?)\]", s, re.S)
    builtins = re.findall(r'"(\w+)"', prim.group(1)) if prim else []
    waits = re.findall(r'"(\w+)"', (re.search(r"var vpwaits = \[(.*?)\]", s, re.S) or re.search(r"()", "")).group(1) or "")
    return members, exports, bases, builtins, waits, files, ctors, shared_init, init_users


def resolve(members, bases, owner):
    """Members of `owner` including inherited ones; a child's own definition wins.
    `inherited_from` names the class that declared it, blank when the owner declares it itself."""
    chain, seen, o = [], set(), owner
    while o and o not in seen:
        seen.add(o)
        chain.append(o)
        o = bases.get(o)
    out = {}
    for o in reversed(chain):                      # base first so the child overrides
        for k, v in members.get(o, {}).items():
            out[k] = dict(v, inherited_from="" if o == owner else o)
    return out


# ---------------------------------------------------------------- vcpp side
def vcpp_index():
    """None when there is no vcpp checkout — the catalogue is still fully valid, just without the
    two vcpp columns. Everything else is a pure function of the pinned glowscript commit."""
    src = {os.path.basename(f): open(f).read() for f in glob.glob(os.path.join(VCPP_SRC, "*.cppm"))}
    if "vcpp-props.cppm" not in src:
        return None
    allsrc = "\n".join(src.values())
    # Accept BOTH spellings. vcpp writes `inline constexpr symbol pos{};`, but one branch spells the
    # same declaration `symbol<> pos{}` for an older clang. Matching only the second made a purely
    # cosmetic edit move 96 rows from declared-only to missing with every check still passing —
    # the column must not depend on which branch happens to be checked out.
    declared = set(re.findall(r"inline constexpr symbol(?:<>)?\s+(\w+)\{\}", src["vcpp-props.cppm"]))
    structs, methods = {}, collections.defaultdict(set)
    for m in re.finditer(r"(?:struct|class)\s+(\w+)\s*(?::\s*(?:public\s+)?(\w+))?\s*\{(.*?)\n\};", allsrc, re.S):
        name, base, body = m.group(1), m.group(2), m.group(3)
        structs[name] = (base, set(re.findall(r"\bm_(\w+)\s*(?:\{|=|;)", body)))
        for mm in re.finditer(r"^\s+(?:constexpr |inline |static |virtual |friend )*[\w:<>&*,\s]+?\b(\w+)\s*\([^;{)]*\)\s*"
                              r"(?:const\s*)?(?:noexcept\s*)?[{;]", body, re.M):
            if mm.group(1) not in ("if", "for", "while", "switch", "return", "sizeof", name):
                methods[name].add(mm.group(1))

    def fields(s):
        base, f = structs.get(s, (None, set()))
        return f | (fields(base) if base else set())

    def all_methods(s):
        base, _ = structs.get(s, (None, set()))
        return methods[s] | (all_methods(base) if base else set())

    wired = collections.defaultdict(set)
    for m in re.finditer(r"struct object_params<(\w+)>\s*\{(.*?)\n\};", allsrc, re.S):
        wired[m.group(1)] |= set(re.findall(r"decltype\((?:vcpp::)?(?:prop::)?(\w+)\)", m.group(2)))
    common = set(re.findall(r"decltype\((\w+)\)",
                            re.search(r"inline constexpr auto common_params\s*=(.*?);\n", allsrc, re.S).group(1)))
    factories = {}
    for m in re.finditer(r"^(?:constexpr\s+|inline\s+)*(\w+_object)\s+(\w+)\s*\(([^)]*)\)\s*\{(.*?)^\}", allsrc, re.S | re.M):
        factories[m.group(2)] = m.group(1)
        wired[m.group(1)] |= set(re.findall(r"is_bound<decltype\((?:vcpp::)?(?:prop::)?(\w+)\)", m.group(4)))
    for st in set(factories.values()):
        if structs.get(st, (None,))[0] == "object_base":
            wired[st] |= common
    free = set(re.findall(r"^(?:inline |constexpr |static )*[\w:<>]+\s+(\w+)\s*\(", allsrc, re.M))
    colors = set(re.findall(r"inline constexpr vec3\s+(\w+)", src.get("vcpp-color.cppm", "")))
    shapes = set(re.findall(r"inline shape2d\s+(\w+)\s*\(", src.get("vcpp-shapes.cppm", "")))
    return dict(declared=declared, structs=structs, fields=fields, methods=all_methods, wired=wired,
                factories=factories, free=free, colors=colors, shapes=shapes)


def match_vcpp(owner, member, kind, V):
    """-> (status, vcpp_name)."""
    name = ALIASES.get((owner, member), member)
    if owner in ("mouse", "camera"):          # vcpp: camera is a struct in vcpp-scene.cppm; no mouse object
        return ("field-only" if owner == "camera" and name in V["fields"]("camera") else "missing"), name
    if owner == "canvas":
        if name in V["methods"]("canvas") or member in ("background", "title", "caption", "visible"):
            return ("wired" if name in V["methods"]("canvas") else "field-only"), name
        return ("field-only" if name in V["fields"]("canvas") else "missing"), name
    if owner in ("color",):
        return ("wired" if member in V["colors"] else "missing"), f"colors::{member}"
    if owner in ("shapes", "paths"):
        return ("wired" if member in V["shapes"] else "missing"), f"shapes::{member}"
    if owner == "vec":
        if member in VEC_OPERATORS:
            return "wired", VEC_OPERATORS[member]
        return ("wired" if member in V["free"] else "missing"), member
    if owner in ("textures", "bumpmaps"):
        return "missing", ""
    st = V["factories"].get(VCPP_FACTORY.get(owner, owner))
    if not st:
        return "no-object", ""
    if kind == "method":
        return ("wired" if name in V["methods"](st) else "missing"), name
    if name in V["wired"][st]:
        return "wired", name
    if name in V["fields"](st):
        return "field-only", name
    if name in V["declared"]:
        return "declared-only", name
    return "missing", name


# ---------------------------------------------------------------- usage + docs mentions
def source_of(path):
    lines = open(path, encoding="utf-8", errors="replace").read().splitlines()
    if lines and re.match(r"\s*#?\s*(Web VPython|VPython|GlowScript)\b", lines[0]):
        lines = lines[1:]
    return "\n".join(lines)


def corpus_usage(owners):
    use = collections.Counter(); progs = collections.defaultdict(set); calls = collections.Counter()
    files = sorted({f for p in CORPUS for f in glob.glob(p, recursive=True)})
    parsed = 0
    for f in files:
        src, nm = source_of(f), os.path.basename(f)
        if "$(" in src:
            continue
        try:
            tree = ast.parse(src)
        except SyntaxError:
            continue
        parsed += 1
        types = {}
        for n in ast.walk(tree):
            if isinstance(n, ast.Assign) and isinstance(n.value, ast.Call) and isinstance(n.value.func, ast.Name) \
                    and n.value.func.id in owners:
                for t in n.targets:
                    if isinstance(t, ast.Name):
                        types[t.id] = n.value.func.id
        # Event objects never come from a constructor, so the rule above cannot type them. Two
        # evidence-based rules instead: the first parameter of a function bound with scene.bind() is
        # an event, and so is whatever scene.waitfor()/scene.pause() returns. A widget's `bind=`
        # callback is deliberately NOT included — widgets pass themselves, not an event.
        bound = {a.id for n in ast.walk(tree)
                 if isinstance(n, ast.Call) and isinstance(n.func, ast.Attribute) and n.func.attr == "bind"
                 for a in n.args if isinstance(a, ast.Name)}
        for n in ast.walk(tree):
            if isinstance(n, (ast.FunctionDef, ast.AsyncFunctionDef)) and n.name in bound and n.args.args:
                types[n.args.args[0].arg] = "event"
            if isinstance(n, ast.Assign) and isinstance(n.value, ast.Call) \
                    and isinstance(n.value.func, ast.Attribute) and n.value.func.attr in ("waitfor", "pause"):
                for t in n.targets:
                    if isinstance(t, ast.Name):
                        types[t.id] = "event"
        for n in ast.walk(tree):
            if isinstance(n, ast.Call) and isinstance(n.func, ast.Name):
                calls[n.func.id] += 1
                if n.func.id in owners:
                    for k in n.keywords:
                        if k.arg:
                            use[(n.func.id, k.arg)] += 1; progs[(n.func.id, k.arg)].add(nm)
            if isinstance(n, ast.Attribute):
                base = n.value
                if isinstance(base, ast.Name):
                    owner = types.get(base.id) or (base.id if base.id in ("color", "shapes", "paths", "textures", "bumpmaps") else None)
                    if owner:
                        use[(owner, n.attr)] += 1; progs[(owner, n.attr)].add(nm)
    return use, progs, calls, parsed, len(files)


def docs_mentions():
    text = ""
    for p in glob.glob(os.path.join(DOCS, "*.html")):
        t = open(p, encoding="utf-8", errors="replace").read()
        text += re.sub(r"\s+", " ", html.unescape(re.sub(r"<[^>]+>", " ", t)))
    return text


# ---------------------------------------------------------------- main
def main():
    os.makedirs(OUT, exist_ok=True)
    diag = []
    members, exports, bases, builtins, waits, files, ctors, shared_init, init_users = runtime_catalogue(diag)
    # The event object is hand-enumerated: trigger() builds it as a literal, so nothing declares it.
    members["event"] = event_members()
    diag.append(f"event members (hand-enumerated from canvas.js:556-590): {len(members['event'])}")
    V = vcpp_index()
    if V is None:
        print(f"NOTE: no vcpp source at {VCPP_SRC} — vcpp_status/vcpp_name will be blank. "
              f"Set VCPP_SRC to fill them.", file=sys.stderr)
        diag.append(f"vcpp source ABSENT ({VCPP_SRC}): vcpp columns blank")
    # Inputs are environment-dependent; say so in the output itself, so a table can be traced back
    # to the corpus that produced it.
    for pat in CORPUS:
        n = len(glob.glob(pat, recursive=True))
        diag.append(f"corpus pattern -> {n} files: {pat}")
        if n == 0:
            print(f"WARNING: corpus pattern matched no files: {pat}\n"
                  f"         corpus_uses/programs will be undercounted.", file=sys.stderr)
    docs = docs_mentions()

    # Owners: every exported constructor (so cone/pyramid/group, which only exist via subclass(), appear),
    # plus anything with its own declare block, plus the graph kinds and extrusion.
    owners = sorted(o for o in (set(members) | ctors | set(GRAPH_KINDS) | {"extrusion"})
                    if o not in INTERNAL_OWNERS)
    # `mouse` and `camera` are reached through `scene`, so they have no constructor of their own.
    diag.append(f"mouse/camera members: mouse={len(members.get('mouse', {}))}, camera={len(members.get('camera', {}))}")

    def takes_shared_init(o):
        """True when this object's constructor calls init()/initObject() — directly or through its base."""
        seen, x = set(), o
        while x and x not in seen:
            if x in init_users:
                return True
            seen.add(x)
            x = bases.get(x)
        return False
    use, progs, calls, parsed, nfiles = corpus_usage(set(owners))

    rows = []
    for owner in owners:
        eff = resolve(members, bases, owner)
        if takes_shared_init(owner):
            for n in shared_init:
                eff.setdefault(n, dict(owner=owner, member=n, kind="attribute", default="", read_only="",
                                       note="constructor option (shared init)", source="primitives.js:init()",
                                       inherited_from=""))
        for name, r in sorted(eff.items()):
            status, vname = match_vcpp(owner, name, r["kind"], V) if V else ("", "")
            documented = "yes" if re.search(rf"\b{re.escape(name)}\b", docs) else "no"
            nprog = len(progs[(owner, name)])
            # user-facing by evidence: the docs mention it, a real program uses it, or the compiler
            # lists it as a built-in. Everything else is likely an internal helper — marked, not dropped.
            user_facing = "yes" if (documented == "yes" or nprog or name in builtins) else "no"
            rows.append(dict(owner=owner, member=name, kind=r["kind"], default=r["default"], read_only=r["read_only"],
                             note=r["note"], runtime_source=r["source"], inherited_from=r.get("inherited_from", ""),
                             documented=documented, user_facing=user_facing,
                             vcpp_status=status, vcpp_name=vname,
                             corpus_uses=use[(owner, name)], programs=nprog))
    with open(os.path.join(OUT, "members.csv"), "w", newline="") as fh:
        w = csv.DictWriter(fh, fieldnames=list(rows[0])); w.writeheader(); w.writerows(rows)

    grows = []
    for name, src in sorted(exports.items()):
        if name in NOT_A_GLOBAL or name.startswith("attributeVector"):
            continue
        grows.append(dict(name=name, runtime_source=src, is_object=("yes" if name in members else ""),
                          compiler_builtin="yes" if name in builtins else "", waits="yes" if name in waits else "",
                          vcpp_has="" if not V else ("yes" if (name in V["free"] or name in V["factories"] or
                                                              VCPP_FACTORY.get(name) in V["factories"]) else "no"),
                          corpus_calls=calls[name]))
    with open(os.path.join(OUT, "globals.csv"), "w", newline="") as fh:
        w = csv.DictWriter(fh, fieldnames=list(grows[0])); w.writeheader(); w.writerows(grows)

    facing = [r for r in rows if r["user_facing"] == "yes"]
    helpers = [r for r in rows if r["user_facing"] == "no"]
    by = collections.Counter(r["vcpp_status"] for r in facing)
    kinds = collections.Counter(r["kind"] for r in facing)
    used = [r for r in rows if r["programs"]]
    gaps = sorted((r for r in used if r["vcpp_status"] != "wired"), key=lambda r: (-r["programs"], -r["corpus_uses"]))
    undoc = [r for r in rows if r["documented"] == "no"]
    L = ["# Web VPython API from GlowScript's runtime → vcpp", "",
         f"Generated by `tools/catalog_runtime.py`; method in `catalog/METHOD.md`. "
         f"{len(rows)} members across {len(owners)} owners, of which {len(facing)} are user-facing "
         f"(documented, used by a program, or a compiler built-in) and {len(helpers)} look like internal "
         f"helpers; {len(grows)} exported globals; corpus {parsed}/{nfiles} programs.", "",
         "All counts below cover the user-facing members only.", "",
         "## Members by kind", ""] + [f"- {k}: {v}" for k, v in kinds.most_common()]
    L += ["", "## vcpp coverage", ""] + [f"- {s}: {by[s]}" for s, _ in by.most_common()]
    L += ["", f"Corpus uses {len(used)} members; {sum(1 for r in used if r['vcpp_status'] == 'wired')} are wired.", "",
          "## Gaps ranked by real usage", "", "| owner | member | kind | vcpp | programs | uses |", "|---|---|---|---|---|---|"]
    L += [f"| {r['owner']} | {r['member']} | {r['kind']} | {r['vcpp_status']} | {r['programs']} | {r['corpus_uses']} |" for r in gaps[:45]]
    L += ["", "## Per-owner coverage (user-facing members)", "", "| owner | members | wired | used | used & wired |", "|---|---|---|---|---|"]
    for o in owners:
        rs = [r for r in facing if r["owner"] == o]
        L.append(f"| {o} | {len(rs)} | {sum(r['vcpp_status'] == 'wired' for r in rs)} | {sum(1 for r in rs if r['programs'])} | "
                 f"{sum(1 for r in rs if r['programs'] and r['vcpp_status'] == 'wired')} |")
    L += ["", f"## In the runtime but never mentioned in the docs ({len(undoc)})", "",
          "These are what a docs-only catalogue (tools/parity.py) cannot see:", "",
          ", ".join(f"`{r['owner']}.{r['member']}`" for r in undoc[:60]),
          "", f"## Probable internal helpers ({len(helpers)}) — not documented, unused, not a built-in", "",
          ", ".join(f"`{r['owner']}.{r['member']}`" for r in helpers[:60])]
    open(os.path.join(OUT, "SUMMARY.md"), "w").write("\n".join(L) + "\n")
    open(os.path.join(OUT, "diagnostics.txt"), "w").write("\n".join(diag) + "\n")

    print(f"owners ({len(owners)}): {owners}")
    print(f"members: {len(rows)} total, {len(facing)} user-facing, {len(helpers)} likely helpers")
    print(f"user-facing kinds: {dict(kinds)}  vcpp: {dict(by)}")
    print(f"globals: {len(grows)}  corpus: {parsed}/{nfiles}")
    print("curve members:", sorted(r["member"] for r in rows if r["owner"] == "curve"))
    print("gcurve members:", sorted(r["member"] for r in rows if r["owner"] == "gcurve"))
    print("shapes:", sorted(r["member"] for r in rows if r["owner"] == "shapes"))
    print("color:", sorted(r["member"] for r in rows if r["owner"] == "color"))
    print("top gaps:", ", ".join(f"{r['owner']}.{r['member']}[{r['vcpp_status']}]({r['programs']})" for r in gaps[:12]))
    if diag:
        print("diagnostics:", diag)
    print(f"wrote {OUT}/members.csv, globals.csv, SUMMARY.md, diagnostics.txt")


if __name__ == "__main__":
    main()
