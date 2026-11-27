#!/usr/bin/env python3
"""gfx_test_test.py: --card at 8 bits took the screenshot at 20 emulated seconds, before 86Box's Voodoo
had drawn the scene: all black. Drawn, every flat cell failed by 3, the VGA showing the palette at 6 bits
a channel and the card at 565; blends by 99, the card mixing colours where software takes the palette's
nearest; lit and SOLID by 9, software recolouring index 201 when the scene changed row 0 after drawing,
which a card holding colours does not."""

import sys
from pathlib import Path

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
print("gfx_test_test ok")
