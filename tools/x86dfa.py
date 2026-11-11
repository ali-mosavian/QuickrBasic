#!/usr/bin/env python3
"""x86dfa.py [check]: the flat build's lexer and statement parser as minimized automata.

  x86dfa.py        write src/x86f/comp/dfa.inc
  x86dfa.py check  the tables against the reference lexer on the lex corpus and random lines

The lexer is two automata. The scanner reads text bytes and gives a symbol: a keyword's token
(the lowest-numbered keyword that is a prefix of the text, as stock), PRINT for '?', else the
byte. The tokenizer reads symbols and writes the line's body: names, numbers, strings, REM and
DATA text. Each tokenizer state says how its symbol is read: by the scanner, raw, or as DATA
text. The statement parser is an automaton over the body's tokens from one LL(1) grammar per
statement, with the compiler's actions on its edges.

Each automaton is built, pruned to what its start reaches and minimized (Moore partition
refinement), then written as dense tables over byte classes. ref_lex is the hand lexer the
tables replace, byte for byte, but for a bug: it took a name for FN's when the byte before it
(spaces skipped) was A5h, which an M_INT's high byte or a name's id can be.
"""

import random
import sys
from dataclasses import dataclass, field
from pathlib import Path

import lexer

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "src/x86f/comp/dfa.inc"
KW: list[tuple[int, bytes]] = [(0x80 + n, k) for n, k in enumerate(lexer.KEYWORDS)]
TK_PRINT, TK_REM, TK_DATA, TK_FN, TK_PLUS, TK_MINUS, TK_PI = 0x99, 0x8F, 0x83, 0xA5, 0xAA, 0xAB, 0xFF
M_VAR, M_INT = 0xCC, 0xCD
SP, QUOTE, COLON = 0x20, ord('"'), ord(":")


def fold(c: int) -> int:
    return c - 32 if 0x61 <= c <= 0x7A else c


class Syms:
    """sym_find: an id per (key, kind, array), numbered as they come."""

    def __init__(self) -> None:
        self.ids: dict[tuple, int] = {}

    def find(self, key: bytes, kind: int, arr: int) -> int:
        return self.ids.setdefault((bytes(key), kind, arr), len(self.ids) + 1)


# ---- the reference: lex.asm's lx_line before the automata


def kw_scan(t: bytearray, i: int) -> tuple[int, int]:
    """The keyword search at i: (token or byte, the index past it). Folds the letters it compares."""
    for tok, k in KW:
        j = 0
        while j < len(k):
            t[i + j] = fold(t[i + j])
            if t[i + j] != k[j]:
                break
            j += 1
        if j == len(k):
            return tok, i + j
    return t[i], i + 1


def lx_get(t: bytearray, i: int, data: bool) -> tuple[int, int, int]:
    """(symbol, where it starts, the index past it); at the 0 the index stays."""
    while True:
        c = t[i]
        if c == 0:
            return 0, i, i
        if c >= 0x80 and c != TK_PI:
            i += 1
            continue
        if c >= 0x80 or data or c == SP or 0x30 <= c < 0x3C:
            return c, i, i + 1
        if c == ord("?"):
            return TK_PRINT, i, i + 1
        tok, j = kw_scan(t, i)
        return tok, i, j


def ref_lex(text: bytes, syms: Syms) -> bytes:
    """The body lx_line made of text, its 0 not included."""
    t = bytearray(text + b"\0")
    out = bytearray()
    i, data = 0, False
    while True:
        c, s, i = lx_get(t, i, data)
        if c == QUOTE or (c == TK_REM and not data):
            out.append(c)
            while t[i]:
                b = t[i]
                i += 1
                out.append(b)
                if c == QUOTE and b == QUOTE:
                    break
            continue
        if data:
            if c == 0:
                return bytes(out)
            out.append(c)
            data = c != COLON
            continue
        if ord(".") <= c <= ord("9"):
            i = ref_num(t, i, c, out)
            continue
        if c == TK_DATA:
            data = True
        if ord("A") <= c <= ord("Z"):
            i = ref_name(t, i, c, out, syms)
            continue
        if c == 0:
            return bytes(out)
        out.append(c)


def ref_num(t: bytearray, i: int, c: int, out: bytearray) -> int:
    """lx_num: the text, or M_INT lo hi for a whole number under 65536; at the line's start digits only."""
    at = len(out)
    line = at == 0
    v, f = 0, False
    while True:
        out.append(c)  # lx_nc
        if 0x30 <= c <= 0x39:
            if not f:
                v = v * 10 + c - 0x30
                f = v > 65535
            c, s, i = lx_get(t, i, False)
        else:
            f = True
            c, s, i = lx_get(t, i, False)
            if out[-1] == ord("E") and c in (TK_PLUS, TK_MINUS):
                continue
        if (0x30 <= c <= 0x39) if line else (c == ord("E") or ord(".") <= c <= ord("9")):
            continue
        break
    if not f:
        del out[at:]
        out += bytes([M_INT, v & 255, v >> 8])
    return s


