#!/usr/bin/env python3
"""cfmt_test.py: tools/cfmt.py's blank lines."""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import cfmt

# A macro defined in a function body: a blank line after its first line ended it there, its body
# left as the function's statements (exec_run's COLD_RUN ran nothing).
DEFINE = """void f(void)
{
    goto *p;
#define M()                                                                                        \\
    a = 1;                                                                                         \\
    goto n;
    M()
}"""

if __name__ == "__main__":
    got = cfmt.blank_lines(DEFINE)
    macro = DEFINE[DEFINE.index("#define"):DEFINE.index("goto n;") + 7]
    if macro not in got:
        sys.exit(f"FAIL define: a line split from its #define:\n{got}")
    print("ok")
