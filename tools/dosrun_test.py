#!/usr/bin/env python3
"""dosrun_test.py: dosrun's event file must reach DOSBox whatever descriptor it opens on."""

import os
import sys
import tempfile
import importlib.util
from pathlib import Path


def load(path: str):
    spec = importlib.util.spec_from_file_location("x86run_under_test", path)
    mod = importlib.util.module_from_spec(spec)
    sys.modules["x86run_under_test"] = mod
    spec.loader.exec_module(mod)
    return mod


def test_events_survive_on_fd9() -> None:
    """On fd 9, a dup2 onto itself left the file close-on-exec: every job of the batch read as lost."""
    mod = load(sys.argv[1] if len(sys.argv) > 1 else str(Path(__file__).with_name("dosrun.py")))
    spare = []
    while True:
        fd = os.open(os.devnull, os.O_RDONLY)
        if fd >= 8:
            break
        spare.append(fd)
    r = mod.run({"a": 'PRINT "OK"\n'}, Path(tempfile.mkdtemp()) / "w")
    assert r["a"].reason == "exit", r["a"].reason
    assert "OK" in r["a"].out, r["a"].out


if __name__ == "__main__":
    test_events_survive_on_fd9()
    print("ok")