def ref_name(t: bytearray, i: int, c: int, out: bytearray, syms: Syms) -> int:
    """A name: M_VAR len name, then its id; FN's when FN is the byte before it, spaces skipped."""
    key = bytearray([c, 0])
    j = len(out) - 1
    while j >= 0 and out[j] == SP:
        j -= 1
    kind = 3 if j >= 0 and out[j] == TK_FN else 0
    out += bytes([M_VAR, 0])
    lenp = len(out)

    def k(b: int) -> None:
        if key[1] == 0:
            key[1] = b
        out.append(b)

    out.append(c)
    while True:
        c, s, i = lx_get(t, i, False)
        if not (0x30 <= c <= 0x39 or 0x41 <= c <= 0x5A):
            break
        k(c)
    if c in (ord("%"), ord("$")):
        kind = 1 if c == ord("%") else 2
        k(c)
    else:
        i = s
    out[lenp - 1] = len(out) - lenp
    arr = 0
    if kind != 3:
        j = i
        while True:
            c, _, j = lx_get(t, j, False)
            if c != SP:
                break
        arr = 0x80 if c == ord("(") else 0
    out.append(syms.find(key, kind, arr))
    return i


# ---- automata


@dataclass
class Dfa:
    """States 0..n-1, start 0. delta[s][cls] = (action, next), next -1 stops. label[s]: what the
    state itself means (a keyword's token, a read mode), kept apart by the minimization."""

    ncls: int
    delta: list[list[tuple[str, int]]] = field(default_factory=list)
    label: list[object] = field(default_factory=list)

    def new(self, label: object, fill: tuple[str, int]) -> int:
        self.delta.append([fill] * self.ncls)
        self.label.append(label)
        return len(self.delta) - 1


def prune(d: Dfa, named: dict[str, int]) -> tuple[Dfa, dict[str, int]]:
    """Only the states the start and the named states reach."""
    seen, todo = {0, *named.values()}, [0, *named.values()]
    while todo:
        for _, t in d.delta[todo.pop()]:
            if t >= 0 and t not in seen:
                seen.add(t)
                todo.append(t)
    num = {s: n for n, s in enumerate(sorted(seen))}
    p = Dfa(d.ncls)
    for s in sorted(seen):
        p.new(d.label[s], ("", -1))
        p.delta[-1] = [(a, num[t] if t >= 0 else -1) for a, t in d.delta[s]]
    return p, {k: num[v] for k, v in named.items()}


def minimize(d: Dfa, named: dict[str, int]) -> tuple[Dfa, dict[str, int]]:
    """Moore: blocks by label and each class's action, split by the blocks the edges reach until
    nothing splits. The start stays state 0; named states (entry points) are mapped, and may merge."""
    d, named = prune(d, named)
    sig0 = [(repr(d.label[s]), tuple(a for a, _ in d.delta[s])) for s in range(len(d.delta))]
    cls = [sorted(set(sig0)).index(x) for x in sig0]
    while True:
        sig = [(cls[s], tuple(cls[t] if t >= 0 else -1 for _, t in d.delta[s])) for s in range(len(d.delta))]
        ids = {x: n for n, x in enumerate(sorted(set(sig)))}
        new = [ids[x] for x in sig]
        if len(set(new)) == len(set(cls)):
            break
        cls = new
    order: dict[int, int] = {}
    for s in range(len(d.delta)):
        order.setdefault(cls[s], len(order))
    m = Dfa(d.ncls)
    for c in order:
        s = cls.index(c)
        m.new(d.label[s], ("", -1))
        m.delta[-1] = [(a, order[cls[t]] if t >= 0 else -1) for a, t in d.delta[s]]
    return m, {k: order[cls[v]] for k, v in named.items()}


def classes(sets: list[set[int]]) -> list[int]:
    """The coarsest byte classes that keep each set whole."""
    sig: dict[tuple, int] = {}
    return [sig.setdefault(tuple(b in s for s in sets), len(sig)) for b in range(256)]


# ---- the scanner: text bytes to a symbol

# its actions: step into a state; a keyword's last byte (its token, the byte
# after not read); the end (0, not stepped past); skip a byte of 80h-FEh; the
# byte; PRINT for '?'; back to the best keyword so far, or the first byte when
# there is none
SC_ACTS = ("step", "hit", "nul", "skip", "byte", "print", "back")


def scanner() -> tuple[Dfa, list[int]]:
    direct = {SP, TK_PI, *range(0x30, 0x3C)}
    kwchars = sorted({c for _, k in KW for c in k})
    letters = ({c, c + 32} if 0x41 <= c <= 0x5A else {c} for c in kwchars)
    cls = classes([{0}, set(range(0x80, 0xFF)), direct, {ord("?")}, *letters])
    d = Dfa(max(cls) + 1)
    start = d.new("start", ("back", -1))
    for b in range(256):
        c = cls[b]
        if b == 0:
            d.delta[start][c] = ("nul", -1)
        elif 0x80 <= b < 0xFF:
            d.delta[start][c] = ("skip", start)
        elif b in direct:
            d.delta[start][c] = ("byte", -1)
        elif b == ord("?"):
            d.delta[start][c] = ("print", -1)

    def better(prefix: bytes, best: int) -> list[int]:
        """The next bytes of keywords longer than prefix that would beat best: only they are read on."""
        n = len(prefix)
        return sorted({k[n] for tok, k in KW if k.startswith(prefix) and len(k) > n and (not best or tok < best)})

    def children(s: int, prefix: bytes, best: int) -> None:
        for c in better(prefix, best):
            p = prefix + bytes([c])
            term = next((tok for tok, k in KW if k == p), 0)
            rec = term if term and (not best or term < best) else 0
            n = d.new(rec, ("back", -1))  # label: the token it records, 0 none
            if better(p, rec or best):
                d.delta[s][cls[c]] = ("step", n)
                children(n, p, rec or best)
            else:
                assert rec, p
                d.delta[s][cls[c]] = ("hit", n)  # a leaf: its token, the byte after not read

    children(start, b"", 0)
    m, _ = minimize(d, {})
    return m, cls


# ---- the tokenizer: symbols to the body

