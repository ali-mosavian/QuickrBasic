#!/usr/bin/env python3
"""basic_src_test.py: tools/basic_src.py's references."""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import basic_src

# A chain inside another's subscript was passed over: renamed by its reading, `p.x` in
# `lv.glaze(cint(p.x) \ TS)` was left as it was, and the game no longer compiled.
NESTED = """type Hero
    x as single
end type
type Level
    glaze() as integer
end type
sub S (lv as Level, p as Hero)
    lv.glaze(cint(p.x) \\ 16) = 1
end sub
"""

if __name__ == "__main__":
    prog = basic_src.Program({Path("m.bas"): NESTED})
    mod = prog.modules[0]
    st = next(s for s in mod.stmts if s.toks[0].text == "lv")
    found = [(r.base, [f for _, f, _ in r.fields], getattr(r, "inner", False)) for r in prog.refs(mod, st)]
    if found != [("lv", ["glaze"], False), ("p", ["x"], True)]:
        sys.exit(f"FAIL nested: {found}")
    if [r.base for r in basic_src.assigned(prog, mod, st)] != ["lv"]:
        sys.exit("FAIL nested: a chain in the target's subscript taken as assigned")
    print("ok")
