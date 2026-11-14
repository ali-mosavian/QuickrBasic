"""A tiny SVG kit: one style for every architecture diagram."""
from html import escape

FONT = "-apple-system, 'Segoe UI', Helvetica, Arial, sans-serif"
MONO = "'SF Mono', Menlo, Consolas, monospace"
INK, MUTED, LINE = "#1f2937", "#6b7280", "#9ca3af"
THEME = {
    "front": ("#eef2ff", "#4f46e5", "#312e81", "#e0e7ff"),
    "engine": ("#ecfdf5", "#059669", "#064e3b", "#d1fae5"),
    "base": ("#f5f5f4", "#78716c", "#292524", "#e7e5e4"),
    "warn": ("#fff7ed", "#ea580c", "#7c2d12", "#ffedd5"),
}


class Svg:
    def __init__(self, w, h, title=None, subtitle=None):
        self.w, self.h, self.out = w, h, []
        self.out.append(f'<rect width="{w}" height="{h}" fill="#ffffff"/>')
        if title:
            self.text(32, 40, title, size=20, weight=700)
        if subtitle:
            self.text(32, 64, subtitle, size=13, fill=MUTED)

    def text(self, x, y, s, size=13, weight=400, fill=INK, anchor="start", mono=False, italic=False):
        fam = MONO if mono else FONT
        st = ' font-style="italic"' if italic else ""
        self.out.append(
            f'<text x="{x}" y="{y}" font-family="{fam}" font-size="{size}" font-weight="{weight}" '
            f'fill="{fill}" text-anchor="{anchor}"{st}>{escape(s)}</text>')

    def box(self, x, y, w, h, theme="base", r=10, dash=False, strong=False):
        fill, stroke, _, _ = THEME[theme]
        d = ' stroke-dasharray="6 4"' if dash else ""
        sw = 2 if strong else 1.25
        self.out.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{r}" fill="{fill}" '
                        f'stroke="{stroke}" stroke-width="{sw}"{d}/>')

    def panel(self, x, y, w, h, theme, label, note=None):
        self.box(x, y, w, h, theme, strong=True)
        ink = THEME[theme][2]
        self.text(x + 16, y + 26, label, size=15, weight=700, fill=ink)
        if note:
            self.text(x + w - 16, y + 26, note, size=12, fill=THEME[theme][1], anchor="end", italic=True)

    def chip(self, x, y, label, theme="base", w=None, h=28, mono=True):
        w = w or (len(label) * 7.6 + 22)
        _, stroke, ink, tint = THEME[theme]
        self.out.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="6" fill="#ffffff" '
                        f'stroke="{stroke}" stroke-opacity="0.55"/>')
        self.text(x + w / 2, y + h / 2 + 4.5, label, size=12, fill=ink, anchor="middle", mono=mono)
        return w

    def chips(self, x, y, labels, theme, gap=8, maxw=None):
        cx, cy = x, y
        for l in labels:
            w = len(l) * 7.6 + 22
            if maxw and cx + w > x + maxw:
                cx, cy = x, cy + 36
            self.chip(cx, cy, l, theme, w)
            cx += w + gap
        return cy + 28

    def arrow(self, pts, color=LINE, dash=False, width=1.6, head=True, label=None, lx=None, ly=None,
              lanchor="middle"):
        d = "M" + " L".join(f"{x},{y}" for x, y in pts)
        da = ' stroke-dasharray="6 4"' if dash else ""
        mk = f' marker-end="url(#ah{color[1:]})"' if head else ""
        self._marker(color)
        self.out.append(f'<path d="{d}" fill="none" stroke="{color}" stroke-width="{width}"{da}{mk}/>')
        if label:
            x = lx if lx is not None else (pts[0][0] + pts[-1][0]) / 2
            y = ly if ly is not None else (pts[0][1] + pts[-1][1]) / 2
            self.text(x, y, label, size=12, fill=color if color != LINE else MUTED, anchor=lanchor)

    _markers = None

    def _marker(self, color):
        if self._markers is None:
            self._markers = set()
        self._markers.add(color)

    def save(self, path):
        defs = "".join(
            f'<marker id="ah{c[1:]}" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="8" '
            f'markerHeight="8" orient="auto-start-reverse"><path d="M0,0 L10,5 L0,10 z" fill="{c}"/></marker>'
            for c in sorted(self._markers or ()))
        body = "\n".join(self.out)
        svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{self.w}" height="{self.h}" '
               f'viewBox="0 0 {self.w} {self.h}"><defs>{defs}</defs>\n{body}\n</svg>\n')
        open(path, "w").write(svg)
