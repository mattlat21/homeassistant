#!/usr/bin/env python3
"""Build the ESP-IDF project and copy firmware artifacts into binaries/vX.Y.Z/.

Version is read from the root CMakeLists.txt (FW_VER_MAJOR/MINOR/PATCH).
Requires ESP-IDF on PATH: source $IDF_PATH/export.sh before running, or set IDF_PATH.

Usage:
  ./scripts/build_firmware_release.py
  ./scripts/build_firmware_release.py --skip-build   # copy from existing build/ only
"""

from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path


def _repo_root() -> Path:
    return Path(__file__).resolve().parents[1]


def _parse_cmake_lists(text: str) -> tuple[int, int, int, str]:
    """Return (major, minor, patch, project_name) from root CMakeLists.txt."""
    m_major = re.search(r"^\s*set\s*\(\s*FW_VER_MAJOR\s+(\d+)\s*\)", text, re.MULTILINE)
    m_minor = re.search(r"^\s*set\s*\(\s*FW_VER_MINOR\s+(\d+)\s*\)", text, re.MULTILINE)
    m_patch = re.search(r"^\s*set\s*\(\s*FW_VER_PATCH\s+(\d+)\s*\)", text, re.MULTILINE)
    m_proj = re.search(r"^\s*project\s*\(\s*(\w+)\s*\)", text, re.MULTILINE)
    if not (m_major and m_minor and m_patch):
        raise ValueError("Could not parse FW_VER_MAJOR/MINOR/PATCH from CMakeLists.txt")
    name = m_proj.group(1) if m_proj else "screen_test_1"
    return int(m_major.group(1)), int(m_minor.group(1)), int(m_patch.group(1)), name


def _run_build(repo: Path) -> None:
    env = os.environ.copy()
    if not env.get("IDF_PATH"):
        print("error: IDF_PATH is not set. Run: source $IDF_PATH/export.sh", file=sys.stderr)
        sys.exit(1)
    cmd = ["idf.py", "build"]
    print(f"+ {' '.join(cmd)} (cwd={repo})")
    subprocess.run(cmd, cwd=repo, env=env, check=True)


def main() -> int:
    parser = argparse.ArgumentParser(description="Build firmware and stage binaries under binaries/vX.Y.Z/")
    parser.add_argument(
        "--skip-build",
        action="store_true",
        help="Do not run idf.py build; only copy from existing build/ directory",
    )
    args = parser.parse_args()

    repo = _repo_root()
    cmake_path = repo / "CMakeLists.txt"
    if not cmake_path.is_file():
        print(f"error: missing {cmake_path}", file=sys.stderr)
        return 1

    text = cmake_path.read_text(encoding="utf-8")
    try:
        major, minor, patch, project_name = _parse_cmake_lists(text)
    except ValueError as e:
        print(f"error: {e}", file=sys.stderr)
        return 1

    ver_str = f"{major}.{minor}.{patch}"
    out_dir = repo / "binaries" / f"v{ver_str}"
    build_dir = repo / "build"

    if not args.skip_build:
        _run_build(repo)
    else:
        if not build_dir.is_dir():
            print(f"error: {build_dir} missing; run without --skip-build first", file=sys.stderr)
            return 1

    app_bin = build_dir / f"{project_name}.bin"
    if not app_bin.is_file():
        print(f"error: missing app binary {app_bin}", file=sys.stderr)
        return 1

    out_dir.mkdir(parents=True, exist_ok=True)

    staged: list[str] = []

    def copy_if_exists(src: Path, dst_name: str | None = None) -> None:
        if not src.is_file():
            return
        name = dst_name or src.name
        dst = out_dir / name
        shutil.copy2(src, dst)
        staged.append(name)

    copy_if_exists(app_bin)

    # Full-flash reference files (optional; paths may vary slightly by IDF version)
    copy_if_exists(build_dir / "bootloader" / "bootloader.bin")
    copy_if_exists(build_dir / "partition_table" / "partition-table.bin")

    flash_args = build_dir / "flash_args"
    if flash_args.is_file():
        copy_if_exists(flash_args)

    flash_project = build_dir / "flash_project_args"
    if flash_project.is_file():
        copy_if_exists(flash_project)

    manifest = {
        "version": ver_str,
        "project": project_name,
        "generated_at_utc": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "artifacts": staged,
        "note": "OTA typically uses only the app .bin over HTTPS; bootloader/partition-table are for full flash reference.",
    }
    (out_dir / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")

    print(f"Staged firmware v{ver_str} -> {out_dir}")
    for p in sorted(out_dir.iterdir()):
        print(f"  {p.name}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
