"""basic_src.py: a BASIC program's source read as statements, records and routines.

Each field reference is resolved to the record it is a field of, by the declared types of the
variables and fields before it. The game's test counts with it the fields assigned outside a
record's owner (docs/engine-design.md, Boundaries).
"""

import re
from pathlib import Path
from dataclasses import field
from dataclasses import dataclass

SUFFIX = "%&!#$"
TOKEN = re.compile(
    r"""(?P<str>"[^"]*"?)
    |(?P<rem>'.*)
    |(?P<num>&[HhOo][0-9A-Fa-f]+&?|(?:\d+\.?\d*|\.\d+)(?:[EeDd][+-]?\d+)?[%&!#]?)
    |(?P<name>[A-Za-z][A-Za-z0-9_]*[%&!#$]?)
    |(?P<op><>|<=|>=|[-+*/\\^=<>(),;:.#])
    |(?P<space>\s+)
    |(?P<other>.)""",
    re.VERBOSE,
)


@dataclass(frozen=True)
class Tok:
    kind: str
    text: str
    line: int
    col: int

    @property
    def low(self) -> str:
        return self.text.lower()


@dataclass
class Stmt:
    """One statement: its tokens, the routine it is in ("" at module level)."""

    toks: list[Tok]
    routine: str


@dataclass
class Var:
    type: str
    array: bool


@dataclass
class Routine:
    name: str
    params: dict[str, Var] = field(default_factory=dict)
    locals: dict[str, Var] = field(default_factory=dict)


@dataclass
class Module:
    path: Path
    text: str
    stmts: list[Stmt] = field(default_factory=list)
    routines: dict[str, Routine] = field(default_factory=dict)
    shared: dict[str, Var] = field(default_factory=dict)


def lex(text: str) -> list[list[Tok]]:
    """The tokens of each logical line, a line ended by ` _` joined to the next; comments kept."""
    lines: list[list[Tok]] = []
    cur: list[Tok] = []
    for n, raw in enumerate(text.split("\n"), 1):
        for m in TOKEN.finditer(raw):
            kind = m.lastgroup or "other"
            if kind == "space":
                continue
            cur.append(Tok(kind, m.group(), n, m.start()))
        code = [t for t in cur if t.kind != "rem"]
        if code and code[-1].text == "_" and code[-1].line == n:
            cur = [t for t in cur if not (t.line == n and t.text == "_" and t is code[-1])]
            continue
        lines.append(cur)
        cur = []
    return lines


def statements(line: list[Tok]) -> list[list[Tok]]:
    """A line's statements, split at its colons; its comment dropped."""
    out: list[list[Tok]] = [[]]
    for t in line:
        if t.kind == "rem":
            break
        if t.text == ":":
            out.append([])
            continue
        out[-1].append(t)
    return [s for s in out if s and s[0].low != "rem"]


def var_decls(toks: list[Tok]) -> list[tuple[str, Var]]:
    """The variables a parameter list or a DIM declares: `a as T`, `b() as U`, `c(9) as V`."""
    out: list[tuple[str, Var]] = []
    i = 0
    while i < len(toks):
        if toks[i].low in ("byval", "shared", "preserve") or toks[i].text == ",":
            i += 1
            continue
        if toks[i].kind != "name":
            i += 1
            continue
        name = toks[i].low
        i += 1
        array = False
        if i < len(toks) and toks[i].text == "(":
            array = True
            i = skip_parens(toks, i)
        typ = ""
        if i + 1 < len(toks) and toks[i].low == "as":
            typ = toks[i + 1].low
            i += 2
        else:
            typ = {"%": "integer", "&": "long", "!": "single", "#": "double", "$": "string"}.get(name[-1], "integer")
        out.append((name, Var(typ, array)))
        while i < len(toks) and toks[i].text != ",":
            i += 1
    return out


def skip_parens(toks: list[Tok], i: int) -> int:
    """The index past the parenthesis group that opens at i."""
    depth = 0
    while i < len(toks):
        if toks[i].text == "(":
            depth += 1
        elif toks[i].text == ")":
            depth -= 1
            if depth == 0:
                return i + 1
        i += 1
    return i


def read_module(path: Path, text: str | None = None) -> tuple[Module, dict[str, dict[str, Var]]]:
    """A module's statements and routines, and the records it declares: each field by name."""
    mod = Module(path, path.read_text() if text is None else text)
    records: dict[str, dict[str, Var]] = {}
    routine = ""
    in_type = ""
    for line in lex(mod.text):
        for st in statements(line):
            low = [t.low for t in st]
            if low[:2] == ["end", "type"]:
                in_type = ""
                continue
            if in_type:
                records[in_type].update(var_decls(st))
                continue
            if low[0] == "type" and len(low) == 2:
                in_type = low[1]
                records[in_type] = {}
                continue
            head = low[1:] if low[0] == "private" else low
            if head[0] in ("sub", "function") and len(head) > 1:
                routine = head[1].rstrip(SUFFIX)
                r = Routine(routine)
                at = low.index(head[1]) + 1
                if at < len(st) and st[at].text == "(":
                    r.params.update(var_decls(st[at + 1 : skip_parens(st, at) - 1]))
                mod.routines[routine] = r
                mod.stmts.append(Stmt(st, routine))
                continue
            if head[:2] in (["end", "sub"], ["end", "function"]):
                mod.stmts.append(Stmt(st, routine))
                routine = ""
                continue
            if low[0] in ("dim", "redim", "static") and "as" in low:
                into = mod.routines[routine].locals if routine else mod.shared
                for name, var in var_decls(st[1:]):
                    into.setdefault(name, var)
            mod.stmts.append(Stmt(st, routine))
    return mod, records


