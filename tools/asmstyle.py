#!/usr/bin/env python3
"""asmstyle.py [--format] [PATH...]: line rules for .s, .inc, .py, .md and .txt files.

Every line is at most 100 columns (Python 120, as ruff); a line that is only a comment is at
most 80.
Entry lines of .txt tables (not comments) may be longer.
No tabs in any covered file. Every routine under src/ has a docstring block above its
label, closed by a `; ::::::::::::::` rule line, and a known prefix of at most 3 letters
(docs/asm-style.md). In assembly a label stands alone: an instruction never shares its line
(data directives may).
--format: a src/ file also opens with the header (holds:, uses:, data:, calls:, in that
order; data: optional), and docstring fields come in the order in:, out:, keeps:, error:.
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
HEAD_RULE: str = f"; {'=' * 74}"
GLOBAL_LABEL: re.Pattern[str] = re.compile(r"^[A-Za-z_]\w*:$")
LABEL_CODE: re.Pattern[str] = re.compile(r"^@?[A-Za-z_]\w*:\s+[A-Za-z]")
PREFIX: re.Pattern[str] = re.compile(
    r"(x|fn|op|st|ev|lx|prg|var|sym|str|arr|flt|int|io|err|rpl|fil|sys|mem|plt|sim|rt|for|rd|ls|ret)_"
)
MNEMONIC: re.Pattern[str] = re.compile(
    r"\s+(adc|and|asl|b(cc|cs|eq|it|mi|ne|pl|rk|vc|vs)|cl[cdiv]|cmp|cp[xy]|de[cxy]|eor|in[cxy]|jmp|jsr"
    r"|ld[axy]|lsr|nop|ora|ph[ap]|pl[ap]|ro[lr]|rt[is]|sbc|se[cdi]|st[axy]|ta[xy]|tsx|tx[as]|tya)\b",
    re.IGNORECASE,
)
HEAD_KEYS: list[str] = ["holds", "uses", "data", "calls"]
HEAD_NEEDED: frozenset[str] = frozenset({"holds", "uses", "calls"})
DOC_KEYS: list[str] = ["in", "out", "keeps", "error"]

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


def prefix_problem(label: str) -> str | None:
    head, sep, _ = label.partition("_")
    if not sep:
        return f"code label {label} has no prefix"
    if len(head) > 3:
        return f"code label {label}: prefix {head}_ over 3 letters"
    if not PREFIX.fullmatch(f"{head}_"):
        return f"code label {label}: unknown prefix {head}_"
    return None


def in_order(keys: list[tuple[int, str]], order: list[str], what: str) -> list[Problem]:
    """A problem for each key that comes before one it should follow."""
    bad, top = [], -1
    for no, k in keys:
        i = order.index(k)
        if i <= top:
            bad.append((no, f"{what} {k}: out of order ({', '.join(order)})"))
        top = max(top, i)
    return bad


def check_header(path: Path, lines: list[str]) -> list[Problem]:
    if len(lines) < 3 or lines[0] != HEAD_RULE or lines[2] != HEAD_RULE:
        return [(1, "no file header")]
    if not lines[1].startswith(f"; {path.name} - "):
        return [(2, f"header title is not '; {path.name} - summary'")]
    keys = []
    for no, line in enumerate(lines[3:], 4):
        if line == HEAD_RULE:
            break
        m = re.match(r"^; (\w+):", line)
        if m and m[1] in HEAD_KEYS:
            keys.append((no, m[1]))
    else:
        return [(1, "file header not closed")]
    bad = [(1, f"header without {k}:") for k in HEAD_KEYS if k in HEAD_NEEDED and k not in {h for _, h in keys}]
    return bad + in_order(keys, HEAD_KEYS, "header section")


def check_docstrings(lines: list[str]) -> list[Problem]:
    bad: list[Problem] = []
    keys: list[tuple[int, str]] | None = None
    for no, line in enumerate(lines, 1):
        if line == DOC_RULE:
            if keys is not None:
                bad += in_order(keys, DOC_KEYS, "docstring field")
            keys = [] if keys is None else None
        elif keys is not None and (m := re.match(r"^; (\w+):", line)) and m[1] in DOC_KEYS:
            keys.append((no, m[1]))
    return bad


def check(path: Path, fmt: bool = False) -> list[Problem]:
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
        if in_src and GLOBAL_LABEL.match(line) and re.match(r"^\s+[A-Za-z]", after):
            if before != DOC_RULE:
                bad.append((no, "routine without a docstring block above it"))
            if MNEMONIC.match(after) and (msg := prefix_problem(line[:-1])):
                bad.append((no, msg))
        if asm and LABEL_CODE.match(line):
            bad.append((no, "instruction on a label line"))
        if "\t" in line:
            bad.append((no, "tab"))
        if len(line) > width and not (ext == ".txt" and not line.startswith("#")):
            bad.append((no, f"{len(line)} columns (max {width})"))
        elif mark and line.lstrip().startswith(mark) and len(line) > COMMENT_MAX:
            bad.append((no, f"comment line {len(line)} columns (max {COMMENT_MAX})"))
    if fmt and in_src:
        bad += check_header(path, lines) + check_docstrings(lines)
    return sorted(bad)


def main(argv: Sequence[str] | None = None) -> int:
    args = list(sys.argv[1:] if argv is None else argv)
    fmt = "--format" in args
    paths = [a for a in args if a != "--format"] or DEFAULT_PATHS
    failed = 0
    for p in files(x for x in paths if Path(x).exists()):
        for no, msg in check(p, fmt):
            print(f"{p}:{no}: {msg}")
            failed = 1
    return failed


if __name__ == "__main__":
    sys.exit(main())
