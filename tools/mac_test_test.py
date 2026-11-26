#!/usr/bin/env python3
"""mac_test_test.py: a virtual Mac, CI's, has no GPU, and its Metal device's scene differed from
software's in 5 runs of 6, failing mac-test; there the GPU's frame is not compared."""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import mac_test

SCENE = (160, 120, bytes(160 * 120 * 3))
OTHER = (160, 120, b"\1" * (160 * 120 * 3))

assert mac_test.scene_shown(SCENE, None), "no GPU, nothing to compare"
assert mac_test.scene_shown(SCENE, SCENE)
assert not mac_test.scene_shown(SCENE, OTHER), "a GPU's other frame"
assert not mac_test.scene_shown((720, 400, b""), None), "the scene not shown"
print("mac_test_test ok")
