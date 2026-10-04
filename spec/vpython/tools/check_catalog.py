#!/usr/bin/env python3
"""Checks on catalog/ — the table in catalog/METHOD.md §7, made executable.

Why this exists: the checks used to be prose, run by hand. A set-literal edit once moved four base
classes into the owner list (owners 47->49, members 1,194->1,241) while every *content* check still
passed, because nothing checked the shape of the output. The regression guards below are that
missing check. Run after any change to tools/catalog_runtime.py:

    python3 tools/catalog_runtime.py && python3 tools/check_catalog.py

Exit status is non-zero if anything fails.
"""
import collections, csv, os, re, sys

# SELF is this tree (the committed tables); REF is the third-party glowscript checkout, which lives
# outside the repo. The citation checks at the bottom need REF — without it they cannot fail, so the
# run warns rather than passing 36/36 on a vacuous reading.
SELF = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REF = os.environ.get("VPYTHON_REF") or os.path.join(os.path.expanduser("~"),
                                                    "workspace/math-physics-academy/vpython-reference")
CAT = os.path.join(SELF, "catalog")
PAR = os.path.join(SELF, "parity")

results = []


def check(name, ok, detail=""):
    results.append((bool(ok), name, detail))


def load(path):
    with open(path, newline="") as fh:
        return list(csv.DictReader(fh))


members = load(os.path.join(CAT, "members.csv"))
globals_ = load(os.path.join(CAT, "globals.csv"))
by_owner = collections.defaultdict(dict)
for r in members:
    by_owner[r["owner"]][r["member"]] = r
gnames = {g["name"] for g in globals_}


def row(owner, member):
    return by_owner.get(owner, {}).get(member)


# ---------------------------------------------------------------- regression guards
INTERNAL_BASES = ["Primitive", "gobject", "shape_object", "path_object"]
leaked = [b for b in INTERNAL_BASES if b in by_owner]
check("internal base classes are not owners", not leaked, f"leaked: {leaked}")
check("owner count == 48", len(by_owner) == 48, f"got {len(by_owner)}")
check("member count in 1,150-1,260", 1150 <= len(members) <= 1260, f"got {len(members)}")

# ---------------------------------------------------------------- content
CURVE_METHODS = ["append", "modify", "clear", "point", "slice", "splice", "pop", "shift", "unshift", "push"]
missing = [m for m in CURVE_METHODS if not row("curve", m) or row("curve", m)["kind"] != "method"]
check("curve methods present and kind=method", not missing, f"missing/wrong: {missing}")

r = row("curve", "npoints")
check("curve.npoints is an attribute (accessor), not a method",
      r and r["kind"] == "attribute" and "accessor" in r["note"], r and f"{r['kind']}/{r['note']}")

r = row("curve", "pos")
check("curve.pos kind=unsupported (its getter throws)", r and r["kind"] == "unsupported",
      r and r["kind"])

for o in ("cone", "pyramid", "group"):
    check(f"{o} has >10 members (via subclass)", len(by_owner.get(o, {})) > 10,
          f"got {len(by_owner.get(o, {}))}")

r = row("cone", "radius")
check("cone.radius inherited_from=cylinder", r and r["inherited_from"] == "cylinder",
      r and r["inherited_from"])

check("mouse is an owner with >=5 members", len(by_owner.get("mouse", {})) >= 5,
      f"got {len(by_owner.get('mouse', {}))}")
check("camera is an owner with >=3 members", len(by_owner.get("camera", {})) >= 3,
      f"got {len(by_owner.get('camera', {}))}")

for o in ("label", "sphere"):
    r = row(o, "canvas")
    check(f"{o}.canvas note='constructor option (shared init)'",
          r and r["note"] == "constructor option (shared init)", r and r["note"])

r = row("texture", "bumpmap")
check("texture.bumpmap note='texture sub-option'", r and r["note"] == "texture sub-option",
      r and r["note"])

r = row("graph", "fast")
check("graph.fast note='constructor option'", r and r["note"] == "constructor option", r and r["note"])

check("canvas has >=34 members", len(by_owner.get("canvas", {})) >= 34,
      f"got {len(by_owner.get('canvas', {}))}")

for n in ("extrusion", "keysdown", "draw"):
    check(f"{n} is in globals.csv", n in gnames)
for n in ("fieldset", "aria_div", "ghistogram"):
    check(f"{n} is NOT in globals.csv", n not in gnames)

r = row("shapes", "roundc")
check("shapes.roundc user_facing=no", r and r["user_facing"] == "no", r and r["user_facing"])

r = row("vec", "add")
check("vec.add vcpp_name=operator+", r and r["vcpp_name"] == "operator+", r and r["vcpp_name"])

r = row("gcurve", "data")
check("gcurve.data vcpp_status=field-only", r and r["vcpp_status"] == "field-only", r and r["vcpp_status"])

r = row("gcurve", "graph")
check("gcurve.graph vcpp_name=graph_ref", r and r["vcpp_name"] == "graph_ref", r and r["vcpp_name"])

# A purely cosmetic edit in vcpp — `symbol<> pos{}` becoming `symbol pos{}` — once made the
# declared-prop regex match nothing, moving 96 rows from declared-only to missing while every other
# check still passed. Guard that the column is POPULATED, not that it equals a fixed total: wiring a
# prop legitimately moves rows out of declared-only and must not fail the run. With no vcpp checkout
# the whole column is blank and this passes vacuously, which is the documented degraded mode.
have_vcpp = any(r["vcpp_status"] for r in members)
dcl = sum(1 for r in members if r["vcpp_status"] == "declared-only")
check("declared-only is populated (vcpp props actually parsed)", (not have_vcpp) or dcl > 0,
      f"declared-only={dcl} while vcpp columns are filled — did vcpp-props.cppm change spelling?")

