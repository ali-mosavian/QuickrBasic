#!/usr/bin/env python3
"""deps_test.py: tools/deps.sh in a scratch root. A C23 compiler (ubuntu's gcc) failed jwasm on its own
bool; dosbox-x was a link into the source cache, dangling once the checkout moved."""

import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SCRIPT = (ROOT / "tools/deps.sh").read_text()


def pin(name: str) -> str:
    m = re.search(rf"^{name}=(\w+)$", SCRIPT, re.M)
    assert m, name
    return m[1]


def deps(root: Path, *args: str, env: dict[str, str] | None = None) -> None:
    subprocess.run(
        ["sh", str(root / "tools/deps.sh"), *args],
        env=os.environ | {"DEPS_CACHE": str(root / "cache")} | (env or {}),
        check=True,
    )


def scratch(d: Path) -> Path:
    (d / "tools").mkdir()
    shutil.copy(ROOT / "tools/deps.sh", d / "tools/deps.sh")
    (d / "cache").mkdir()
    return d


def dosbox_outlives_cache() -> None:
    with tempfile.TemporaryDirectory() as t:
        root = scratch(Path(t))
        src = root / f"cache/dosbox-x-{pin('DOSBOX')}"
        (src / ".git").mkdir(parents=True)
        (src / ".built").touch()
        (src / "src").mkdir()
        (src / "src/dosbox-x").write_text("#!/bin/sh\necho dosbox\n")
        (src / "src/dosbox-x").chmod(0o755)
        deps(root, "dosbox")
        shutil.rmtree(root / "cache")
        out = subprocess.run([str(root / "build/deps/bin/dosbox-x")], capture_output=True, text=True)
        assert out.stdout == "dosbox\n", f"dosbox-x lost with the cache: {out}"


def jw_under_c23() -> None:
    with tempfile.TemporaryDirectory() as t:
        root = scratch(Path(t))
        for repo, name in (("JWasm", "JWASM"), ("JWlink", "JWLINK")):
            have = ROOT / f"build/deps/src/{repo}-{pin(name)}"
            if have.is_dir():  # a local clone: no network
                dst = root / f"cache/{repo}-{pin(name)}"
                subprocess.run(["git", "clone", "-q", str(have), str(dst)], check=True)
                subprocess.run(["git", "-C", str(dst), "checkout", "-q", pin(name)], check=True)
        deps(root, "build", env={"CC": "cc -std=c23"})
        for tool in ("jwasm", "jwlink"):
            assert (root / "build/deps/bin" / tool).is_file(), tool


if __name__ == "__main__":
    dosbox_outlives_cache()
    jw_under_c23()
    print("ok")
    sys.exit(0)