@dataclass
class Ref:
    """A chain such as `wd.lv.ob(i).x`: its base variable, and for each field the record it is of."""

    start: int
    end: int
    base: str
    base_type: str
    fields: list[tuple[int, str, str]]  # (token index, field, record it is a field of)

    @property
    def type(self) -> str:
        return self.fields[-1][2] if self.fields else self.base_type


class Program:
    """Every module of a program, and the records they declare."""

    def __init__(self, files: dict[Path, str]) -> None:
        self.records: dict[str, dict[str, Var]] = {}
        self.modules: list[Module] = []
        for path, text in files.items():
            mod, recs = read_module(path, text)
            self.modules.append(mod)
            self.records.update(recs)
        # a variable declared at module level in the main program (CHARLIE.BAS's `wd`)
        self.main_shared: dict[str, Var] = {}
        for mod in self.modules:
            if not mod.routines or mod.path.stem.isupper():
                self.main_shared.update(mod.shared)

    def lookup(self, mod: Module, routine: str, name: str) -> Var | None:
        if routine:
            r = mod.routines[routine]
            if name in r.locals:
                return r.locals[name]
            if name in r.params:
                return r.params[name]
        return mod.shared.get(name) or self.main_shared.get(name)

    def refs(self, mod: Module, st: Stmt) -> list[Ref]:
        """Every chain of a statement whose base is a variable of a record; unresolved fields raise."""
        out: list[Ref] = []
        toks = st.toks
        i = 0
        while i < len(toks):
            t = toks[i]
            if t.kind != "name" or (i and toks[i - 1].text == "."):
                i += 1
                continue
            var = self.lookup(mod, st.routine, t.low)
            if var is None or var.type not in self.records:
                i += 1
                continue
            ref = Ref(i, i + 1, t.low, var.type, [])
            j = i + 1
            if j < len(toks) and toks[j].text == "(":
                j = skip_parens(toks, j)
            typ = var.type
            while j + 1 < len(toks) and toks[j].text == "." and toks[j + 1].kind == "name":
                fields = self.records.get(typ)
                name = toks[j + 1].low
                if fields is None or name not in fields:
                    raise KeyError(f"{mod.path}:{toks[j + 1].line}: no field {name} of {typ}")
                ref.fields.append((j + 1, name, typ))
                typ = fields[name].type
                j += 2
                if j < len(toks) and toks[j].text == "(":
                    j = skip_parens(toks, j)
            ref.end = j
            out.append(ref)
            i = j
        return out


WRITERS = ("for", "redim", "erase", "swap", "read", "input", "get", "line")


def clauses(toks: list[Tok]) -> list[list[Tok]]:
    """A statement's parts: a one-line IF's condition, what it does then, and else."""
    out: list[list[Tok]] = [[]]
    for t in toks:
        if t.low in ("then", "else") and toks[0].low in ("if", "elseif", "else"):
            out.append([])
            continue
        out[-1].append(t)
    return [c for c in out if c]


def assigned(prog: Program, mod: Module, st: Stmt) -> list[Ref]:
    """The chains a statement assigns: its target, a FOR's counter, what REDIM, SWAP, READ or GET writes."""
    return [r for part in clauses(st.toks) for r in assigned_one(prog, mod, Stmt(part, st.routine))]


def assigned_one(prog: Program, mod: Module, st: Stmt) -> list[Ref]:
    toks = st.toks
    low = [t.low for t in toks]
    refs = prog.refs(mod, st)
    if not refs:
        return []
    start = 1 if low[0] == "let" else 0
    if low[start] == "mid$" and len(toks) > start + 2:
        start += 2
    first = [r for r in refs if r.start == start]
    if first and first[0].end < len(toks) and toks[first[0].end].text == "=":
        return first
    if low[0] in WRITERS:
        if low[0] == "get" and "," in low:  # GET (x, y)-(x2, y2), target / GET #n, , target
            cut = len(low) - 1 - low[::-1].index(",")
            return [r for r in refs if r.start > cut]
        if low[0] == "line":
            return [r for r in refs if low[:2] == ["line", "input"] and r.start > low.index(",")]
        if low[0] == "for":
            return [r for r in refs if r.start == 1]
        return [r for r in refs if r.start >= 1 and (r.start == 1 or toks[r.start - 1].text == ",")]
    return []
