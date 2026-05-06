#!/usr/bin/env python3
"""Probe HA automation YAML *shape* without PyYAML (heuristic only; prints to stdout)."""
import json
import re
import sys
from pathlib import Path


def _first_significant_line(text: str) -> str | None:
    for line in text.splitlines():
        t = line.strip()
        if not t or t.startswith("#"):
            continue
        return line
    return None


def _infer_root_shape(text: str) -> dict:
    first = _first_significant_line(text)
    if first is None:
        return {"root": "empty", "first_line": None}
    stripped = first.lstrip()
    if stripped.startswith("- "):
        n = len(re.findall(r"(?m)^[ \t]*-\s+alias:\s", text))
        return {
            "root": "list",
            "first_sig_line_preview": stripped[:72],
            "approx_automation_entries": max(n, 1),
        }
    if re.match(r"^alias:\s", stripped):
        return {"root": "dict", "first_sig_line_preview": stripped[:72]}
    return {"root": "unknown", "first_sig_line_preview": stripped[:72]}


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    argv = sys.argv[1:]
    target = root / (argv[0] if argv else "home_assistant/ha_automation_data_bedroom_3_mode.yaml")

    try:
        rel_path = str(target.resolve().relative_to(root.resolve()))
    except ValueError:
        rel_path = str(target)

    try:
        raw = target.read_text(encoding="utf-8")
    except Exception as e:
        print(f"Read failed: {rel_path}: {e}", file=sys.stderr)
        return 1

    shape = _infer_root_shape(raw)
    print(json.dumps({"path": rel_path, **shape}, indent=2, ensure_ascii=False))

    hint = (
        "Create automation → Edit in YAML expects ONE mapping {alias, triggers, actions...}. "
        "A file whose first significant line begins with `- ` is a YAML list (multiple automations), "
        "which commonly triggers HA errors such as extra keys at data['0']."
    )
    print(json.dumps({"note": hint, "list_root_may_break_single_yaml_editor": shape.get("root") == "list"}, indent=2))

    return 0


if __name__ == "__main__":
    sys.exit(main())
