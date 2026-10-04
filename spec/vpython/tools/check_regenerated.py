#!/usr/bin/env python3
"""Check that the committed tables are what the extractors produce from this checkout.

Regenerates catalog/ and parity/ from GlowScript (VPYTHON_REF) and this repo's own src/, compares
every column that doesn't come from the program corpus, then puts the committed files back.

The corpus columns are skipped because the student labs are private and CI doesn't have them.
Everything else is a pure function of GlowScript plus vcpp's src/, so a vcpp change that alters
what's wired fails here until the tables are regenerated:

    python3 spec/vpython/tools/catalog_runtime.py && python3 spec/vpython/tools/parity.py

usage: check_regenerated.py        (needs VPYTHON_REF; VCPP_SRC defaults to this repo's src/)
"""
import csv, os, subprocess, sys

SELF = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))  # spec/vpython
TOOLS = os.path.join(SELF, "tools")
REPO_SRC = os.path.normpath(os.path.join(SELF, "..", "..", "src"))

# file -> columns that depend on the corpus. parity/user_attrs.csv is skipped outright: its rows
# are the attributes programs set, so the whole file is corpus-derived.
COMPARED = {
    "catalog/members.csv": {"user_facing", "corpus_uses", "programs"},
    "catalog/globals.csv": {"corpus_calls"},
    "parity/objects.csv": {"kwarg_uses", "assign_uses", "read_uses", "programs"},
    "parity/methods.csv": set(),
    "parity/globals.csv": {"corpus_calls", "programs"},
}


def rows(path, skip):
    with open(path, newline="") as fh:
        r = list(csv.DictReader(fh))
    cols = [c for c in (r[0].keys() if r else []) if c not in skip]
    return {tuple(row[c] for c in cols) for row in r}, cols


def main():
    if not os.environ.get("VPYTHON_REF"):
        sys.exit("VPYTHON_REF is not set: without GlowScript's source there is nothing to regenerate from.")
    env = dict(os.environ, VCPP_SRC=os.environ.get("VCPP_SRC") or REPO_SRC)
    if not os.path.isdir(env["VCPP_SRC"]):
        sys.exit(f"no vcpp source at {env['VCPP_SRC']}")

    # Snapshot everything the generators write, so a local run leaves the tree as it found it.
    snapshot = {}
    for sub in ("catalog", "parity"):
        d = os.path.join(SELF, sub)
        for f in os.listdir(d):
            p = os.path.join(d, f)
            if os.path.isfile(p):
                snapshot[p] = open(p, "rb").read()
    committed = {f: rows(os.path.join(SELF, f), skip) for f, skip in COMPARED.items()}

    bad = 0
    try:
        for tool in ("catalog_runtime.py", "parity.py"):
            subprocess.run([sys.executable, os.path.join(TOOLS, tool)], env=env, check=True,
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        for f, skip in COMPARED.items():
            (old, cols), (new, new_cols) = committed[f], rows(os.path.join(SELF, f), skip)
            cols = cols or new_cols
            diff = sorted(old ^ new)
            print(f"{'ok  ' if not diff else 'FAIL'}  {f}: {len(new)} rows, {len(diff)} differ")
            for t in diff[:10]:
                side = "committed  " if t in old else "regenerated"
                print(f"        {side} {dict(zip(cols, t))}")
            bad += bool(diff)
    finally:
        for p, data in snapshot.items():
            open(p, "wb").write(data)

    if bad:
        print("\nThe committed tables are stale. Regenerate them (see this script's docstring) and commit.")
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
