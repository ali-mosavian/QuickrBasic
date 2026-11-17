#!/usr/bin/env python3
"""mothlite_test.py [EXE]: MOTHLITE.BAS's stones drawn at every place they scroll through.

The demo's main loop is swapped for one that draws each kind of stone from off the left edge to
off the right. A stone with runes partly off the left (x < 0) took a negative rune index:
Subscript out of range mid-game.
"""

import sys
import tempfile
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEMO = ROOT / "src/plat/wasm/web/demos/MOTHLITE.BAS"

DRIVER = """OpenScreen
LoadTables
LoadTerrain
DIM st(0) AS Stone, kind AS INTEGER, x AS INTEGER
FOR kind = 0 TO NKIND - 1
    FOR x = -STONEW - 6 TO 645
        st(0).x = x
        st(0).kind = kind
        st(0).gap = 100
        DrawStone st(), 0, 0
    NEXT
NEXT
PRINT "stones ok"
END
"""


def program() -> str:
    head, rest = DEMO.read_text().split("\nMain\nEND\n", 1)
    return head + "\n" + DRIVER + rest


def main() -> None:
    exe = str(Path(sys.argv[1] if len(sys.argv) > 1 else ROOT / "build/host/basic").resolve())
    check = subprocess.run([exe, "--check", str(DEMO)], capture_output=True, text=True, timeout=60)
    if check.returncode:
        print("mothlite FAIL\n" + (check.stdout + check.stderr)[-600:])
        sys.exit(1)
    with tempfile.TemporaryDirectory() as tmp:
        path = Path(tmp) / "STONES.BAS"
        path.write_text(program())
        run = subprocess.run([exe, path.name], cwd=tmp, capture_output=True, timeout=60)
        out = run.stdout.decode(errors="replace")
    ok = "stones ok" in out
    print("mothlite " + ("ok" if ok else "FAIL\n" + out[-600:]))
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