RD_KW, RD_RAW, RD_DATA = 0, 1, 2  # how a state reads its symbol

# its actions, as lex.asm's lt_* routines; '=' first: the symbol is read again
# by the next state
TK_ACTS = tuple("put end nbeg ndig nput nend vbeg fbeg nchr npct ndol nlen fend nvar narr skip".split())

TK_SETS: dict[str, set[int]] = {
    "nul": {0}, "quote": {QUOTE}, "dig": set(range(0x30, 0x3A)), "dot": {ord(".")}, "E": {ord("E")},
    "let": set(range(0x41, 0x5B)) - {ord("E")}, "rem": {TK_REM}, "data": {TK_DATA}, "fn": {TK_FN},
    "sp": {SP}, "sign": {TK_PLUS, TK_MINUS}, "pct": {ord("%")}, "dol": {ord("$")}, "colon": {COLON},
    "open": {ord("(")},
}


def tokenizer() -> tuple[Dfa, list[int]]:
    cls = classes(list(TK_SETS.values()))
    of = {n: cls[min(s)] for n, s in TK_SETS.items()}
    of["other"] = cls[ord(",")]
    allc = " ".join(of)
    d = Dfa(max(cls) + 1)
    modes = {"start": RD_KW, "main": RD_KW, "fn": RD_KW, "quote": RD_RAW, "dquote": RD_RAW, "rem": RD_RAW,
             "data": RD_DATA, "lnum": RD_KW, "num": RD_KW, "exp": RD_KW, "name": RD_KW, "fname": RD_KW, "arr": RD_KW}
    S = {n: d.new(m, ("", -1)) for n, m in modes.items()}

    def on(st: str, cs: str, act: str, nxt: str | None) -> None:
        for n in cs.split():
            d.delta[S[st]][of[n]] = (act, S[nxt] if nxt else -1)

    for st in ("start", "main", "fn"):
        on(st, allc, "put", "main")
        on(st, "nul", "end", None)
        on(st, "quote", "put", "quote")
        on(st, "rem", "put", "rem")
        on(st, "data", "put", "data")
        on(st, "fn", "put", "fn")
        on(st, "dig dot", "nbeg", "lnum" if st == "start" else "num")
        on(st, "let E", "vbeg", "name")
    on("fn", "sp", "put", "fn")
    on("fn", "let E", "fbeg", "fname")
    for st, back in (("quote", "main"), ("dquote", "data")):
        on(st, allc, "put", st)
        on(st, "quote", "put", back)
        on(st, "nul", "end", None)
    on("rem", allc, "put", "rem")
    on("rem", "nul", "end", None)
    on("data", allc, "put", "data")
    on("data", "quote", "put", "dquote")
    on("data", "colon", "put", "main")
    on("data", "nul", "end", None)
    # a number: digits, '.', E and the sign after it; the line's number digits
    # only
    for st in ("num", "exp", "lnum"):
        on(st, allc, "=nend", "main")
        on(st, "dig", "ndig", "lnum" if st == "lnum" else "num")
    for st in ("num", "exp"):
        on(st, "dot", "nput", "num")
        on(st, "E", "nput", "exp")
    on("exp", "sign", "nput", "num")
    # a name: letters, digits, then % or $; an array when '(' comes after it,
    # spaces skipped;
    # FN's name has no array but with % or $
    for st in ("name", "fname"):
        on(st, allc, "=nlen" if st == "name" else "=fend", "arr" if st == "name" else "main")
        on(st, "dig let E", "nchr", st)
        on(st, "pct", "npct", "arr")
        on(st, "dol", "ndol", "arr")
    on("arr", allc, "nvar", "main")
    on("arr", "sp", "skip", "arr")
    on("arr", "open", "narr", "main")
    m, _ = minimize(d, {})
    return m, cls


# ---- the statement parser: tokens to the compiler's actions
#
# A grammar per statement, written as the scripts of comp.asm ran it. 'x' a
# token (= is TK_EQ),
# a keyword its token; $ the statement's end, not stepped past; a lower-case
# word the action
# A_WORD, !X a CALL of K_X; [ ] optional, { } repeated, ( | ) a choice. An
# action reads its own
# operands; a token is stepped past by one byte (the next reader skips spaces).

GRAMMAR: dict[str, str] = {
    "END": "!END",
    "FOR": "fv = e fst TO e fk ( STEP e fk for | for1 )",
    "NEXT": "nv { , nv }",
    "DATA": "data",
    "INPUT#": "e tob !FIIN , !INFILE lv in { , lv in } !INEND !FICLR",
    "INPUT": "!INBEG [ '\"' str !SPR ; ] !INPR lv in { , lv in } !INEND",
    "DIM": "dv { , dv }",
    "READ": "lv rd { , lv rd }",
    "LET": "lv = e let",
    "GOTO": "l goto",
    "RUN": "run",
    "IF": "if",
    "RESTORE": "res",
    "GOSUB": "l gosub",
    "RETURN": "ret",
    "REM": "rem",
    "STOP": "!STOP",
    "ON": "e on onl { , onl }",
    "WAIT": "e toa , e tob ( , e tob !WAIT | !WAIT2 )",
    "LOAD": "n0 ( $ | e tos n1 ( $ | , e tob n1 ( $ | , e tob n1 ( $ | , ) ) ) ) narg fop",
    "SAVE": "n0 ( $ | e tos n1 ( $ | , e tob n1 ( $ | , e tob n1 ( $ | , ) ) ) ) narg fop",
    "VERIFY": "n0 ( $ | e tos n1 ( $ | , e tob n1 ( $ | , e tob n1 ( $ | , ) ) ) ) narg fop",
    "DEF": "def",
    "POKE": "e toa , e tob !POKE",
    "PRINT#": "e tob !FIOUT ( $ | , ) pr !FICLR",
    "PRINT": "pr",
    "CONT": "!CONT",
    "LIST": "lst",
    "CLR": "!CLR",
    "CMD": "e tob !FIOUT ( $ | , ) pr",
    "SYS": "e toa !SYS",
    "OPEN": "n0 e tob n1 ( $ | , e tob n1 ( $ | , e tob n1 ( $ | , e tos n1 ) ) ) narg !OPEN",
    "CLOSE": "e tob !CLOSE",
    "GET": "( # e tob !FIIN , lv get { , lv get } !FICLR | !NODIR lv get { , lv get } )",
    "NEW": "!NEW",
    "GO": "TO l goto",
}
# the entry points besides the keywords': a name starts LET; IF's THEN or GOTO
# before a number
ENTRIES = [*(k.decode() for _, k in KW[: 0xA3 - 0x80]), "GO"]
TK_EQ = 0xB2
EOS = (0, COLON)


