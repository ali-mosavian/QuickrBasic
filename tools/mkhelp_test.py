#!/usr/bin/env python3
"""mkhelp_test.py: tools/mkhelp.py refuses what would be a dead link or a lost topic."""

import struct
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import mkhelp

REQUIRED = {
    "contents.md": "# Contents\n\nGo.\n",
    "using-help.md": "# Using Help\nkeys: using help\n\nTab.\n",
    "survival.md": "# Survival\n\nKeys.\n",
    "status.txt": "one\n",
}


def build(files: dict[str, str]) -> tuple[bytes, dict[str, int]]:
    with tempfile.TemporaryDirectory() as tmp:
        for name, text in {**REQUIRED, **files}.items():
            (Path(tmp) / name).write_text(text, encoding="utf-8")
        return mkhelp.build(Path(tmp))


def refused(files: dict[str, str], why: str) -> str | None:
    try:
        build(files)
    except mkhelp.HelpError as e:
        return None if why in str(e) else f"wrong reason: {e}"
    return "was accepted"


CASES = [
    ("a link to no key was a link to nothing: F1 on it beeped",
     {"a.md": "# A\n\nSee [B](nowhere).\n"}, "unknown key"),
    ("two topics with one key: the second was unreachable",
     {"a.md": "# A\nkeys: X\n\n.\n", "b.md": "# B\nkeys: x\n\n.\n"}, "also"),
    ("a line over 76 columns ran off the help window",
     {"a.md": "# A\n\n" + "w" * 80 + "\n"}, "columns"),
    ("a character outside code page 437 drew as garbage",
     {"a.md": "# A\n\nsmile ☺\n"}, "code page 437"),
    ("an authored Index key shadowed the generated Index",
     {"a.md": "# A\nkeys: index\n\n.\n"}, "INDEX"),
    ("a missing contents topic left Contents a dead link",
     {"a.md": "# A\n\n.\n", "contents.md": None}, "CONTENTS"),
]

if __name__ == "__main__":
    bad = []
    for why, files, needle in CASES:
        if files.get("contents.md", "") is None:
            files = {k: v for k, v in files.items() if k != "contents.md"}
            REQUIRED_SAVE = REQUIRED.pop("contents.md")
            r = refused(files, needle)
            REQUIRED["contents.md"] = REQUIRED_SAVE
        else:
            r = refused(files, needle)
        if r:
            bad.append(f"{why}: {r}")

    # a hidden key and a file's name find their topic but are not in the Index
    data, _ = build({"a.md": "# A\nkeys: ~secret, shown\n\n.\n"})
    index = mkhelp.read_topic(data, struct.unpack_from("<H", data, 14)[0])
    if b"shown" not in index or b"secret" in index or b"\x11a\x10" in index:
        bad.append("the Index listed a hidden key or a file name, or lost a shown one")
    # compression round trip over a topic longer than the window
    long_text = "\n".join(f"Line {i}: the quick brown fox {i % 5}" for i in range(2000)).encode()
    d = mkhelp.make_dictionary([long_text])
    if mkhelp.lz_decode(d, mkhelp.lz_encode(d, long_text), len(long_text)) != long_text:
        bad.append("a topic past the window did not come back whole")
    if bad:
        sys.exit("FAIL " + "\nFAIL ".join(bad))
    print("ok")
