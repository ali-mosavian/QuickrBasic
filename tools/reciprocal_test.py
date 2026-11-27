"""reciprocal_test.py: the DOS draw path's 1 / w and 1 / z skip the divide where w or z is 1, as a 2D draw's are.
Written as x == 1 ? 1 : 1 / x, clang folded the test away, 1 / 1 being 1: each corner divided in gfx, the player and
the driver, a 486's 73 cycles each, ~45k of MEADOW16's frame."""

import os
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OBJDUMP = os.environ.get("OBJDUMP", "objdump")
# each function and the object it is in: their reciprocals inlined
DIVIDED = {
    "_poly": "build/c/gfx/stdlib/gfx/src/backend/gpu/backend.obj",
    "_op_draw": "build/c/obj/plat/dos32/accel/player.obj",
    "_tdfx_corners": "build/c/drivers/3dfx/setup.obj",
}
ONE = re.compile(r"\bcmpl\s+\$0x3f800000,")  # 1.0f's bits compared


def main() -> int:
    subprocess.run(["make", *sorted(set(DIVIDED.values()))], cwd=ROOT, check=True, capture_output=True)
    version = subprocess.run([OBJDUMP, "--version"], capture_output=True, text=True).stdout
    coff = ["-b", "pe-i386"] if "GNU" in version else []  # GNU's cannot tell COFF's flavours apart
    for fn, obj in DIVIDED.items():
        listing = subprocess.run([OBJDUMP, *coff, "-d", "--no-show-raw-insn", f"--disassemble-symbols={fn}", obj],
                                 cwd=ROOT, check=True, capture_output=True, text=True).stdout
        assert f"<{fn}>:" in listing, f"{fn} not in {obj}"
        assert ONE.search(listing), f"{fn} divides where w or z is 1: no compare with 1.0f's bits"
    print("ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
