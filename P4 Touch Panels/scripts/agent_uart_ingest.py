#!/usr/bin/env python3
"""Pipe `idf.py monitor` (or any serial tee) through stdin; append NDJSON lines to .cursor/debug-cf6577.log."""
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.abspath(__file__))
LOG = os.path.normpath(os.path.join(ROOT, "..", ".cursor", "debug-cf6577.log"))
os.makedirs(os.path.dirname(LOG), exist_ok=True)
ANSI = re.compile(r"\x1b\[[0-9;]*m")


def main() -> None:
    for raw in sys.stdin:
        line = ANSI.sub("", raw)
        idx = line.find("NDJSONDBG:")
        if idx < 0:
            continue
        j = line.find("{", idx)
        if j < 0:
            continue
        k = line.rfind("}")
        if k < j:
            continue
        blob = line[j : k + 1]
        try:
            json.loads(blob)
        except json.JSONDecodeError:
            continue
        with open(LOG, "a", encoding="utf-8") as f:
            f.write(blob + "\n")


if __name__ == "__main__":
    main()
