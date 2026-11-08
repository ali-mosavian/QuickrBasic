#!/usr/bin/env python3
"""asmstyle.py [PATH...]: line rules for .s, .inc, .py, .md and .txt files.

Every line is at most 100 columns (Python 120, as ruff); a line that is only a comment is at
most 80.
Entry lines of .txt tables (not comments) may be longer.
No tabs in any covered file. Every routine under src/ has a docstring block above its
label, closed by a `; ::::::::::::::` rule line. In assembly a label stands alone: an
instruction never shares its line (data directives may).
Default PATHs: src inc tools test docs tables.
"""

import re
import sys
from pathlib import Path
from collections.abc import Iterable
from collections.abc import Iterator
from collections.abc import Sequence

CODE_MAX: int = 100
PY_MAX: int = 120
COMMENT_MAX: int = 80
SUFFIX: dict[str, str | None] = {".s": ";", ".inc": ";", ".py": "#", ".txt": "#", ".md": None}
DEFAULT_PATHS: list[str] = ["src", "inc", "tools", "test", "docs", "tables", "readme.md"]
DOC_RULE: str = f"; {':' * 14}"
GLOBAL_LABEL: re.Pattern[str] = re.compile(r"^[A-Za-z_]\w*:$")
LABEL_CODE: re.Pattern[str] = re.compile(r"^@?[A-Za-z_]\w*:\s+[A-Za-z]")

type Problem = tuple[int, str]


def files(paths: Iterable[str]) -> Iterator[Path]:
    for p in map(Path, paths):
        if p.is_file():
            yield p
            continue
        for root, _, names in p.walk():
            for n in sorted(names):
                if Path(n).suffix in SUFFIX:
                    yield root / n


def check(path: Path) -> list[Problem]:
    ext = path.suffix
    mark = SUFFIX.get(ext)
    in_src = ext == ".s" and "/src/" in str(path.absolute())
    asm = ext in (".s", ".inc")
    bad = []
    lines = path.read_text(encoding="utf-8").split("\n")
    width = PY_MAX if ext == ".py" else CODE_MAX
    for no, line in enumerate(lines, 1):
        after = lines[no] if no < len(lines) else ""
        before = lines[no - 2] if no > 1 else ""
        if in_src and GLOBAL_LABEL.match(line) and re.match(r"^\s+[A-Za-z]", after) and before != DOC_RULE:
            bad.append((no, "routine without a docstring block above it"))
        if asm and LABEL_CODE.match(line):
            bad.append((no, "instruction on a label line"))
        if "\t" in line:
            bad.append((no, "tab"))
        if len(line) > width and not (ext == ".txt" and not line.startswith("#")):
            bad.append((no, f"{len(line)} columns (max {width})"))
        elif mark and line.lstrip().startswith(mark) and len(line) > COMMENT_MAX:
            bad.append((no, f"comment line {len(line)} columns (max {COMMENT_MAX})"))
    return bad


def main(argv: Sequence[str] | None = None) -> int:
    paths = list(sys.argv[1:] if argv is None else argv) or DEFAULT_PATHS
    failed = 0
    for p in files(x for x in paths if Path(x).exists()):
        for no, msg in check(p):
            print(f"{p}:{no}: {msg}")
            failed = 1
    return failed


if __name__ == "__main__":
    sys.exit(main())
