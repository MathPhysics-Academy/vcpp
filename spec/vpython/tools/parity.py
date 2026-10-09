#!/usr/bin/env python3
"""VPython -> vcpp parity table. See ../parity/METHOD.md for the full method.

Spec side:  GlowScript's reference docs (glowscript/docs/VPythonDocs) = Web VPython 3.2.
vcpp side:  vcpp/src/*.cppm — which names exist, and which are settable by name per object.
Usage side: corpus/*/*.vpy (official) + the student labs, with per-program type inference.

Writes parity/objects.csv, parity/methods.csv, parity/user_attrs.csv, parity/globals.csv,
parity/SUMMARY.md. Prints diagnostics so the extraction can be checked against the pages.
"""
import ast, collections, csv, glob, html, os, re

HOME = os.path.expanduser("~")
# Two roots — see catalog_runtime.py for the rationale. REF is the third-party material (outside this
# repo, never committed); SELF is this tree, where the generated tables belong.
SELF = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REF = os.environ.get("VPYTHON_REF") or os.path.join(HOME, "workspace/math-physics-academy/vpython-reference")
DOCS = os.path.join(REF, "glowscript/docs/VPythonDocs")
COMPILER = os.path.join(REF, "glowscript/lib/compiling/GScompiler.js")
VCPP_SRC = os.environ.get("VCPP_SRC") or os.path.join(HOME, "workspace/math-physics-academy/vcpp/src")
# The second pattern is private material (student labs, not redistributable).
CORPUS = [os.path.join(REF, "corpus/*/*.vpy"),
          os.environ.get("VPYTHON_CORPUS") or os.path.join(HOME, "workspace/pycode_similar/vpython_codes/**/*.py")]
OUT = os.path.join(SELF, "parity")

if not os.path.isdir(DOCS):
    raise SystemExit(f"no glowscript docs at {DOCS}\n"
                     f"Set VPYTHON_REF to the folder holding glowscript/ — third-party, kept outside this repo.")

# VPython object name -> vcpp factory name, where they differ (vcpp-objects.cppm: text3d()).
VCPP_FACTORY = {"text": "text3d"}
# (VPython object, VPython attr) -> vcpp symbol, for renames verified in vcpp source:
#   gcurve graph -> graph_ref : vcpp-graph-objects.cppm gcurve() handles is_bound<decltype(graph_ref)>
#   gcurve dot   -> show_dot  : param_spec<&gcurve_object::m_dot, decltype(show_dot), …>
#   text depth   -> thickness : param_spec<&text3d_object::m_depth, decltype(thickness), …>
ALIASES = {("gcurve", "graph"): "graph_ref", ("gcurve", "dot"): "show_dot", ("text", "depth"): "thickness"}

OBJECTS_3D = ["arrow", "box", "compound", "cone", "curve", "cylinder", "ellipsoid", "extrusion", "helix",
              "points", "pyramid", "ring", "sphere", "simple_sphere", "label", "text", "vertex", "triangle",
              "quad", "local_light", "distant_light"]
GRAPHS = ["graph", "gcurve", "gdots", "gvbars", "ghbars"]
WIDGETS = ["button", "slider", "checkbox", "radio", "menu", "winput", "wtext"]
KNOWN = set(OBJECTS_3D + GRAPHS + WIDGETS + ["canvas", "attach_trail", "attach_arrow", "attach_light", "group"])
# Doc names that describe positional arguments, not attributes (graph.html gc.plot, attacharrow.html, compound.html).
POSITIONAL = {"firstargument", "secondargument", "thirdargument", "first_argument"}
# Well-known receiver names in the docs: `scene` is VPython's default canvas (canvas.html, texture.html).
RECEIVERS = {"scene": "canvas"}
# Object page that documents each object, where the file name differs from the object name.
PAGE_OF = {"simple_sphere": "sphere.html", "triangle": "vertex.html", "quad": "vertex.html"}

ITEM_RE = re.compile(r"^(\w+)(\(\))?\s*(?:\(([^)]*)\))?\s*[–-]\s*(.*)$", re.S)