def ptoken(w: str) -> int:
    if w == "=":
        return TK_EQ
    kw = {k.decode(): tok for tok, k in KW}
    return kw[w] if w in kw else ord(w.strip("'"))


@dataclass
class NState:
    eps: list[int] = field(default_factory=list)
    edges: list[tuple[tuple, int]] = field(default_factory=list)  # ("tok", byte) | ("eos",) | ("act", name)
    final: bool = False


def thompson(text: str, nfa: list[NState]) -> int:
    """The NFA of one grammar; its start state. Its end state is final."""
    words = text.split()
    pos = 0

    def new() -> int:
        nfa.append(NState())
        return len(nfa) - 1

    def alt(stop: str) -> tuple[int, int]:
        nonlocal pos
        s, e = new(), new()
        while True:
            a, b = seq(stop)
            nfa[s].eps.append(a)
            nfa[b].eps.append(e)
            if pos < len(words) and words[pos] == "|":
                pos += 1
                continue
            return s, e

    def seq(stop: str) -> tuple[int, int]:
        nonlocal pos
        s = cur = new()
        while pos < len(words) and words[pos] not in ("|", stop):
            w = words[pos]
            pos += 1
            if w in "[{(":
                a, b = alt({"[": "]", "{": "}", "(": ")"}[w])
                pos += 1
                if w == "[":
                    nfa[a].eps.append(b)
                if w == "{":
                    nfa[a].eps.append(b)
                    nfa[b].eps.append(a)
            else:
                a, b = new(), new()
                if w == "$":
                    sym = ("eos",)
                elif w.startswith("!"):
                    sym = ("act", "K_" + w[1:])
                elif w.islower():
                    sym = ("act", "A_" + w.upper())
                else:
                    sym = ("tok", ptoken(w))
                nfa[a].edges.append((sym, b))
            nfa[cur].eps.append(a)
            cur = b
        return s, cur

    s, e = alt("")
    nfa[e].final = True
    return s


def closure(nfa: list[NState], ss: frozenset[int]) -> frozenset[int]:
    todo, seen = list(ss), set(ss)
    while todo:
        for t in nfa[todo.pop()].eps:
            if t not in seen:
                seen.add(t)
                todo.append(t)
    return frozenset(seen)


# a parser entry: (what, next); what: "tok" step past the byte, "eos" go on
# without it, an action
# A_*/K_* run it (it reads its own operands), "acc" the statement is done, "err"
# ?SYNTAX ERROR
def parser() -> tuple[Dfa, list[int], dict[str, int]]:
    words = {w for g in GRAMMAR.values() for w in g.split()}
    toks = sorted({ptoken(w) for w in words if not (w in "[]{}()|$" or w.startswith("!") or w.islower())})
    cls = classes([set(EOS), *({b} for b in toks)])
    ncls = max(cls) + 1
    nfa: list[NState] = []
    starts = {name: thompson(GRAMMAR[name], nfa) for name in GRAMMAR}
    d = Dfa(ncls)
    index: dict[frozenset[int], int] = {}
    todo: list[frozenset[int]] = []

    def get(ss: frozenset[int]) -> int:
        c = closure(nfa, ss)
        if c not in index:
            index[c] = d.new(None, ("err", -1))
            todo.append(c)
        return index[c]

    named = {name: get(frozenset({s})) for name, s in starts.items()}
    while todo:
        c = todo.pop()
        me = index[c]
        by: dict[tuple, set[int]] = {}
        for q in c:
            for sym, t in nfa[q].edges:
                by.setdefault(sym, set()).add(t)
        acts = [s for s in by if s[0] == "act"]
        final = any(nfa[q].final for q in c)
        assert len(acts) + final <= 1, f"not LL(1): {acts} final={final}"
        if acts:
            d.delta[me] = [(acts[0][1], get(frozenset(by[acts[0]])))] * ncls
        elif final:
            d.delta[me] = [("acc", -1)] * ncls
        for sym, ts in by.items():
            if sym[0] == "tok":
                d.delta[me][cls[sym[1]]] = ("tok", get(frozenset(ts)))
            elif sym[0] == "eos":
                d.delta[me][cls[0]] = ("eos", get(frozenset(ts)))
    m, named = minimize(d, named)
    return m, cls, named


# ---- checks


def corpus_lines() -> list[bytes]:
    rows = [line.split() for line in lexer.CORPUS.read_text().splitlines() if line and not line.startswith("#")]
    return [bytes.fromhex(r[1]) for r in rows]


