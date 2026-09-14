#!/usr/bin/env python3
"""Convention-free ground truth about GlowScript's runtime, for INDEPENDENT reproduction.

Why this exists: `catalog_runtime.py` produces 1,207 members over 48 owners, but that total encodes
choices (what counts as an owner, which internals to drop, how to name `Mouse.prototype`). Someone
writing their own extractor will make different choices and get a different total — and a different
total is not evidence either side is wrong.

So this script measures only things that are TRUE OF THE SOURCE, independent of any convention:
raw counts of declare blocks, subclass edges, exports entries, constructors. Any correct extraction
must account for all of them. Compare your numbers against these, not against ours.

Reads ONLY the pinned glowscript checkout. No vcpp, no corpus, no docs, no CSVs — so anyone can run
it with nothing but the public clone:

    git clone https://github.com/vpython/glowscript && git -C glowscript checkout 744de98
    python3 tools/invariants.py
"""
import collections, glob, os, re, sys

REF = os.environ.get("VPYTHON_REF") or os.path.join(os.path.expanduser("~"),
                                                    "workspace/math-physics-academy/vpython-reference")
GLOW = os.path.join(REF, "glowscript/lib/glow")
EXTRUDE = os.path.join(GLOW, "extrude.js")
COMPILER = os.path.join(REF, "glowscript/lib/compiling/GScompiler.js")

if not os.path.isdir(GLOW):
    sys.exit(f"no glowscript checkout at {GLOW} (set VPYTHON_REF)")

files = {os.path.basename(p): open(p, encoding="utf-8", errors="replace").read()
         for p in sorted(glob.glob(os.path.join(GLOW, "*.js"))) + [EXTRUDE, COMPILER]}

print("=" * 78)
print("GlowScript runtime invariants — compare your own extraction against these")
print("=" * 78)

# 1. property.declare blocks: the primary source of members.
targets = []
for fn, s in files.items():
    for m in re.finditer(r"property\.declare\(\s*([\w.]+)\s*,", s):
        targets.append((fn, m.group(1)))
print(f"\n1. property.declare() blocks: {len(targets)}")
for fn, t in sorted(targets):
    print(f"     {t:34} {fn}")

# 2. subclass() edges: the inheritance graph.
edges = []
for fn, s in files.items():
    for m in re.finditer(r"subclass\(\s*(\w+)\s*,\s*(\w+)\s*\)", s):
        # ("sub", "base") is the parameter list where subclass() is DEFINED, not an instance of it.
        # Counting it as an edge sends you looking for classes that do not exist.
        if (m.group(1), m.group(2)) == ("sub", "base"):
            continue
        edges.append((m.group(1), m.group(2)))
print(f"\n2. subclass(child, base) edges: {len(edges)}")
for c, b in sorted(edges):
    print(f"     {c:20} <- {b}")

# 3. exports blocks: the names the runtime publishes.
print("\n3. exports blocks (raw, unfiltered):")
total_exports = 0
for fn, s in sorted(files.items()):
    for m in re.finditer(r"(?:var|let|const)\s+exports\s*=\s*\{", s):
        depth, i = 0, m.end() - 1
        while i < len(s):
            if s[i] == "{":
                depth += 1
            elif s[i] == "}":
                depth -= 1
                if depth == 0:
                    break
            i += 1
        names = re.findall(r"(?:^|[{,])\s*(\w+)\s*:", s[m.end() - 1:i + 1])
        total_exports += len(names)
        print(f"     {fn:22} {len(names):3} names")
print(f"     {'TOTAL':22} {total_exports:3}")

# 4. Constructors that accept an options object.
ctors = set()
for fn in ("primitives.js", "graph.js", "canvas.js", "extrude.js"):
    s = files[fn]
    for m in re.finditer(r"^[ \t]*function (\w+)\s*\(([^)]*)\)", s, re.M):
        if re.search(r"\b(args|options|objects|parameters)\b", m.group(2)):
            ctors.add(m.group(1))
print(f"\n4. constructors taking args/options/parameters: {len(ctors)}")
print("     " + " ".join(sorted(ctors)))

# 5. The shared init() option set — applied to every object whose constructor calls it.
m = re.search(r"^[ \t]*function init\(obj, args\)", files["primitives.js"], re.M)
if m:
    b = files["primitives.js"].find("{", m.end())
    depth, i = 0, b
    s = files["primitives.js"]
    while i < len(s):
        if s[i] == "{":
            depth += 1
        elif s[i] == "}":
            depth -= 1
            if depth == 0:
                break
        i += 1
    opts = sorted({n for n in re.findall(r"\bargs\.(\w+)", s[b:i]) if not n.startswith("_")})
    print(f"\n5. shared init() options: {len(opts)}")
    print("     " + " ".join(opts))

# 6. Compiler built-ins: names usable without import.
s = files["GScompiler.js"]
prim = re.search(r"const vp_primitives = \[(.*?)\]", s, re.S)
prims = re.findall(r'"(\w+)"', prim.group(1)) if prim else []
print(f"\n6. GScompiler vp_primitives: {len(prims)}")
print("     " + " ".join(prims))

# 7. Event objects have NO declaration — this is the fact that forces hand-enumeration.
tr = re.search(r"^[ \t]*trigger: async function", files["canvas.js"], re.M)
line = files["canvas.js"].count("\n", 0, tr.start()) + 1 if tr else -1
print(f"\n7. canvas.trigger() at canvas.js:{line} — builds event objects as literals.")
print("     There is no declare block, prototype or export for them. Any extractor that")
print("     reads declarations WILL miss event objects; they must be enumerated by hand.")

print("\n" + "=" * 78)
print("Expect these to be stable at glowscript 744de98. If your extraction cannot account")
print("for every declare block, subclass edge and exports name above, it has a gap.")
print("Totals that differ from catalog/members.csv are expected — see catalog/AUDIT.md.")