def text(s):
    return re.sub(r"\s+", " ", html.unescape(re.sub(r"<[^>]+>", "", s))).replace("¶", "").strip()


def default_of(desc):
    m = re.search(r"Default(?: is)?\s*(.+?)(?:\.\s|;|\.$|$)", desc)
    return m.group(1).strip() if m else ""


def items_of(dd_html):
    """Parameters block -> [(name, is_method, type, desc)]. One <li> per item; a lone item may be a bare <p>."""
    lis = re.findall(r"<li>(.*?)</li>", dd_html, re.S) or [dd_html]
    out = []
    for li in lis:
        m = ITEM_RE.match(text(li))
        if m:
            out.append((m.group(1), bool(m.group(2)), (m.group(3) or "").strip(), m.group(4).strip()))
    return out


# ---------------------------------------------------------------- spec: GlowScript docs
def parse_docs():
    spec = collections.defaultdict(dict)        # obj -> attr -> {type, default, page, readonly}
    methods = collections.defaultdict(dict)     # obj -> method -> {params, page}
    positional = collections.defaultdict(dict)  # obj -> name -> {type, desc}
    trail, standard, links, skipped = {}, {}, {}, []
    tok_re = re.compile(r'(<dt class="sig sig-object py"[^>]*>.*?</dt>)|'
                        r'<dt class="field-(?:odd|even)">(.*?)</dt>\s*<dd class="field-(?:odd|even)">(.*?)</dd>', re.S)
    for path in sorted(glob.glob(os.path.join(DOCS, "*.html"))):
        page = os.path.basename(path)
        t = open(path, encoding="utf-8").read()
        links[page] = set(re.findall(r'href="([A-Za-z_]+\.html)', t))
        if page == "standardAttributes.html":           # reference only; see METHOD.md for why not propagated
            fields = dict((text(k), text(v)) for k, v in re.findall(
                r'<dt class="field-(?:odd|even)">(.*?)</dt>\s*<dd class="field-(?:odd|even)">(.*?)</dd>', t, re.S))
            for k, v in fields.items():
                if k.startswith("param "):
                    a = k[6:].rstrip(":")
                    standard[a] = {"type": fields.get(f"type {a}:", ""), "default": default_of(v)}
            continue
        ctx, var2obj = None, {}                          # ctx = ("object", obj) | ("method", obj, meth)
        for m in tok_re.finditer(t):
            if m.group(1):
                sig = text(m.group(1))
                mm = re.match(r"\s*(?:(\w+)\s*=\s*)?(\w+)(?:\.(\w+))?\s*\(", sig)
                if not mm:
                    continue
                var, head, meth = mm.groups()
                if meth:                                 # gc.plot(x, y)  /  graph.get_selected()  /  scene.waitfor(...)
                    # Resolve the receiver from evidence only: a variable assigned earlier on this page,
                    # a well-known receiver (`scene`), the docs' `my<object>` naming (`mygraph.select()`),
                    # or an object name. Otherwise skip and report it — never guess.
                    my = re.fullmatch(r"my(\w+)", head)
                    obj = (var2obj.get(head) or RECEIVERS.get(head)
                           or (my.group(1) if my and my.group(1) in KNOWN else None)
                           or (head if head in KNOWN else None))
                    ctx = ("method", obj, meth) if obj else None
                    if obj: methods[obj].setdefault(meth, {"params": [], "page": page, "sig": sig})
                    else: skipped.append((page, sig))
                elif head in KNOWN:
                    ctx = ("object", head)
                    if var: var2obj[var] = head
                    if page != "trail.html":             # trail.html's examples are about trails, not sphere
                        for p in re.findall(r"[(,]\s*(\w+)\s*=", sig):
                            spec[head].setdefault(p, {"type": "", "default": "", "page": page, "readonly": False})
                else:
                    ctx = None
            elif ctx and text(m.group(2)).startswith("Parameters"):
                for name, is_meth, typ, desc in items_of(m.group(3)):
                    if ctx[0] == "method":
                        methods[ctx[1]][ctx[2]]["params"].append(name)
                    elif is_meth:
                        methods[ctx[1]].setdefault(name, {"params": [], "page": page, "sig": f"{name}()"})
                    elif name in POSITIONAL:
                        positional[ctx[1]][name] = {"type": typ, "desc": desc[:80]}
                    else:
                        entry = {"type": typ, "default": default_of(desc), "page": page,
                                 "readonly": "read-only" in desc.lower()}
                        if page == "trail.html": trail[name] = entry
                        else: spec[ctx[1]][name] = entry
    # Trail attributes go to exactly the objects whose own page links trail.html.
    trail_objs = [o for o in OBJECTS_3D if "trail.html" in links.get(PAGE_OF.get(o, f"{o}.html"), set())]
    for o in trail_objs:
        for a, d in trail.items():
            spec[o].setdefault(a, dict(d, page="trail.html"))
    return spec, methods, positional, standard, trail, trail_objs, skipped