def random_lines(n: int) -> list[bytes]:
    rnd = random.Random(7)
    words = [k for _, k in KW] + [b"A", b"AB", b"X1", b"FNA", b"I%", b"S$", b"1E5", b"1.5", b"65535", b"65536"]
    words += [b"42405", b'"x y"', b"REM a", b'DATA 1,a ,"b:c"', b":", b" ", b"(", b"?", b"\x81", b"\xff"]
    words += [b"e", b"to", b"go to"]
    out = []
    for _ in range(n):
        parts = [rnd.choice(words) for _ in range(rnd.randint(1, 8))]
        if rnd.random() < 0.3:
            parts.insert(0, str(rnd.randint(0, 70000)).encode())
        out.append(b"".join(p + (b" " if rnd.random() < 0.3 else b"") for p in parts)[:80])
    return out


def fn_bug(text: bytes, ref: Syms, dfa: Syms) -> bool:
    """The difference is only the reference's FN bug: a name it made FN's with no FN before it."""
    def plain(s: Syms) -> set:
        return {(k, 0 if n == 3 else n, a) for (k, n, a) in s.ids}

    return plain(ref) == plain(dfa) and not any(
        n == 3 for (_, n, _) in dfa.ids)


def check(T: dict) -> int:
    """lx_line over the tables against the reference: the body and the names' kinds."""
    bad = 0
    for text in corpus_lines() + random_lines(20000):
        a, b = Syms(), Syms()
        want, got = ref_lex(text, a), asm_lex(T, text, b)
        if (got, b.ids) != (want, a.ids) and not fn_bug(text, a, b):
            bad += 1
            if bad <= 10:
                print(f"DIFF {text!r}\n  ref  {want.hex()}\n  dfa  {got.hex()}")
    print(f"lexer: {bad} differ")
    return 1 if bad else 0


# ---- the old scripts, as the parser's model

# comp.asm's scripts and action numbers before the automata (commit a4ae5de), as
# the parser's model
OLD_A = """
A_BASE  = 64
A_E     = A_BASE
A_X     = A_BASE+1
A_C     = A_BASE+2
A_O     = A_BASE+3
A_FC    = A_BASE+4
A_ES    = A_BASE+5
A_M     = A_BASE+6
A_LM    = A_BASE+7
A_LOOP  = A_BASE+8
A_LV    = A_BASE+9
A_L     = A_BASE+10
A_TOB   = A_BASE+11
A_TOA   = A_BASE+12
A_TOS   = A_BASE+13
A_STR   = A_BASE+14
A_FV    = A_BASE+15
A_FST   = A_BASE+16
A_FK    = A_BASE+17
A_FOR   = A_BASE+18
A_FOR1  = A_BASE+19
A_NV    = A_BASE+20
A_IF    = A_BASE+21
A_ON    = A_BASE+22
A_ONL   = A_BASE+23
A_PR    = A_BASE+24
A_DEF   = A_BASE+25
A_LST   = A_BASE+26
A_DV    = A_BASE+27
A_DATA  = A_BASE+28
A_REM   = A_BASE+29
A_LET   = A_BASE+30
A_IN    = A_BASE+31
A_RD    = A_BASE+32
A_GET   = A_BASE+33
A_GOTO  = A_BASE+34
A_GOSUB = A_BASE+35
A_RUN   = A_BASE+36
A_RES   = A_BASE+37
A_RET   = A_BASE+38
A_N0    = A_BASE+39
A_N1    = A_BASE+40
A_NARG  = A_BASE+41
A_FOP   = A_BASE+42
"""
OLD_CMS = """
cms     label   byte
s_end   db      K_END or LAST
s_data  db      A_DATA or LAST
s_if    db      A_IF or LAST
s_ret   db      A_RET or LAST
s_rem   db      A_REM or LAST
s_stop  db      K_STOP or LAST
s_def   db      A_DEF or LAST
s_print db      A_PR or LAST
s_cont  db      K_CONT or LAST
s_list  db      A_LST or LAST
s_clr   db      K_CLR or LAST
s_new   db      K_NEW or LAST
s_run   db      A_RUN or LAST
s_res   db      A_RES or LAST
s_for   db      A_FV,A_X,TK_EQ,A_E,A_FST,A_X,TK_TO,A_E,A_FK,A_O,TK_STEP,A_E,A_FK
        db      A_FOR or LAST,A_M,A_FOR1 or LAST
s_next  db      A_LM,A_NV,A_LOOP or LAST
s_inputn db     A_E,A_TOB,K_FIIN,A_C,K_INFILE,A_LM,A_LV,A_IN,A_LOOP,K_INEND
        db      K_FICLR or LAST
s_input db      K_INBEG,A_O,'"',A_STR,K_SPR,A_X,';',A_M,K_INPR,A_LM,A_LV,A_IN
        db      A_LOOP,K_INEND or LAST
s_dim   db      A_LM,A_DV,A_LOOP or LAST
s_read  db      A_LM,A_LV,A_RD,A_LOOP or LAST
s_let   db      A_LV,A_X,TK_EQ,A_E,A_LET or LAST
s_go    db      A_X,TK_TO
s_goto  db      A_L,A_GOTO or LAST
s_gosub db      A_L,A_GOSUB or LAST
s_on    db      A_E,A_ON,A_LM,A_ONL,A_LOOP or LAST
s_wait  db      A_E,A_TOA,A_C,A_E,A_TOB,A_O,',',A_E,A_TOB,K_WAIT or LAST,A_M
        db      K_WAIT2 or LAST
s_poke  db      A_E,A_TOA,A_C,A_E,A_TOB,K_POKE or LAST
s_file  db      A_N0,A_ES,A_E,A_TOS,A_N1,A_FC,A_E,A_TOB,A_N1,A_FC,A_E,A_TOB,A_N1
        db      A_FC,A_M,A_NARG,A_FOP or LAST
s_open  db      A_N0,A_E,A_TOB,A_N1,A_FC,A_E,A_TOB,A_N1,A_FC,A_E,A_TOB,A_N1,A_FC
        db      A_E,A_TOS,A_N1,A_M,A_NARG,K_OPEN or LAST
s_close db      A_E,A_TOB,K_CLOSE or LAST
s_sys   db      A_E,A_TOA,K_SYS or LAST
s_printn db     A_E,A_TOB,K_FIOUT,A_FC,A_M,A_PR,K_FICLR or LAST
s_cmd   db      A_E,A_TOB,K_FIOUT,A_FC,A_M,A_PR or LAST
s_get   db      A_O,'#',A_E,A_TOB,K_FIIN,A_C,A_LM,A_LV,A_GET,A_LOOP,K_FICLR or LAST
        db      A_M,K_NODIR,A_LM,A_LV,A_GET,A_LOOP or LAST
"""