# ---------------------------------------------------------------- event objects (hand-enumerated)
# These guard the shape of catalog_runtime.EVENT_MEMBERS. Its *meanings* are guarded only by
# catalog/EVENTS.md and a human re-reading canvas.js:556-590.
EVENT_EXPECTED = {"type", "event", "canvas", "pageX", "pageY", "which", "pos", "press", "release",
                  "key", "alt", "ctrl", "shift"}
ev = by_owner.get("event", {})
check("event is an owner", bool(ev))
check("event has exactly the 13 enumerated members", set(ev) == EVENT_EXPECTED,
      f"extra={sorted(set(ev) - EVENT_EXPECTED)} missing={sorted(EVENT_EXPECTED - set(ev))}")
bad = [m for m, r in ev.items() if not r["runtime_source"].startswith("canvas.js:")]
check("every event row cites canvas.js", not bad, f"not cited: {bad}")

# A citation that merely *starts with* canvas.js is not evidence. Open every cited line and require
# the member's name on EACH of them. Two citations were off by one line before this check existed,
# and every other check still passed. "Any line matches" is not enough: it lets a wrong line hide
# behind correct siblings, which is exactly how `event`'s bad citation survived the first version.
CANVAS = os.path.join(REF, "glowscript/lib/glow/canvas.js")
if os.path.exists(CANVAS):
    src = open(CANVAS, encoding="utf-8", errors="replace").read().splitlines()
    bad = []
    for m, r in sorted(ev.items()):
        nums = [int(x) for x in r["runtime_source"].partition(":")[2].split(",") if x.strip().isdigit()]
        wrong = [n for n in nums if not (0 < n <= len(src) and m in src[n - 1])]
        if not nums or wrong:
            bad.append(f"{m}@{r['runtime_source']}" + (f" (bad lines {wrong})" if wrong else " (no line cited)"))
    check("every cited line names the member it is cited for", not bad, f"wrong: {bad}")
else:
    print("NOTE  event citations not verified — no glowscript checkout at "
          f"{os.path.relpath(CANVAS, REF)}")
bad = [m for m, r in ev.items() if r["vcpp_status"] != "no-object"]
check("every event member is no-object in vcpp", not bad, f"unexpected: {bad}")
# Widget callbacks receive the widget, not an event — these must never migrate onto `event`.
WIDGET_ONLY = {"checked", "index", "selected", "value", "text", "disabled"}
bad = sorted(WIDGET_ONLY & set(ev))
check("widget properties are not on the event object", not bad, f"leaked: {bad}")

# ---------------------------------------------------------------- citations point at something
# A blank cited line means the anchor regex matched one line early. `^\s*` under re.M does exactly
# that, because \s matches \n, so the match begins on the PREVIOUS line's terminator. That bug put
# 148 rows one line off while all 34 other checks passed — the event-citation check didn't catch it
# either, because it falls back to a brace-matched block search that still lands in the right body.
GLOWDIR = os.path.join(REF, "glowscript/lib/glow")
if not os.path.isdir(GLOWDIR):
    print(f"WARNING: no glowscript checkout at {GLOWDIR} — the citation checks below read no source "
          f"and cannot fail. Set VPYTHON_REF to make them meaningful.", file=sys.stderr)
_src = {}


def _lines(fn):
    if fn not in _src:
        p = os.path.join(GLOWDIR, fn)
        _src[fn] = open(p, encoding="utf-8", errors="replace").read().splitlines() if os.path.exists(p) else []
    return _src[fn]


blank = []
for r in members:
    fn, _, rest = r["runtime_source"].partition(":")
    L = _lines(fn)
    if not L:
        continue
    for n in (int(x) for x in re.findall(r"\d+", rest)):
        if 0 < n <= len(L) and not L[n - 1].strip():
            blank.append(f"{r['owner']}.{r['member']}@{r['runtime_source']}")
            break
check("no row cites a blank line", not blank, f"{len(blank)} rows, e.g. {blank[:4]}")

# ---------------------------------------------------------------- cross-check against the docs table
# The independent check that the runtime catalogue isn't missing documented API. The two known
# exceptions are docs-vs-runtime discrepancies, both hand-verified (METHOD.md §6).
KNOWN = {("canvas", "visible"), ("triangle", "bumpmap")}
pobj = os.path.join(PAR, "objects.csv")
if os.path.exists(pobj):
    pairs = {(r["object"], r["attr"]) for r in load(pobj)}
    have = {(r["owner"], r["member"]) for r in members}
    uncovered = pairs - have
    check(f"docs table covered ({len(pairs) - len(uncovered)} of {len(pairs)} pairs)",
          uncovered == KNOWN, f"unexpected uncovered: {sorted(uncovered - KNOWN)}")
else:
    check("docs cross-check", False, f"{pobj} missing — run tools/parity.py")

# ---------------------------------------------------------------- report
failed = [r for r in results if not r[0]]
for ok, name, detail in results:
    print(f"{'PASS' if ok else 'FAIL'}  {name}" + (f"   [{detail}]" if detail and not ok else ""))
print(f"\n{len(results) - len(failed)}/{len(results)} checks passed")
print(f"owners: {len(by_owner)}   members: {len(members)}   globals: {len(globals_)}")
sys.exit(1 if failed else 0)
