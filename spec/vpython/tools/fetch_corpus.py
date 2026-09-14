#!/usr/bin/env python3
"""Fetch GlowScript's official example programs into ../corpus/glowscript-official/.

Uses glowscript.org's public read-only API (routes in glowscript/ide/routes.py):
  GET /api/user/<user>/folder/                         -> {"folders": [...]}
  GET /api/user/<user>/folder/<folder>/program/        -> listing (names, screenshots; no source)
  GET /api/user/<user>/folder/<folder>/program/<name>  -> {"source": ...}
Sequential with a pause between requests — it's someone else's server.
Writes <folder>__<name>.vpy and refreshes ../api-listings/.
"""
import json, os, time, urllib.parse, urllib.request

USER = "GlowScriptDemos"
FOLDERS = ["Examples", "matterandinteractions"]      # "Examples-JavaScript" is JS, not VPython
BASE = "https://www.glowscript.org/api/user"
HERE = os.path.dirname(os.path.abspath(__file__))
# Fetched programs are third-party and must NOT land in this repo: they go to VPYTHON_REF, the same
# folder holding the glowscript clone. (Writing them next to this script would commit someone else's
# code on the next `git add`.)
REF = os.environ.get("VPYTHON_REF") or os.path.join(os.path.expanduser("~"),
                                                    "workspace/math-physics-academy/vpython-reference")
OUT = os.path.join(REF, "corpus", "glowscript-official")
LISTINGS = os.path.join(REF, "api-listings")


def get(url):
    with urllib.request.urlopen(url, timeout=30) as r:
        return json.load(r)


def main():
    os.makedirs(OUT, exist_ok=True); os.makedirs(LISTINGS, exist_ok=True)
    folders = get(f"{BASE}/{USER}/folder/")
    json.dump(folders, open(os.path.join(LISTINGS, "gs_folders.json"), "w"))
    ok = total = 0
    for folder in FOLDERS:
        listing = get(f"{BASE}/{USER}/folder/{folder}/program/")
        json.dump(listing, open(os.path.join(LISTINGS, f"gs_{folder}.json"), "w"))
        for p in listing.get("programs", []):
            total += 1
            url = f"{BASE}/{USER}/folder/{folder}/program/{urllib.parse.quote(p['name'])}"
            try:
                src = get(url).get("source", "")
                open(os.path.join(OUT, f"{folder}__{p['name']}.vpy"), "w").write(src); ok += 1
            except Exception as e:
                print("FAIL", folder, p["name"], e)
            time.sleep(0.5)
    print(f"fetched {ok}/{total} programs into {os.path.normpath(OUT)}")


if __name__ == "__main__":
    main()
