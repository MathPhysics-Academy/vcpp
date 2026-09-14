#!/usr/bin/env python3
"""Feature usage across Web VPython programs.

usage: corpus.py [dir_or_glob ...]   (default: ../corpus/*/*.vpy plus the student labs)
Strips the 'Web VPython 3.2' header line; programs using jQuery `$(` are reported, not parsed.
"""
import ast, collections, glob, os, re, sys

HERE = os.path.dirname(os.path.abspath(__file__))
# The corpus is third-party and lives outside this repo, under VPYTHON_REF.
REF = os.environ.get("VPYTHON_REF") or os.path.expanduser("~/workspace/math-physics-academy/vpython-reference")
DEFAULT = [os.path.join(REF, "corpus", "*", "*.vpy"),
           os.environ.get("VPYTHON_CORPUS") or os.path.expanduser("~/workspace/pycode_similar/vpython_codes/**/*.py")]

def source_of(path):
    lines = open(path, encoding="utf-8", errors="replace").read().splitlines()
    if lines and re.match(r"\s*#?\s*(Web VPython|VPython|GlowScript)\b", lines[0]):
        lines = lines[1:]
    return "\n".join(lines)

def main(patterns):
    files = sorted({f for p in patterns for f in glob.glob(p, recursive=True)})
    calls, kw, setattrs, nodes = collections.Counter(), collections.defaultdict(collections.Counter), collections.Counter(), collections.Counter()
    users, jquery, bad = collections.defaultdict(set), [], []
    for f in files:
        src, name = source_of(f), os.path.basename(f)
        if "$(" in src:
            jquery.append(name); continue
        try:
            tree = ast.parse(src)
        except SyntaxError as e:
            bad.append((name, f"line {e.lineno}: {e.msg}")); continue
        for n in ast.walk(tree):
            nodes[type(n).__name__] += 1
            if isinstance(n, ast.Call):
                c = n.func.id if isinstance(n.func, ast.Name) else (n.func.attr if isinstance(n.func, ast.Attribute) else "?")
                calls[c] += 1; users[c].add(name)
                for k in n.keywords:
                    if k.arg: kw[c][k.arg] += 1
            if isinstance(n, (ast.Assign, ast.AugAssign)):
                for t in (n.targets if isinstance(n, ast.Assign) else [n.target]):
                    if isinstance(t, ast.Attribute): setattrs[t.attr] += 1
    parsed = len(files) - len(jquery) - len(bad)
    print(f"files: {len(files)}  parsed: {parsed}  jQuery (skipped): {len(jquery)}  syntax errors: {len(bad)}")
    for b in bad: print("  syntax error:", *b)
    for j in jquery: print("  uses jQuery:", j)
    keep = ["While", "For", "If", "FunctionDef", "ClassDef", "Lambda", "ListComp", "List", "Dict", "Global", "Return"]
    print("structure:", {k: nodes[k] for k in keep if nodes[k]})
    print("calls (programs using):", ", ".join(f"{c}({len(s)})" for c, s in sorted(users.items(), key=lambda x: -len(x[1]))[:60]))
    for c in ["sphere", "box", "cylinder", "cone", "pyramid", "ellipsoid", "ring", "helix", "arrow", "curve", "points",
              "label", "text", "compound", "extrusion", "graph", "gcurve", "gdots", "gvbars", "button", "slider", "menu"]:
        if kw[c]: print(f"  {c}: {dict(kw[c].most_common())}")
    print("attributes assigned:", ", ".join(f"{k}({v})" for k, v in setattrs.most_common(40)))

if __name__ == "__main__":
    main(sys.argv[1:] or DEFAULT)
