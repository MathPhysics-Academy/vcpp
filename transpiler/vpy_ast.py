#!/usr/bin/env python3
"""Print a VPython program's syntax tree as JSON, for the vcpp transpiler.

usage: vpy_ast.py program.py > program.json

This is the transpiler's only Python: Python's own parser, so a program parses exactly as Python
would parse it. Each node becomes {"_type": <node class>, "lineno", "col_offset", <its fields>}.
GlowScript programs start with a 'Web VPython 3.2' line, which is not Python; it is blanked so line
numbers still match the file.
"""
import ast
import json
import sys


def to_json(node):
    if isinstance(node, ast.AST):
        out = {"_type": type(node).__name__}
        for attr in ("lineno", "col_offset"):
            if hasattr(node, attr):
                out[attr] = getattr(node, attr)
        for field, value in ast.iter_fields(node):
            out[field] = to_json(value)
        return out
    if isinstance(node, list):
        return [to_json(v) for v in node]
    if node is None or isinstance(node, (bool, int, float, str)):
        return node
    raise SystemExit(f"unsupported constant {node!r}")


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__.strip().splitlines()[2])
    path = sys.argv[1]
    lines = open(path, encoding="utf-8").read().split("\n")
    if lines and lines[0].startswith(("Web VPython", "GlowScript")):
        lines[0] = ""
    try:
        tree = ast.parse("\n".join(lines), filename=path)
    except SyntaxError as e:
        sys.exit(f"{path}:{e.lineno}: {e.msg}")
    json.dump(to_json(tree), sys.stdout)


if __name__ == "__main__":
    main()
