"""The README's speed chart: python3 tools/diagrams/speed.py RESULTS.json OUT.svg

RESULTS.json is `tools/bench.py qbasic --json`'s: each benchmark's seconds in QBasic 1.1 and in
QuickrBasic's IDE, both in the same DOSBox-X at a fixed 5000 cycles. A bar is QBasic's time over
ours: 1.0× is QBasic, past it faster, short of it slower. The scale is logarithmic, so 2× faster and
2× slower are bars of one length."""

# ruff: noqa: RUF001, RUF002

import sys
import json
import math
from pathlib import Path

from svg import INK, LINE, MUTED, THEME, Svg

ORDER = [
    ("sieve", "Sieve of Eratosthenes"),
    ("sort", "Bubble sort"),
    ("fib", "Recursion: Fib(18)"),
    ("mandel", "Mandelbrot"),
    ("words", "Word splitting"),
    ("for_int", "Empty FOR loop"),
    ("int_math", "Integer maths"),
    ("sng_math", "SINGLE maths"),
    ("dbl_fn", "SQR, SIN on DOUBLE"),
    ("strings", "String building"),
    ("arrays", "Array updates"),
    ("calls", "SUB and GOSUB calls"),
    ("print", "PRINT"),
    ("pset", "PSET"),
    ("line", "LINE"),
    ("circle", "CIRCLE"),
]
TICKS = [0.5, 1, 2, 5, 10, 20, 50, 100]


def main() -> None:
    data = json.loads(Path(sys.argv[1]).read_text())
    qb, us = data["qbasic"], data["ours"]
    rows = [(k, name, qb[k] / us[k]) for k, name in ORDER if k in qb and us.get(k)]
    total = sum(qb[k] for k, _, _ in rows) / sum(us[k] for k, _, _ in rows)
    lo = min(0.5, min(r for _, _, r in rows) / 1.2)
    hi = max(r for _, _, r in rows) * 1.4
    left, right, top, row_h = 230, 920, 120, 30
    span = math.log(hi) - math.log(lo)

    def x(v: float) -> float:
        return left + (math.log(v) - math.log(lo)) / span * (right - left)

    bottom = top + len(rows) * row_h
    s = Svg(
        1000,
        bottom + 96,
        f"{total:.1f}× faster than QBasic 1.1",
        "QBasic's time over ours, same DOSBox-X at 5000 cycles. 1.0× is QBasic: right is faster, left slower.",
    )
    fast, slow = THEME["engine"], THEME["warn"]
    for t in TICKS:
        if lo <= t <= hi:
            s.out.append(
                f'<line x1="{x(t):.1f}" y1="{top - 8}" x2="{x(t):.1f}" y2="{bottom}" '
                f'stroke="{LINE if t != 1 else INK}" stroke-width="{1.6 if t == 1 else 0.6}"'
                f"{'' if t == 1 else ' stroke-dasharray="3 4"'}/>"
            )
            s.text(
                x(t),
                top - 14,
                "1.0×" if t == 1 else f"{t:g}×",
                size=11,
                anchor="middle",
                fill=INK if t == 1 else MUTED,
                weight=700 if t == 1 else 400,
            )
    s.text(x(1), bottom + 18, "QBasic 1.1", size=12, anchor="middle", weight=600)
    for i, (_, name, r) in enumerate(rows):
        y = top + i * row_h
        th = fast if r >= 1 else slow
        x0, x1 = sorted((x(1), x(r)))
        s.text(left - 14, y + 17, name, size=13, anchor="end", fill=INK)
        s.out.append(
            f'<rect x="{x0:.1f}" y="{y + 5}" width="{max(x1 - x0, 1.5):.1f}" height="16" rx="3" fill="{th[1]}"/>'
        )
        tx, anchor = (x1 + 6, "start") if r >= 1 else (x0 - 6, "end")
        s.text(tx, y + 18, f"{r:.1f}×", size=12, weight=700, anchor=anchor, fill=th[2])
    y = bottom + 34
    s.out.append(f'<line x1="32" y1="{y}" x2="968" y2="{y}" stroke="{LINE}" stroke-width="1"/>')
    s.text(left - 14, y + 30, "All of them", size=13, weight=700, anchor="end")
    s.out.append(
        f'<rect x="{x(1):.1f}" y="{y + 16}" width="{x(total) - x(1):.1f}" height="18" rx="3" fill="{fast[1]}"/>'
    )
    s.text(x(total) + 6, y + 30, f"{total:.1f}×", size=14, weight=700, fill=fast[2])
    s.save(sys.argv[2])


main()
