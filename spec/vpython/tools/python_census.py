import ast, collections, csv, glob, os, sys
# SELF = this tree (tools + the committed CSVs); REF = third-party material, outside the repo.
SELF = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REF = os.environ.get("VPYTHON_REF") or os.path.expanduser("~/workspace/math-physics-academy/vpython-reference")
sys.path.insert(0, os.path.join(SELF, "tools"))
import catalog_runtime as C

vp_globals = {r["name"] for r in csv.DictReader(open(os.path.join(SELF, "catalog/globals.csv")))}
vp_members = collections.defaultdict(set)
vp_owners = set()
for r in csv.DictReader(open(os.path.join(SELF, "catalog/members.csv"))):
    vp_owners.add(r["owner"]); vp_members[r["owner"]].add(r["member"])
vp_any_member = set().union(*vp_members.values())

nodes = collections.Counter(); nprog = collections.defaultdict(set)
callnames = collections.Counter(); cprog = collections.defaultdict(set)
attrcalls = collections.Counter(); imports = collections.Counter()
defined = set(); parsed = 0; skipped = []

files = sorted({f for p in C.CORPUS for f in glob.glob(p, recursive=True)})
for f in files:
    src, nm = C.source_of(f), os.path.basename(f)
    if "$(" in src:
        skipped.append((nm, "jQuery")); continue
    try:
        tree = ast.parse(src)
    except SyntaxError as e:
        skipped.append((nm, f"SyntaxError: {e.msg}")); continue
    parsed += 1
    for n in ast.walk(tree):
        nodes[type(n).__name__] += 1; nprog[type(n).__name__].add(nm)
        if isinstance(n, (ast.FunctionDef, ast.AsyncFunctionDef, ast.ClassDef)):
            defined.add(n.name)
        if isinstance(n, ast.Call):
            if isinstance(n.func, ast.Name):
                callnames[n.func.id] += 1; cprog[n.func.id].add(nm)
            elif isinstance(n.func, ast.Attribute):
                attrcalls[n.func.attr] += 1
        if isinstance(n, ast.Import):
            for a in n.names: imports[a.name] += 1
        if isinstance(n, ast.ImportFrom):
            imports[n.module or "?"] += 1

print(f"parsed {parsed} of {len(files)} programs; skipped {len(skipped)}")
for nm, why in skipped: print(f"   skip {nm}: {why}")

LANG = ["ClassDef","FunctionDef","Lambda","Return","Yield","GeneratorExp","ListComp","DictComp","SetComp",
        "Try","ExceptHandler","Raise","With","Assert","Global","Nonlocal","Del","While","For","If",
        "IfExp","BoolOp","Compare","AugAssign","Subscript","Slice","Starred","JoinedStr","FormattedValue",
        "Dict","Set","Tuple","List","Break","Continue","Import","ImportFrom","Await","AsyncFunctionDef"]
print("\n=== Python language constructs used (count / programs)")
for k in LANG:
    if nodes[k]: print(f"  {k:18} {nodes[k]:5}  in {len(nprog[k])} programs")
print("\n  NOT used at all:", ", ".join(k for k in LANG if not nodes[k]))

print(f"\n=== imports: {dict(imports) or 'none'}")

nonvp = {k: v for k, v in callnames.items()
         if k not in vp_globals and k not in vp_owners and k not in defined}
print(f"\n=== called names that are neither VPython nor user-defined ({len(nonvp)}) — i.e. Python builtins/stdlib")
for k, v in sorted(nonvp.items(), key=lambda x: -x[1]):
    print(f"  {k:18} {v:4}  in {len(cprog[k])} programs")

meth = {k: v for k, v in attrcalls.items() if k not in vp_any_member}
print(f"\n=== method calls not explained by the VPython catalogue ({len(meth)})")
print(" ", ", ".join(f"{k}({v})" for k, v in sorted(meth.items(), key=lambda x: -x[1])))
