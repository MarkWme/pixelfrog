#!/usr/bin/env python3
"""Emit manifest.json for generic repo upload (used from GitHub Actions)."""
from __future__ import annotations

import json
import os
import subprocess
import sys
from datetime import datetime, timezone


def main() -> int:
    out_path = sys.argv[1] if len(sys.argv) > 1 else "manifest.json"
    graph_json = os.environ.get("PIXELFROG_GRAPH_JSON", "graph-info.json")
    deps: list[str] = []
    if os.path.isfile(graph_json):
        with open(graph_json, encoding="utf-8") as f:
            data = json.load(f)
        nodes = (data.get("graph") or {}).get("nodes") or {}
        for _nid, node in nodes.items():
            if not isinstance(node, dict):
                continue
            if node.get("recipe") == "Consumer":
                continue
            ref = node.get("ref")
            if ref:
                deps.append(str(ref))
        deps = sorted(set(deps))
    manifest = {
        "package": "pixelfrog",
        "version": "1.0.0",
        "build_number": os.environ.get("GITHUB_RUN_NUMBER", "local"),
        "git_sha": os.environ.get("GITHUB_SHA", "local"),
        "timestamp": datetime.now(timezone.utc).isoformat(),
        "conan_dependencies": deps,
    }
    try:
        root = os.environ.get("GITHUB_WORKSPACE") or os.path.abspath(
            os.path.join(os.path.dirname(__file__), "..")
        )
        manifest["git_sha"] = subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True, cwd=root
        ).strip()
    except (subprocess.CalledProcessError, FileNotFoundError):
        pass
    with open(out_path, "w", encoding="utf-8") as f:
        json.dump(manifest, f, indent=2)
        f.write("\n")
    print(f"Wrote {out_path} with {len(deps)} dependency entries")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