def old_scripts() -> tuple[bytes, dict[str, int], dict[int, str]]:
    """The old cms, each statement's offset in it, and the action names by number."""
    import re

    names: dict[str, int] = {"LAST": 0x80, "A_BASE": 64}
    for m in re.finditer(r"^(A_\w+)\s*=\s*A_BASE(?:\+(\d+))?", OLD_A, re.M):
        names[m[1]] = 64 + int(m[2] or 0)
    for m in re.finditer(r"^(K_\w+)\s*=\s*(\d+)\b", (ROOT / "src/x86f/vm/isa.inc").read_text(), re.M):
        names[m[1]] = int(m[2])
    for m in re.finditer(r"^(TK_\w+)\s*=\s*0?([0-9A-F]+)h", (ROOT / "src/x86f/def.inc").read_text(), re.M):
        names[m[1]] = int(m[2], 16)
    out, offs = bytearray(), {}
    for line in OLD_CMS.splitlines()[2:]:
        m = re.match(r"(\w+)?\s+db\s+(.*)", line)
        if not m:
            continue
        if m[1]:
            offs[m[1]] = len(out)
        for item in re.findall(r"(?:'[^']*'|[^,;'])+", re.sub(r"\s;(?=(?:[^']*'[^']*')*[^']*$).*", "", m[2])):
            v = 0
            for part in item.strip().split(" or "):
                part = part.strip()
                v |= ord(part[1]) if part.startswith("'") else names[part]
            out.append(v)
    acts = {v: k for k, v in names.items() if k.startswith(("A_", "K_")) and k != "A_BASE"}
    return bytes(out), offs, acts


SCRIPT_OF = {
    "END": "s_end", "FOR": "s_for", "NEXT": "s_next", "DATA": "s_data", "INPUT#": "s_inputn",
    "INPUT": "s_input", "DIM": "s_dim", "READ": "s_read", "LET": "s_let", "GOTO": "s_goto", "RUN": "s_run",
    "IF": "s_if", "RESTORE": "s_res", "GOSUB": "s_gosub", "RETURN": "s_ret", "REM": "s_rem",
    "STOP": "s_stop", "ON": "s_on", "WAIT": "s_wait", "LOAD": "s_file", "SAVE": "s_file", "VERIFY": "s_file",
    "DEF": "s_def", "POKE": "s_poke", "PRINT#": "s_printn", "PRINT": "s_print", "CONT": "s_cont",
    "LIST": "s_list", "CLR": "s_clr", "CMD": "s_cmd", "SYS": "s_sys", "OPEN": "s_open", "CLOSE": "s_close",
    "GET": "s_get", "NEW": "s_new", "GO": "s_go",
}
A_C, A_X, A_O, A_FC, A_ES, A_M, A_LM, A_LOOP = 66, 65, 67, 68, 69, 70, 71, 72


def script_run(cms: bytes, pc: int, body: bytes, act, acts: dict[int, str]) -> str:
    """cm_scr over a token body: 'acc' or 'err'."""
    i = 0

    def get() -> int:
        nonlocal i
        while i < len(body) and body[i] == SP:
            i += 1
        return body[i] if i < len(body) else 0

    def skm() -> None:
        nonlocal pc
        while cms[pc] & 0x7F != A_M:
            pc += 1
        pc += 1

    while True:
        b = cms[pc]
        pc += 1
        a = b & 0x7F
        if a in (A_X, A_C):
            want = ord(",") if a == A_C else cms[pc]
            pc += a == A_X
            if get() != want:
                return "err"
            i += 1
        elif a == A_O:
            want = cms[pc]
            pc += 1
            if get() == want:
                i += 1
            else:
                skm()
        elif a in (A_FC, A_ES):
            if get() in EOS:
                skm()
            elif a == A_FC:
                if get() != ord(","):
                    return "err"
                i += 1
        elif a == A_LOOP:
            if get() == ord(","):
                i += 1
                pc -= 1
                while cms[pc] != A_LM:
                    pc -= 1
                continue
        elif a not in (A_M, A_LM):
            i = act(acts[a], i)
            if i < 0:
                return "err"
        if b & 0x80:
            return "acc"