# ---------------------------------------------------------------- vcpp side
def parse_vcpp():
    src = {os.path.basename(f): open(f).read() for f in glob.glob(os.path.join(VCPP_SRC, "*.cppm"))}
    allsrc = "\n".join(src.values())
    # Both spellings — see catalog_runtime.py: `symbol pos{}` and `symbol<> pos{}` are the same
    # declaration, and matching only one made declared-only depend on the checked-out branch.
    declared = set(re.findall(r"inline constexpr symbol(?:<>)?\s+(\w+)\{\}", src["vcpp-props.cppm"]))
    structs = {}                                         # struct -> (base, own fields)
    for m in re.finditer(r"struct\s+(\w+)\s*(?::\s*(?:public\s+)?(\w+))?\s*\{(.*?)\n\};", allsrc, re.S):
        structs[m.group(1)] = (m.group(2), set(re.findall(r"\bm_(\w+)\s*(?:\{|=|;)", m.group(3))))

    def fields(s):
        base, f = structs.get(s, (None, set()))
        return f | (fields(base) if base else set())

    wired = collections.defaultdict(set)                 # struct -> symbols settable by name
    for m in re.finditer(r"struct object_params<(\w+)>\s*\{(.*?)\n\};", allsrc, re.S):
        wired[m.group(1)] |= set(re.findall(r"decltype\((?:vcpp::)?(?:prop::)?(\w+)\)", m.group(2)))
    common = set(re.findall(r"decltype\((\w+)\)",
                            re.search(r"inline constexpr auto common_params\s*=(.*?);\n", allsrc, re.S).group(1)))
    factories = {}                                       # factory name -> struct
    for m in re.finditer(r"^(?:constexpr\s+|inline\s+)*(\w+_object)\s+(\w+)\s*\(([^)]*)\)\s*\{(.*?)^\}", allsrc, re.S | re.M):
        struct, fname, body = m.group(1), m.group(2), m.group(4)
        factories[fname] = struct
        wired[struct] |= set(re.findall(r"is_bound<decltype\((?:vcpp::)?(?:prop::)?(\w+)\)", body))  # manual handling
    for struct in set(factories.values()):
        if structs.get(struct, (None,))[0] == "object_base":   # make<T>() applies common_params to object_base
            wired[struct] |= common
    return dict(declared=declared, structs=structs, fields=fields, wired=wired, factories=factories, common=common)


def vcpp_status(obj, attr, V):
    struct = V["factories"].get(VCPP_FACTORY.get(obj, obj))
    name = ALIASES.get((obj, attr), attr)
    if not struct:
        return "no-object", "", name
    if name in V["wired"][struct]:
        return "wired", struct, name
    if name in V["fields"](struct):
        return "field-only", struct, name
    if name in V["declared"]:
        return "declared-only", struct, name
    return "missing", struct, name


