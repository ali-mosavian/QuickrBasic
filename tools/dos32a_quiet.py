#!/usr/bin/env python3
"""dos32a_quiet.py DOS32A.EXE OUT: DOS/32A's loader, its startup banner off, for beside a stub32a executable."""

import sys
from pathlib import Path

data = bytearray(Path(sys.argv[1]).read_bytes())
i = data.find(b"ID32")
if i < 0:
    sys.exit("dos32a_quiet: no ID32 configuration block")
data[i + 21] &= ~0x08  # dos32a_misc2 bit 3: the copyright banner
Path(sys.argv[2]).write_bytes(data)
