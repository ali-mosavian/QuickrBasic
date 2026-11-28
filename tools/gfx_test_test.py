#!/usr/bin/env python3
"""gfx_test_test.py: --card at 8 bits took the screenshot at 20 emulated seconds, before 86Box's Voodoo
had drawn the scene: all black. Drawn, every flat cell failed by 3, the VGA showing the palette at 6 bits
a channel and the card at 565; blends by 99, the card mixing colours where software takes the palette's
nearest; lit and SOLID by 9, software recolouring index 201 when the scene changed row 0 after drawing,
which a card holding colours does not. --card killed 86Box as it quit, before it wrote the VM's CMOS: the file
left empty, the next boot found no disk."""

import sys
import tempfile
from pathlib import Path
from types import SimpleNamespace

sys.path.insert(0, str(Path(__file__).parent))
import gfx_test

scene = gfx_test.card_scene(8)
drawn = max(scene.rindex("gfx.draw."), scene.rindex("gfx.display.set_palette"))
said = scene.index(f"out &HE9, {ord(gfx_test.CARD_DRAWN)}")
assert drawn < scene.rindex("gfx.display.flip", 0, said) < said, "said before drawn and shown"
assert scene.rindex("same&(201) = was&") > scene.rindex("gfx.draw."), "row 0 left as it was not drawn"

assert gfx_test.most_off("solid", 8, True) >= 3, "565 against the VGA's 6 bits"
assert gfx_test.most_off("solid", 16, True) == 0
assert gfx_test.most_off("add", 8, True) == 255, "the card's own blends"
assert gfx_test.most_off("solid", 8, False) == 0, "the Mac's GPU"
# --card read bench's map of cards to VMs, gone once a card's VM became a machine's: every card run raised
assert gfx_test.card_vm("voodoo1").name == "qb-p100-voodoo1"
assert gfx_test.card_vm("voodoo1", "p6").name == "qb-ppro200-voodoo1", "--machine's VM"


class Emulator:
    """86Box headless: quit, it writes the CMOS and exits; killed before that, the CMOS is left empty."""

    def __init__(self) -> None:
        self.quitting = self.exited = self.emptied = False
        self.said = chr(4)

    def wait(self, timeout: float | None = None) -> int:
        assert self.quitting, "waited on an emulator not told to quit"
        self.exited = True
        return 0

    def poll(self) -> int | None:
        return 0 if self.exited else None

    def kill(self) -> None:
        self.emptied = self.emptied or not self.exited


class Client:
    def __init__(self, path: Path, timeout: float) -> None:
        pass

    def __enter__(self) -> "Client":
        return self

    def __exit__(self, *_: object) -> None:
        pass

    def status(self) -> dict:
        return {"emu_ms": 1}

    def screen_text(self) -> str:
        return ""

    def debug_out(self) -> str:
        said, emulator.said = emulator.said, ""
        return said

    def screenshot(self, path: Path) -> None:
        path.write_bytes(b"P6 1 1 255\n\0\0\0")

    def quit(self) -> None:
        emulator.quitting = True


emulator = Emulator()
with tempfile.TemporaryDirectory() as d:
    built = Path(d)
    (built / "lib").mkdir()
    for name in ("basic32.exe", "DOS32A.EXE", "lib/gfx.qbx"):
        (built / name).write_bytes(b"")
    gfx_test.dosrun.COM = built / "basic32.exe"
    gfx_test.card_vm = lambda card, machine=None: built
    bench = sys.modules.setdefault("bench", __import__("bench"))
    bench.mtools = lambda *a, **k: b""
    bench.hl86_module = lambda: SimpleNamespace(
        dos_get=lambda image, name: b"", dos_put=lambda image, path, name: None,
        launch=lambda *a, **k: emulator, Client=Client)
    assert gfx_test.card_shot("voodoo1", 16)[0] == 1
assert not emulator.emptied, "86Box killed before it wrote the CMOS"
print("gfx_test_test ok")