def mock(trace: list, body: bytes):
    """Actions over a body of placeholders: v an operand. Each appends to trace; -1 is an error."""
    takes = {f"A_{a}": "v!" for a in ("E", "LV", "FV", "DV")}
    takes |= {f"A_{a}": "v?" for a in ("L", "NV", "ONL", "STR", "RUN", "RES")}
    takes |= {f"A_{a}": "*" for a in ("IF", "PR", "DEF", "LST", "DATA", "REM")}

    def act(name: str, i: int) -> int:
        while i < len(body) and body[i] == SP:
            i += 1
        trace.append((name, i))
        how = takes.get(name, "")
        if how == "*":
            return len(body)
        if how and i < len(body) and body[i] == ord("v"):
            return i + 1
        return -1 if how == "v!" else i

    return act


def check_parser(T: dict) -> int:
    cms, offs, acts = old_scripts()
    pd, pcls, named = parser()
    rnd = random.Random(11)
    alpha = [b"v", b",", b";", b"#", b'"', bytes([TK_EQ]), bytes([0xA4]), bytes([0xA9]), b" ", b":", b"x"]
    bad = 0
    for name in GRAMMAR:
        for _ in range(3000):
            body = b"".join(rnd.choice(alpha) for _ in range(rnd.randint(0, 9)))
            body = body.split(b":")[0] + (b":" if b":" in body else b"")
            t1, t2 = [], []
            r1 = script_run(cms, offs[SCRIPT_OF[name]], body, mock(t1, body), acts)
            start = {"LET": T["ST_LET"], "GO": T["ST_GO"]}.get(name) or T["ps_ent"][ptoken(name) - 0x80]
            r2 = asm_parse(T, start, body, mock(t2, body))
            if (r1, t1) != (r2, t2):
                bad += 1
                if bad <= 8:
                    print(f"PARSE {name} {body!r}\n  script {r1} {t1}\n  dfa    {r2} {t2}")
    print(f"parser: {bad} differ")
    return 1 if bad else 0


# ---- the tables


def rows(name: str, kind: str, vals: list) -> list[str]:
    """A table as db or dw lines of at most 100 columns."""
    text = [f"0{v:02X}h" if kind == "db" else (v if isinstance(v, str) else f"0{v:04X}h") for v in vals]
    out, line = [f"{name:<8}label   {'byte' if kind == 'db' else 'word'}"], []
    for x in text:
        if line and 16 + len(",".join([*line, x])) > 100:
            out.append(f"        {kind}      " + ",".join(line))
            line = []
        line.append(x)
    return [*out, f"        {kind}      " + ",".join(line)] if line else out


def tk_entry(act: str, nx: int) -> int:
    """A tokenizer entry: the action (bit 7: the symbol is read again), the next state above it."""
    return TK_ACTS.index(act.lstrip("=")) | (0x80 if act.startswith("=") else 0) | (nx if nx >= 0 else 0xFF) << 8


def tables() -> dict:
    """The arrays and constants dfa.inc holds, as lex.asm and comp.asm read them."""
    sd, scls = scanner()
    td, tcls = tokenizer()
    pd, pcls, named = parser()
    # the scanner: leaves (a keyword's last byte) numbered last, so a target at
    # SC_LEAF or over is a hit
    leaves = sorted({nx for row in sd.delta for a, nx in row if a == "hit"})
    order = [s for s in range(len(sd.delta)) if s not in leaves] + leaves
    num = {s: n for n, s in enumerate(order)}
    assert len(order) < 0xF8 and order[0] == 0
    nleaf = len(order) - len(leaves)
    return {
        "SC_NC": sd.ncls, "SC_LEAF": nleaf, "TK_NC": td.ncls, "PS_NC": pd.ncls,
        "nstates": (len(order), len(td.delta), len(pd.delta)),
        "ST_LET": named["LET"], "ST_GOTO": named["GOTO"], "ST_GO": named["GO"],
        "sc_cls": scls,
        # a leaf's row is not read
        "sc_tab": [num[nx] if a in ("step", "hit") else SC_CODE[a] for s in order[:nleaf] for a, nx in sd.delta[s]],
        "sc_acc": [sd.label[s] if isinstance(sd.label[s], int) else 0 for s in order],
        "tk_cls": tcls,
        "tk_tab": [tk_entry(a, nx) for row in td.delta for a, nx in row],
        "tk_mode": [td.label[s] for s in range(len(td.delta))],
        "ps_cls": pcls,
        "ps_tab": [(PS_CODE.get(a, a), nx if nx >= 0 else 0xFF) for row in pd.delta for a, nx in row],
        "ps_ent": [named[k.decode()] for _, k in KW[: 0xA3 - 0x80]],
    }


SC_CODE = {"nul": 0xF8, "skip": 0xF9, "byte": 0xFA, "print": 0xFB, "back": 0xFC}
PS_CODE = {"acc": 0xFC, "err": 0xFD, "eos": 0xFE, "tok": 0xFF}