# ---------------------------------------------------------------- usage side
def source_of(path):
    lines = open(path, encoding="utf-8", errors="replace").read().splitlines()
    if lines and re.match(r"\s*#?\s*(Web VPython|VPython|GlowScript)\b", lines[0]):
        lines = lines[1:]
    return "\n".join(lines)


def parse_corpus(spec):
    U = dict(kw=collections.Counter(), assign=collections.Counter(), read=collections.Counter(),
             progs=collections.defaultdict(set), user_attr=collections.Counter(), user_progs=collections.defaultdict(set),
             global_calls=collections.Counter(), global_progs=collections.defaultdict(set),
             skipped_jquery=[], syntax_errors=[])
    files = sorted({f for p in CORPUS for f in glob.glob(p, recursive=True)})
    U["files"] = len(files); parsed = 0
    for f in files:
        src, name = source_of(f), os.path.basename(f)
        if "$(" in src:
            U["skipped_jquery"].append(name); continue
        try:
            tree = ast.parse(src)
        except SyntaxError as e:
            U["syntax_errors"].append(f"{name}: line {e.lineno}: {e.msg}"); continue
        parsed += 1
        types = {}

        def ctor(node):
            return node.func.id if isinstance(node, ast.Call) and isinstance(node.func, ast.Name) and node.func.id in KNOWN else None

        for n in ast.walk(tree):                           # pass 1: variable -> constructor (x = sphere(...))
            if isinstance(n, ast.Assign) and ctor(n.value):
                for t in n.targets:
                    if isinstance(t, ast.Name): types[t.id] = ctor(n.value)

        def count(key, bucket):
            if key[1] in spec.get(key[0], {}): U[bucket][key] += 1; U["progs"][key].add(name)
            elif bucket != "read": U["user_attr"][key] += 1; U["user_progs"][key].add(name)

        for n in ast.walk(tree):                           # pass 2: usage
            if isinstance(n, ast.Call) and isinstance(n.func, ast.Name):
                U["global_calls"][n.func.id] += 1; U["global_progs"][n.func.id].add(name)
                if n.func.id in KNOWN:
                    for k in n.keywords:
                        if k.arg: count((n.func.id, k.arg), "kw")
            if isinstance(n, (ast.Assign, ast.AugAssign)):
                for t in (n.targets if isinstance(n, ast.Assign) else [n.target]):
                    if isinstance(t, ast.Attribute) and isinstance(t.value, ast.Name) and t.value.id in types:
                        count((types[t.value.id], t.attr), "assign")
            if isinstance(n, ast.Attribute) and isinstance(n.ctx, ast.Load) and isinstance(n.value, ast.Name) and n.value.id in types:
                count((types[n.value.id], n.attr), "read")
    U["parsed"] = parsed
    return U


# ---------------------------------------------------------------- globals
def parse_globals():
    comp = open(COMPILER).read()
    names = re.findall(r'"(\w+)"', re.search(r"const vp_primitives = \[(.*?)\]", comp, re.S).group(1))
    vec_page = open(os.path.join(DOCS, "vector.html"), encoding="utf-8").read()
    vec_funcs = []
    for s in re.findall(r'<dt class="sig sig-object py"[^>]*>(.*?)</dt>', vec_page, re.S):
        m = re.match(r"(?:\w+\s*=\s*)?(?:\w+\.)?(\w+)\s*\(", text(s))
        if m and m.group(1) not in vec_funcs: vec_funcs.append(m.group(1))
    vcpp_all = "\n".join(open(f).read() for f in glob.glob(os.path.join(VCPP_SRC, "*.cppm")))
    # Comments mention names they don't declare ("range times exp(...)")
    vcpp_all = re.sub(r"/\*.*?\*/|//[^\n]*", "", vcpp_all, flags=re.S)

    def has(n):  # heuristic: a function/object/constant/namespace of that name is declared in vcpp/src
        n2 = VCPP_FACTORY.get(n, n)
        return bool(re.search(rf"\b[\w:<>]+\s+{n2}\s*\(", vcpp_all)
                    or re.search(rf"\bnamespace\s+{n2}\b|\binline constexpr [\w:<>]+\s+{n2}\b", vcpp_all))
    rows = [(n, "builtin (GScompiler.js vp_primitives)", has(n)) for n in dict.fromkeys(names)]
    rows += [(n, "vector function/method (vector.html)", has(n)) for n in vec_funcs if n not in names]
    return rows


