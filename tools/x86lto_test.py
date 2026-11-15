#!/usr/bin/env python3
"""x86lto_test.py EXE: the LTO build's messages and keywords, which its layout of data moves.

A message referred to entries past the messages' list, the keywords', and found them only while the
linker laid that list next: one module of everything put other data between, and READY. printed as
INPUT#Y., keywords three early.
"""

import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import x86run

JOB = "PRINT 5\r\nLIST\r\n"  # the error names its keyword
WANT = ("READY.\r\n\r\n 5 \r\nREADY.\r\n\r\nSyntax error\r\n  LIST\r\n  ^\r\n`LIST` cannot be used here: it does"
        " not start a valid statement at this point.\r\nCheck the statement against the manual; the mark shows"
        " where it stopped being understood.\r\nREADY.\r\n")


def main() -> None:
    x86run.COM = Path(sys.argv[1])
    got = x86run.run({"a": JOB}, Path(tempfile.mkdtemp()) / "w", 20000)["a"].out
    if got != WANT:
        print(f"FAIL lto: {got!r}")
        sys.exit(1)
    print(f"ok {x86run.COM}")


if __name__ == "__main__":
    main()