def emit(T: dict) -> str:
    ns = T["nstates"]
    ps = [f"{a}+{nx * 256}" if isinstance(a, str) else (a | nx << 8) for a, nx in T["ps_tab"]]
    out = [
        "; generated by tools/x86dfa.py: the lexer's and the statement",
        "; parser's automata, minimized. States x classes: scanner",
        f"; {ns[0]} x {T['SC_NC']}, tokenizer {ns[1]} x {T['TK_NC']}, parser {ns[2]} x {T['PS_NC']}.",
        f"SC_NC   = {T['SC_NC']}",
        f"SC_LEAF = {T['SC_LEAF']}                ; a keyword's last byte from here: sc_acc",
        *(f"SC_{k.upper():<5} = 0{v:02X}h" for k, v in SC_CODE.items()),
        f"TK_NC   = {T['TK_NC']}",
        *(f"LT_{a.upper():<5} = {n}" for n, a in enumerate(TK_ACTS)),
        f"PS_NC   = {T['PS_NC']}",
        *(f"PS_{k.upper()}  = 0{v:02X}h" for k, v in PS_CODE.items()),
        f"ST_LET  = {T['ST_LET']}",
        f"ST_GOTO = {T['ST_GOTO']}",
        f"ST_GO   = {T['ST_GO']}",
        "        .data",
        *rows("sc_cls", "db", T["sc_cls"]),
        *rows("sc_tab", "db", T["sc_tab"]),
        *rows("sc_acc", "db", T["sc_acc"]),
        *rows("tk_cls", "db", T["tk_cls"]),
        *rows("tk_tab", "dw", T["tk_tab"]),
        *rows("tk_mode", "db", T["tk_mode"]),
        *rows("ps_cls", "db", T["ps_cls"]),
        *rows("ps_tab", "dw", ps),
        *rows("ps_ent", "db", T["ps_ent"]),
        "        .code",
    ]
    return "\n".join(out) + "\n"


# ---- lex.asm and comp.asm over the tables, step for step


def asm_scan(T: dict, t: bytearray, i: int) -> tuple[int, int, int]:
    """lx_scan: (AL, lxs, SI)."""
    while True:
        lxs, dl, dh, ecx = i, 0, 0, 0
        while True:
            t[i] = fold(t[i])
            a = T["sc_tab"][dl * T["SC_NC"] + T["sc_cls"][t[i]]]
            if a < SC_CODE["nul"]:
                i += 1
                if a >= T["SC_LEAF"]:
                    return T["sc_acc"][a], lxs, i
                dl = a
                if T["sc_acc"][a]:
                    dh, ecx = T["sc_acc"][a], i
                continue
            break
        if a == SC_CODE["skip"]:
            i += 1
            continue
        if a == SC_CODE["nul"]:
            return 0, lxs, i
        if a == SC_CODE["byte"]:
            return t[i], lxs, i + 1
        if a == SC_CODE["print"]:
            return TK_PRINT, lxs, i + 1
        return (dh, lxs, ecx) if dh else (t[lxs], lxs, lxs + 1)


def asm_lex(T: dict, text: bytes, syms: Syms) -> bytes:
    """lx_line: the body, its 0 not included."""
    t = bytearray(text + b"\0")
    out = bytearray()
    i, st = 0, 0
    v = f = n = nlenp = lxm = kind = 0
    key = bytearray(2)
    while True:
        mode = T["tk_mode"][st]
        if mode == RD_KW:
            al, lxs, i = asm_scan(T, t, i)
        else:
            lxs = i
            while mode == RD_DATA and t[lxs] >= 0x80 and t[lxs] != TK_PI:
                lxs += 1
            al, i = t[lxs], lxs + (1 if t[lxs] else 0)
        e = T["tk_tab"][st * T["TK_NC"] + T["tk_cls"][al]]
        if e & 0x80:
            i = lxs
        st = e >> 8
        act = TK_ACTS[e & 0x7F]
        if act in ("put", "end"):
            out.append(al)
        elif act == "nbeg":
            n, f = len(out), al == ord(".")
            out.append(al)
            v = al & 15
        elif act == "ndig":
            out.append(al)
            if not f:
                v = v * 10 + (al & 15)
                f = v > 65535
        elif act == "nput":
            out.append(al)
            f = True
        elif act == "nend":
            if not f:
                del out[n:]
                out += bytes([M_INT, v & 255, v >> 8 & 255])
        elif act in ("vbeg", "fbeg"):
            kind, key = (3 if act == "fbeg" else 0), bytearray([al, 0])
            out += bytes([M_VAR, M_VAR, al])
            nlenp = len(out) - 1
        elif act in ("nchr", "npct", "ndol"):
            if act != "nchr":
                kind = 1 if act == "npct" else 2
            if key[1] == 0:
                key[1] = al
            out.append(al)
            if act != "nchr":
                out[nlenp - 1], lxm = len(out) - nlenp, i
        elif act in ("nlen", "fend"):
            out[nlenp - 1], lxm = len(out) - nlenp, i
            if act == "fend":
                out.append(syms.find(key, kind, 0))
        elif act in ("nvar", "narr"):
            i = lxm
            out.append(syms.find(key, kind, 0x80 if act == "narr" else 0))
        if st == 0xFF:
            return bytes(out[:-1])


def asm_parse(T: dict, start: int, body: bytes, act) -> str:
    """cm_dfa over a token body: 'acc' or 'err'; act(name, i) runs an action and gives the new index, -1 an error."""
    st, i = start, 0
    while True:
        while i < len(body) and body[i] == SP:
            i += 1
        b = body[i] if i < len(body) else 0
        what, st = T["ps_tab"][st * T["PS_NC"] + T["ps_cls"][b]]
        if what == PS_CODE["acc"]:
            return "acc"
        if what == PS_CODE["err"]:
            return "err"
        if what == PS_CODE["tok"]:
            i += 1
        elif what != PS_CODE["eos"]:
            i = act(what, i)
            if i < 0:
                return "err"


if __name__ == "__main__":
    if sys.argv[1:] == ["check"]:
        T = tables()
        bad = check(T) | check_parser(T)
        if OUT.read_text() != emit(T):
            print(f"{OUT.relative_to(ROOT)} is not what the generator writes: run tools/x86dfa.py")
            bad = 1
        sys.exit(bad)
    OUT.write_text(emit(tables()))
