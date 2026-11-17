#!/usr/bin/env python3
"""x86csoff.py LISTING: the data segment's offset from CS, the code's end to a paragraph, for -DCSOFF."""

import re
import sys

end = int(re.search(r"\ncodeend [ .]*Byte\s+([0-9A-F]+)h _TEXT", open(sys.argv[1]).read()).group(1), 16)
print(f"0{(end + 15) & ~15:X}h")