# ---------------------------------------------------------------- main
def main():
    os.makedirs(OUT, exist_ok=True)
    spec, methods, positional, standard, trail, trail_objs, skipped = parse_docs()
    V = parse_vcpp()
    U = parse_corpus(spec)
    G = parse_globals()

    print(f"spec: {len(spec)} objects, {sum(len(a) for a in spec.values())} (object, attribute) pairs")
    for o in sorted(spec):
        print(f"  {o:14} {len(spec[o]):3}  {' '.join(sorted(spec[o]))}")
    print(f"trail attrs {sorted(trail)} -> applied to {trail_objs}")
    print(f"positional (not attributes): { {o: sorted(p) for o, p in positional.items()} }")
    print(f"methods: { {o: sorted(m) for o, m in methods.items()} }")
    print(f"method signatures skipped (receiver unresolved): {len(skipped)}")
    for p, s in skipped:
        print(f"    {p}: {s[:90]}")
    # Record page + callee only. The signature text is GlowScript's own documentation prose, and this
    # file is committed — so the open-work list ships without carrying third-party text into the repo.
    def callee(sig):
        m = re.search(r"([A-Za-z_][\w.]*)\s*\(", sig)
        return m.group(1) if m else "?"
    with open(os.path.join(OUT, "skipped_signatures.txt"), "w") as fh:
        fh.write("# Method signatures whose receiver could not be resolved from evidence (METHOD.md rule 2).\n"
                 "# Page and callee only: the documented signature text is third-party and not copied here.\n"
                 "# Review by hand; add a rule only with evidence from the pages.\n")
        fh.writelines(f"{p}\t{callee(s)}\n" for p, s in skipped)
    print(f"vcpp: {len(V['factories'])} factories; common_params {sorted(V['common'])}")
    print(f"corpus: {U['parsed']}/{U['files']} parsed; jQuery skipped {U['skipped_jquery']}; syntax errors {U['syntax_errors']}")

    rows = []
    for obj in sorted(spec):
        for attr in sorted(spec[obj]):
            st, struct, vname = vcpp_status(obj, attr, V)
            k = (obj, attr); d = spec[obj][attr]
            rows.append(dict(object=obj, attr=attr, doc_type=d["type"], doc_default=d["default"], doc_page=d["page"],
                             read_only="yes" if d.get("readonly") else "", vcpp_struct=struct, vcpp_name=vname,
                             vcpp_status=st, kwarg_uses=U["kw"][k], assign_uses=U["assign"][k], read_uses=U["read"][k],
                             programs=len(U["progs"][k])))
    with open(os.path.join(OUT, "objects.csv"), "w", newline="") as fh:
        w = csv.DictWriter(fh, fieldnames=list(rows[0])); w.writeheader(); w.writerows(rows)
    with open(os.path.join(OUT, "methods.csv"), "w", newline="") as fh:
        w = csv.writer(fh); w.writerow(["object", "method", "params", "doc_page", "doc_sig"])
        for o in sorted(methods):
            for mth, d in sorted(methods[o].items()):
                w.writerow([o, mth, " ".join(d["params"]), d["page"], d.get("sig", "")])
    with open(os.path.join(OUT, "user_attrs.csv"), "w", newline="") as fh:
        w = csv.writer(fh); w.writerow(["object", "attr", "uses", "programs"])
        for (o, a), n in U["user_attr"].most_common(): w.writerow([o, a, n, len(U["user_progs"][(o, a)])])
    with open(os.path.join(OUT, "globals.csv"), "w", newline="") as fh:
        w = csv.writer(fh); w.writerow(["name", "kind", "vcpp_has", "corpus_calls", "programs"])
        for n, kind, h in G: w.writerow([n, kind, "yes" if h else "no", U["global_calls"][n], len(U["global_progs"][n])])

    spec_names = {a for o in spec for a in spec[o]} | set(ALIASES.values())
    vcpp_only = sorted(V["declared"] - spec_names)
    by = collections.Counter(r["vcpp_status"] for r in rows)
    used = [r for r in rows if r["programs"]]
    gaps = sorted((r for r in used if r["vcpp_status"] != "wired"),
                  key=lambda r: (-r["programs"], -(r["kwarg_uses"] + r["assign_uses"] + r["read_uses"])))
    L = ["# VPython → vcpp parity", "",
         "Generated by `tools/parity.py` — method and caveats in `parity/METHOD.md`. "
         f"Spec: GlowScript docs (Web VPython 3.2). vcpp: `vcpp/src`. Usage: {U['parsed']} of {U['files']} programs parsed.", "",
         "Status: **wired** settable by name · **field-only** member exists, `obj(name=…)` ignores it · "
         "**declared-only** symbol exists, nothing backs it · **missing** neither · **no-object** vcpp has no factory for it.", "",
         f"## Totals ({len(rows)} object/attribute pairs)", ""]
    L += [f"- {s}: {by[s]}" for s in ["wired", "field-only", "declared-only", "missing", "no-object"]]
    L += ["", f"The corpus uses {len(used)} of these pairs; {sum(1 for r in used if r['vcpp_status'] == 'wired')} are wired.", "",
          "## Gaps ranked by real usage", "", "| object | attribute | vcpp status | programs | ctor kwargs | assigns | reads |",
          "|---|---|---|---|---|---|---|"]
    L += [f"| {r['object']} | {r['attr']} | {r['vcpp_status']} | {r['programs']} | {r['kwarg_uses']} | {r['assign_uses']} | {r['read_uses']} |"
          for r in gaps[:40]]
    L += ["", "## Per-object coverage", "", "| object | attrs in spec | wired | used by corpus | used & wired |", "|---|---|---|---|---|"]
    for obj in sorted(spec):
        rs = [r for r in rows if r["object"] == obj]
        L.append(f"| {obj} | {len(rs)} | {sum(r['vcpp_status'] == 'wired' for r in rs)} | {sum(1 for r in rs if r['programs'])} | "
                 f"{sum(1 for r in rs if r['programs'] and r['vcpp_status'] == 'wired')} |")
    L += ["", "## vcpp names the GlowScript docs don't have", "",
          "Declared in `vcpp-props.cppm` but not a documented attribute of any object (some exist in the runtime but are "
          "undocumented, e.g. `trail_color`; others are vcpp's own):", "", ", ".join(f"`{n}`" for n in vcpp_only)]
    L += ["", "## User-defined attributes (not in the spec) — the attribute-bag case", "",
          ", ".join(f"`{o}.{a}` ({n})" for (o, a), n in U["user_attr"].most_common(30))]
    L += ["", "## Globals (builtins + vector functions)", "", "| name | kind | vcpp has | programs |", "|---|---|---|---|"]
    L += [f"| {n} | {k} | {'yes' if h else '**no**'} | {len(U['global_progs'][n])} |" for n, k, h in G]
    L += ["", "## Methods documented per object", ""] + [f"- {o}: {', '.join(sorted(m))}" for o, m in sorted(methods.items())]
    open(os.path.join(OUT, "SUMMARY.md"), "w").write("\n".join(L) + "\n")

    print(f"\nstatus totals: {dict(by)}")
    print(f"used pairs: {len(used)}, wired among used: {sum(1 for r in used if r['vcpp_status'] == 'wired')}")
    print("top gaps:", ", ".join(f"{r['object']}.{r['attr']}[{r['vcpp_status']}]({r['programs']})" for r in gaps[:15]))
    print(f"vcpp-only names: {vcpp_only}")
    print(f"wrote {OUT}/objects.csv, methods.csv, user_attrs.csv, globals.csv, SUMMARY.md")


if __name__ == "__main__":
    main()
