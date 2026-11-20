#!/usr/bin/env python3
"""gen_dfa.py [check]: the flat build's keyword scanner and statement parser as minimized automata.

  gen_dfa.py        write src/core/comp/dfa.h and src/core/io/gen.h (the messages and keyword lists)
  gen_dfa.py check  both as the generator writes them; the scanner against the keyword list

The keywords are QBasic's: llrm's NAMES_QB45, the words BC reserves. A word is matched whole,
never as a prefix: the scanner reads its letters and gives the keyword it spells, or none, and
then the word is a name. A token is a byte from 80h to FEh, or FFh and a byte. The first block
keeps the 16-bit build's order, so the messages that name its keywords and the compiler's ranges
of operators and functions hold; a spelling None is not lexed (INPUT#, LOAD, ..), only listed.
The statement parser is an automaton over a line's tokens from one LL(1) grammar per statement,
with the compiler's actions on its edges.
"""

import random
import sys
from dataclasses import dataclass, field
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MSGS = ROOT / "src/core/io/gen_msgs.hex"  # the 16-bit build's packed messages
COUT = ROOT / "src/core/comp/dfa.h"
CGEN = ROOT / "src/core/io/gen.h"
# the parser's actions, A_BASE on, in comp/comp.inc's order
ACTIONS = """E LV L TOB TOA TOS STR FV FST FK FOR FOR1 NV IF ON ONL PR LST DV DATA REM LET IN RD GET GOTO
GOSUB RUN RES RET N0 N1 NARG FOP SWAP TOI TOD VA WR LIN LE""".split()

# (listed as, lexed as): the 16-bit build's tokens 80h..CCh
C64 = [
    ("END", "END"), ("FOR", "FOR"), ("NEXT", "NEXT"), ("DATA", "DATA"), ("INPUT#", None),
    ("INPUT", "INPUT"), ("DIM", "DIM"), ("READ", "READ"), ("LET", "LET"), ("GOTO", "GOTO"),
    ("RUN", "RUN"), ("IF", "IF"), ("RESTORE", "RESTORE"), ("GOSUB", "GOSUB"), ("RETURN", "RETURN"),
    ("REM", "REM"), ("STOP", "STOP"), ("ON", "ON"), ("WAIT", "WAIT"), ("LOAD", None), ("SAVE", None),
    ("VERIFY", None), ("DEF", "DEF"), ("POKE", "POKE"), ("PRINT#", None), ("PRINT", "PRINT"),
    ("CONT", None), ("LIST", "LIST"), ("CLR", "CLEAR"), ("CMD", None), ("SYS", None), ("OPEN", "OPEN"),
    ("CLOSE", "CLOSE"), ("GET", "GET"), ("NEW", None), ("TAB(", "TAB"), ("TO", "TO"), ("FN", None),
    ("SPC(", "SPC"), ("THEN", "THEN"), ("NOT", "NOT"), ("STEP", "STEP"), ("+", None), ("-", None),
    ("*", None), ("/", None), ("^", None), ("AND", "AND"), ("OR", "OR"), (">", None), ("=", None),
    ("<", None), ("SGN", "SGN"), ("INT", "INT"), ("ABS", "ABS"), ("USR", None), ("FRE", "FRE"),
    ("POS", "POS"), ("SQR", "SQR"), ("RND", "RND"), ("LOG", "LOG"), ("EXP", "EXP"), ("COS", "COS"),
    ("SIN", "SIN"), ("TAN", "TAN"), ("ATN", "ATN"), ("PEEK", "PEEK"), ("LEN", "LEN"), ("STR$", "STR$"),
    ("VAL", "VAL"), ("ASC", "ASC"), ("CHR$", "CHR$"), ("LEFT$", "LEFT$"), ("RIGHT$", "RIGHT$"),
    ("MID$", "MID$"), ("GO", None),
]
# the rest of the one-byte tokens, CDh..FEh
MORE = """ELSE ELSEIF ENDIF CASE SELECT DO LOOP WHILE WEND UNTIL EXIT SUB FUNCTION DECLARE CALL SHARED
STATIC CONST TYPE AS INTEGER LONG SINGLE DOUBLE STRING DEFINT DEFLNG DEFSNG DEFDBL DEFSTR REDIM
ERASE OPTION BASE IS SWAP RANDOMIZE CLS LOCATE COLOR SYSTEM MOD XOR EQV IMP BYVAL LBOUND UBOUND
INSTR FIX INKEY$""".split()
# NAMES_QB45 (crates/frontends/qb/src/front/reserved.rs in llrm)
QB45 = """ABS ACCESS ALIAS AND ANY APPEND AS ASC ATN BASE BEEP BINARY BLOAD BSAVE BYVAL CALL CALLS CASE
CDBL CDECL CHAIN CHDIR CHR$ CINT CIRCLE CLEAR CLNG CLOSE CLS COLOR COM COMMAND$ COMMON CONST COS CSNG
CSRLIN CVD CVDMBF CVI CVL CVS CVSMBF DATA DATE$ DECLARE DEF DEFDBL DEFINT DEFLNG DEFSNG DEFSTR DIM DO
DOUBLE DRAW ELSE ELSEIF END ENDIF ENVIRON ENVIRON$ EOF EQV ERASE ERDEV ERDEV$ ERL ERR ERROR EXIT EXP
FIELD FILEATTR FILES FIX FOR FRE FREEFILE FUNCTION GET GOSUB GOTO HEX$ IF IMP INKEY$ INP INPUT INPUT$
INSTR INT INTEGER IOCTL IOCTL$ IS KEY KILL LBOUND LCASE$ LEFT$ LEN LET LINE LIST LOC LOCAL LOCATE LOCK
LOF LOG LONG LOOP LPOS LPRINT LSET LTRIM$ MID$ MKD$ MKDIR MKDMBF$ MKI$ MKL$ MKS$ MKSMBF$ MOD NAME NEXT
NOT OCT$ OFF ON OPEN OPTION OR OUT OUTPUT PAINT PALETTE PCOPY PEEK PEN PLAY PMAP POINT POKE POS PRINT
PRESET PSET PUT RANDOM RANDOMIZE READ REDIM REM RESET RESTORE RESUME RETURN RIGHT$ RMDIR RND RSET
RTRIM$ RUN SADD SCREEN SEEK SEG SELECT SETMEM SGN SHARED SHELL SIGNAL SIN SINGLE SLEEP SOUND SPACE$
SPC SQR STATIC STEP STICK STOP STR$ STRIG STRING STRING$ SUB SWAP SYSTEM TAB TAN THEN TIME$ TIMER TO
TROFF TRON TYPE UBOUND UCASE$ UEVENT UNLOCK UNTIL USING VAL VARPTR VARPTR$ VARSEG VIEW WAIT WEND WHILE
WIDTH WINDOW WRITE XOR""".split()

PAGE0 = [*C64, *((w, w) for w in MORE)]
assert len(PAGE0) == 0xFF - 0x80, len(PAGE0)
_lexed = {s for _, s in PAGE0 if s}
EXTRA = ["SHORT", "SSEG", "PRESERVE", "EXPLICIT", "DEFSHORT", "BYTE", "DEFBYTE", "CBYTE", "CONTINUE", "IMPORT",
         "FROM", "PRIVATE", "PUBLIC"]
# ours: a 2-byte integer, for records' QBasic layout (INTEGER is 4 bytes); PDS's REDIM PRESERVE; VBDOS's OPTION EXPLICIT;
# ours: an unsigned byte; CONTINUE FOR | DO | WHILE (the lexer makes it a keyword only there: lexr_continues);
# IMPORT and FROM, keywords only where they start such a statement (comp/lexer.c lexr_word); PRIVATE before SUB,
# FUNCTION, CONST or TYPE, PUBLIC after DIM: a module's exports (comp/bind.c)
# ours, listed only, by its name in the code: (:), an array whole by its elements (comp/lexer.c)
LISTED1 = {"(:)": "WHOLE"}
PAGE1 = [w for w in QB45 if w not in _lexed] + EXTRA + list(LISTED1)
TOKENS: dict[str, int] = {s: 0x80 + n for n, (_, s) in enumerate(PAGE0) if s}
TOKENS |= {w: 0x100 + n for n, w in enumerate(PAGE1) if w not in LISTED1}  # FFh, n
assert set(QB45) <= set(TOKENS), set(QB45) - set(TOKENS)
assert len(PAGE1) < 256

# the asm names of the one-byte tokens the code uses: TK_ and the spelling, but these
ASM_NAME = {"INPUT#": "INPUTN", "PRINT#": "PRINTN", "RESTORE": "RES", "RETURN": "RET", "CLR": "CLR",
            "TAB(": "TAB", "SPC(": "SPC", "+": "PLUS", "-": "MINUS", "*": "MUL", "/": "DIV", "^": "POW",
            ">": "GT", "=": "EQ", "<": "LT", "MID$": "MID"}
# bytes the lexer turns into tokens
CHAR_TOK = {ord("+"): 0xAA, ord("-"): 0xAB, ord("*"): 0xAC, ord("/"): 0xAD, ord("^"): 0xAE,
            ord(">"): 0xB1, ord("="): 0xB2, ord("<"): 0xB3, ord("?"): TOKENS["PRINT"]}
TK_EQ, TK_REM, TK_DATA = 0xB2, TOKENS["REM"], TOKENS["DATA"]


def asm_name(listed: str) -> str:
    return "TK_" + ASM_NAME.get(listed, listed.replace("$", "S"))


# ---- automata


@dataclass
class Dfa:
    """States 0..n-1, start 0. delta[s][cls] = (action, next), next -1 stops. label[s]: what the
    state itself means (a keyword's token), kept apart by the minimization."""

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


# ---- the scanner: a word's letters, upper case, to its keyword
#
# State 0 is dead: no keyword starts so; KW_START the start. A word is
# letters, digits and dots; a '$' straight after it is read too when that
# spells a keyword (MID$). acc: the token, 100h + n for FFh n, else 0.


def scanner() -> tuple[Dfa, list[int], int]:
    words = sorted(TOKENS)
    chars = sorted({ord(c) for w in words for c in w})
    cls = classes([{c} for c in chars])
    d = Dfa(max(cls) + 1)
    dead = d.new(0, ("", 0))
    start = d.new(0, ("", 0))
    node: dict[str, int] = {"": start}
    for w in words:
        for i in range(1, len(w) + 1):
            p = w[:i]
            if p not in node:
                node[p] = d.new(TOKENS.get(p, 0), ("", dead))
                d.delta[node[w[: i - 1]]][cls[ord(w[i - 1])]] = ("", node[p])
    m, named = minimize(d, {"start": start})
    assert m.label[0] == 0 and all(t == 0 for _, t in m.delta[0])
    return m, cls, named["start"]


def kw_step(T: dict, st: int, cls: int) -> int:
    """lx_word's step: state st's transition on class cls in its list, else 0 (dead)."""
    for i in range(T["kw_off"][st], T["kw_off"][st + 1]):
        if T["kw_tc"][i] == cls:
            return T["kw_tn"][i]
    return 0


def ps_step(T: dict, st: int, cls: int) -> tuple:
    """cm_dfa's step: state st's exception for class cls, else its default."""
    for i in range(T["ps_off"][st], T["ps_off"][st + 1]):
        if T["ps_xc"][i] == cls:
            return T["ps_xv"][i]
    return T["ps_def"][st]


def scan_word(T: dict, word: str) -> int:
    """lx_word over the tables: the keyword's token, 0 for a name."""
    st = T["KW_START"]
    for c in word:
        st = kw_step(T, st, T["kw_cls"][ord(c)])
    return T["kw_acc"][st]


# ---- the statement parser: tokens to the compiler's actions
#
# A grammar per statement. 'x' a byte (= is TK_EQ), a keyword its token; $ the
# statement's end, not stepped past; a lower-case word the action A_WORD, !X a
# CALL of K_X; [ ] optional, { } repeated, ( | ) a choice. An action reads its
# own operands; a token is stepped past by one byte.

GRAMMAR: dict[str, str] = {
    "END": "!END",
    "SYSTEM": "!END",
    "FOR": "fv = e fst TO e fk ( STEP e fk for | for1 )",
    "NEXT": "nv { , nv }",
    "DATA": "data",
    "INPUT": "( # e toi !FIIN , !INBEG !INFILE lv in { , lv in } !INEND !FICLR | !INBEG ( '\"' str !SPR ( ; !INPR | , !INLN ) | !INPR ) lv in { , lv in } !INEND )",
    "DIM": "[ SHARED ] dv { , dv }",
    "CONST": "lv = e let { , lv = e let }",
    "DEFINT": "data",
    "DEFLNG": "data",
    "DEFSNG": "data",
    "DEFDBL": "data",
    "DEFSTR": "data",
    "READ": "lv rd { , lv rd }",
    "LET": "lv = le let",
    "GOTO": "l goto",
    "RUN": "run",
    "IF": "if",
    "RESTORE": "res",
    "GOSUB": "l gosub",
    "RETURN": "ret",
    "REM": "rem",
    "STOP": "!STOP",
    "ON": "e on onl { , onl }",
    "POKE": "e toi , e tob !POKE",
    "OUT": "e toi , e toi !OUT",
    "WAIT": "e toi , e toi ( , e toi !WAIT | !WAIT2 )",
    "PRINT": "( # e toi !FIOUT , pr !FICLR | pr )",
    "WRITE": "( # e toi !FIOUT , wr !FICLR | wr )",
    "LINE": "INPUT ( # e toi !FIIN , !INLN lv lin !FICLR | [ ; ] [ '\"' str !SPR ( ; | , ) ] !INLN lv lin )",
    "OPEN": "!ODEF e tos ( [ FOR ( INPUT !OMI | OUTPUT !OMO | APPEND !OMA | RANDOM !OMR | BINARY !OMB ) ]"
            " [ ACCESS ( READ [ WRITE ] | WRITE ) ] [ SHARED | LOCK ( READ [ WRITE ] | WRITE ) ]"
            " AS [ # ] e toi [ LEN = e toi !OREC ] !OPEN | , [ # ] e toi , e tos [ , e toi !OREC ] !OPEN2 )",
    "CLOSE": "( $ !CLALL | [ # ] e toi !CLOSE { , [ # ] e toi !CLOSE } )",
    "RESET": "!CLALL",
    "KILL": "e tos !KILL",
    "MKDIR": "e tos !MKDIR",
    "RMDIR": "e tos !RMDIR",
    "CHDIR": "e tos !CHDIR",
    "NAME": "e tos AS e tos !NAME",
    "FILES": "( $ n0 narg | e tos n0 n1 narg ) !FILES",
    "SEEK": "[ # ] e toi , e toi !SEEK",
    "GET": "[ # ] e toi ( , ( , n0 narg va | e toi ( , va | !GNV ) ) | n0 narg !GNV ) !GET",
    "PUT": "[ # ] e toi ( , ( , n0 narg va | e toi ( , va | !GNV ) ) | n0 narg !GNV ) !PUT",
    "FIELD": "[ # ] e toi !FLDB , e toi AS va !FLD { , e toi AS va !FLD }",
    "LSET": "va = e tos !LSET",
    "RSET": "va = e tos !RSET",
    "BSAVE": "e tos , e toi , e toi !BSAVE",
    "BLOAD": "e tos ( , e toi n0 n1 narg | n0 narg ) !BLOAD",
    "LOCK": "[ # ] e toi [ , e toi [ TO e toi !DROP ] !DROP ] !LOCK",
    "UNLOCK": "[ # ] e toi [ , e toi [ TO e toi !DROP ] !DROP ] !LOCK",
    "CLEAR": "!CLR",
    "SWAP": "swap",
    "ERROR": "e toi !ERROR",
    "TRON": "!TRON",
    "TROFF": "!TROFF",
    "RANDOMIZE": "( $ !RNDQ | e tod ) !RNDZ",
    "SLEEP": "( $ n0 narg | e toi ) !SLEEP",
    "DATE$": "= e tos !SDATE",
    "TIME$": "= e tos !STIME",
    "ENVIRON": "e tos !ENVSET",
    "SHELL": "( $ !SHELL0 | e tos !SHELL )",
    "CHAIN": "e tos !CHAIN",
}
# what ends a statement (st_eos): its own classes, ':' kept apart from FFh 3Ah's second byte only
# where that keyword is wanted
EOS = (0, ord(":"), TK_REM, TOKENS["ELSE"])


def ptoken(w: str) -> list[int]:
    """A grammar word's bytes in the body: a token, FFh n, or a character."""
    if w == "=":
        return [TK_EQ]
    if w in TOKENS:
        t = TOKENS[w]
        assert t != 0x100 + ord(" "), w  # st_get skips a space: not in a grammar
        return [t] if t < 0x100 else [0xFF, t - 0x100]
    return [ord(w.strip("'"))]


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
                nfa[cur].eps.append(a)
            else:
                a, b = new(), new()
                nfa[cur].eps.append(a)
                if w == "$":
                    sym = ("eos",)
                elif w.startswith("!"):
                    sym = ("act", "K_" + w[1:])
                elif w.islower():
                    sym = ("act", "A_" + w.upper())
                else:
                    *head, last = ptoken(w)
                    for byte in head:  # FFh n: a state between
                        m = new()
                        nfa[a].edges.append((("tok", byte), m))
                        a = m
                    sym = ("tok", last)
                nfa[a].edges.append((sym, b))
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
# without it, an action A_*/K_* run it (it reads its own operands), "acc" the
# statement is done, "err" a syntax error
def parser() -> tuple[Dfa, list[int], dict[str, int]]:
    words = {w for g in GRAMMAR.values() for w in g.split()}
    toks = sorted({b for w in words if not (w in "[]{}()|$" or w.startswith("!") or w.islower()) for b in ptoken(w)})
    cls = classes([*({b} for b in EOS), *({b} for b in toks)])
    ncls = max(cls) + 1
    nfa: list[NState] = []
    starts = {name: thompson(GRAMMAR[name], nfa) for name in GRAMMAR}
    d = Dfa(ncls)
    d.new(None, ("err", -1))  # state 0: every byte an error
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
            # past a two-byte keyword's head the action could not have its byte
            assert ("tok", 0xFF) not in by, f"{acts[0][1]} beside a two-byte keyword"
            d.delta[me] = [(acts[0][1], get(frozenset(by[acts[0]])))] * ncls
        elif final:
            d.delta[me] = [("acc", -1)] * ncls
        for sym, ts in by.items():
            if sym[0] == "tok":
                d.delta[me][cls[sym[1]]] = ("tok", get(frozenset(ts)))
        for sym, ts in by.items():  # after the tokens: a class one shares stays the token's
            if sym[0] == "eos":
                for b in EOS:
                    if d.delta[me][cls[b]][0] != "tok":
                        d.delta[me][cls[b]] = ("eos", get(frozenset(ts)))
    m, named = minimize(d, named)
    assert len(m.delta) < 0xFFFF
    return m, cls, named


# ---- the tables


def rows(name: str, kind: str, vals: list) -> list[str]:
    """A table as db, dw or dd lines of at most 100 columns."""
    width = {"db": 2, "dw": 4, "dd": 8}[kind]
    text = [v if isinstance(v, str) else f"0{v:0{width}X}h" for v in vals]
    out, line = [f"{name:<8} label   {dict(db='byte', dw='word', dd='dword')[kind]}"], []
    for x in text:
        if line and 16 + len(",".join([*line, x])) > 100:
            out.append(f"        {kind}      " + ",".join(line))
            line = []
        line.append(x)
    return [*out, f"        {kind}      " + ",".join(line)] if line else out


PS_CODE = {"acc": 0xFC, "err": 0xFD, "eos": 0xFE, "tok": 0xFF}


def sparse_kw(delta: list) -> dict:
    """The scanner's transitions, a list per state (most have one): kw_off its first, the
    next state's its end; kw_tc a transition's class, kw_tn its state. None: the dead state 0."""
    off, tc, tn = [], [], []
    for row in delta:
        off.append(len(tc))
        for c, (_, nx) in enumerate(row):
            if nx:
                tc.append(c)
                tn.append(nx)
    return {"kw_off": [*off, len(tc)], "kw_tc": tc, "kw_tn": tn}


def sparse_ps(tab: list, ncls: int) -> dict:
    """The parser's entries, a default per state (its most common) and the classes that
    differ: ps_off its first, the next state's its end; ps_xc a class, ps_xv its entry."""
    dflt, off, xc, xv = [], [], [], []
    for s in range(0, len(tab), ncls):
        row = tab[s : s + ncls]
        d = max(row, key=row.count)
        dflt.append(d)
        off.append(len(xc))
        for c, e in enumerate(row):
            if e != d:
                xc.append(c)
                xv.append(e)
    return {"ps_def": dflt, "ps_off": [*off, len(xc)], "ps_xc": xc, "ps_xv": xv}


def class_names(pcls: list[int], ncls: int) -> list[str]:
    """What a parser class is, as a syntax error's "expected" names it: the grammar's word whose
    byte it holds (a keyword's second byte after FFh too), the statement's end, a string; empty
    for FFh and the bytes no grammar names."""
    words = {w for g in GRAMMAR.values() for w in g.split()
             if not (w in "[]{}()|$" or w.startswith("!") or w.islower())}
    by_byte: dict[int, set[str]] = {}
    for w in words:
        by_byte.setdefault(ptoken(w)[-1], set()).add(w.strip("'"))
    names = []
    for c in range(ncls):
        bs = [b for b in range(256) if pcls[b] == c]
        named = {w for b in bs for w in by_byte.get(b, ())}
        if 0 in bs:
            names.append("the end of the statement")
        elif named == {'"'}:
            names.append("a string")
        elif len(named) == 1 and 0xFF not in bs:
            names.append(f"`{named.pop()}`")
        else:
            names.append("")
    return names


def tables() -> dict:
    """The arrays and constants dfa.inc holds, as lex.asm and comp.asm read them."""
    sd, scls, kstart = scanner()
    pd, pcls, named = parser()
    by_tok = {TOKENS[s]: s for s in GRAMMAR}
    return {
        "KW_NC": sd.ncls, "KW_START": kstart, "nstates": (len(sd.delta), len(pd.delta)), "PS_NC": pd.ncls,
        "ST_LET": named["LET"], "ST_GOTO": named["GOTO"],
        "kw_cls": scls,
        "kw_tab": [nx for row in sd.delta for _, nx in row],
        **sparse_kw(sd.delta),
        "kw_start": [nx for _, nx in sd.delta[kstart]],  # the start's row whole: every word's first step
        "kw_acc": [lab for lab in sd.label],
        "chr_tok": [CHAR_TOK.get(b, 0) for b in range(256)],
        "ps_cls": pcls,
        "ps_cname": class_names(pcls, pd.ncls),
        "ps_tab": (ps_tab := [(PS_CODE.get(a, a), nx if nx >= 0 else 0xFFFF) for row in pd.delta for a, nx in row]),
        **sparse_ps(ps_tab, pd.ncls),
        # a token that starts no statement: state 0, all errors; ps_ent1 FFh n's
        "ps_ent": [named[by_tok[t]] if t in by_tok else 0 for t in range(0x80, 0xFF)],
        "ps_ent1": [named[by_tok[0x100 + n]] if 0x100 + n in by_tok else 0 for n in range(len(PAGE1))],
    }



def c_rows(ctype: str, name: str, vals: list) -> list[str]:
    """A C array's definition, lines of at most 100 columns."""
    out, line = [f"const {ctype} {name}[{len(vals)}] = {{"], "   "
    for v in vals:
        x = f" {v},"
        if len(line) + len(x) > 100:
            out.append(line)
            line = "   "
        line += x
    return [*out, line, "};"]


def c_emit(T: dict) -> str:
    """dfa.h: the tokens, the codes and the tables for the C port; the tables defined where
    DFA_DATA is (comp/lexer.c)."""
    ns = T["nstates"]

    def ps(es: list) -> list:
        return [f"{a} + ({nx} << 8)" if isinstance(a, str) else f"0x{a | nx << 8:X}" for a, nx in es]

    acts = {a for row in parser()[0].delta for a, _ in row if a.startswith("A_")}
    assert acts <= {f"A_{a}" for a in ACTIONS}, acts
    tabs = [("uint8_t", "lexr_kw_class", T["kw_cls"]), ("uint16_t", "lexr_kw_first", T["kw_off"]),
            ("uint8_t", "lexr_kw_char", T["kw_tc"]), ("uint16_t", "lexr_kw_next", T["kw_tn"]),
            ("uint16_t", "lexr_kw_token", T["kw_acc"]),
            ("uint16_t", "lexr_kw_start", T["kw_start"]), ("uint8_t", "lexr_char_token", T["chr_tok"]),
            ("uint8_t", "lexr_ps_class", T["ps_cls"]), ("uint32_t", "lexr_ps_default", ps(T["ps_def"])),
            ("uint16_t", "lexr_ps_first", T["ps_off"]), ("uint8_t", "lexr_ps_xclass", T["ps_xc"]),
            ("uint32_t", "lexr_ps_xentry", ps(T["ps_xv"])), ("uint16_t", "lexr_ps_entry", T["ps_ent"]),
            ("uint16_t", "lexr_ps_entry1", T["ps_ent1"])]
    out = [
        "/* generated by tools/gen_dfa.py: the keyword scanner's and the statement parser's automata,",
        f"   minimized. States x classes: scanner {ns[0]} x {T['KW_NC']}, parser {ns[1]} x {T['PS_NC']}; each",
        "   sparse: the scanner's transitions a list per state, the parser's a default and its",
        "   exceptions, an entry its code | the next state << 8. The tables are defined where DFA_DATA",
        "   is (comp/lexer.c). */",
        "#ifndef COMP_DFA_H",
        "#define COMP_DFA_H",
        '#include "vm/exec.h"',
        "",
        "/* The one-byte tokens; TK_X the escape of the rest, TK1_* their second byte. */",
        "enum : uint8_t {",
        *(f"    {asm_name(lst)} = 0x{0x80 + n:02X}," for n, (lst, _) in enumerate(PAGE0)),
        "    TK_X = 0xFF,",
        "};",
        "enum : uint8_t {",
        *(f"    TK1_{LISTED1.get(w, w.replace('$', 'S'))} = {n}," for n, w in enumerate(PAGE1)),
        "};",
        "enum {",
        f"    KW_NC = {T['KW_NC']},",
        f"    KW_START = {T['KW_START']},",
        f"    PS_NC = {T['PS_NC']},",
        f"    ST_LET = {T['ST_LET']},",
        f"    ST_GOTO = {T['ST_GOTO']},",
        "};",
        "enum : uint8_t {",
        *(f"    PS_{k.upper()} = 0x{v:02X}," for k, v in PS_CODE.items()),
        "};",
        "/* The parser's actions: under A_BASE a CALL's K_*, from it the compiler's (comp.c). */",
        "enum : uint8_t {",
        "    A_BASE = 0xC8,",
        *(f"    A_{a} = A_BASE + {n}," for n, a in enumerate(ACTIONS)),
        "    A_END,",
        "};",
        "static_assert(K_N <= A_BASE && A_END < PS_ACC);",
        "",
        *(f"extern const {t} {n}[{len(v)}];" for t, n, v in tabs),
        f"extern const char *const lexr_ps_classname[{len(T['ps_cname'])}]; /* empty: none */",
        "",
        "#ifdef DFA_DATA",
    ]
    for t, n, v in tabs:
        out += c_rows(t, n, v)
    out += [f"const char *const lexr_ps_classname[{len(T['ps_cname'])}] = {{",
            *(f'    "{n}",' for n in T["ps_cname"]), "};", "#endif", "#endif"]
    return "\n".join(out) + "\n"


def kwlist(words: list[str]) -> list[int]:
    """A keyword list: each word, its last byte with bit 7; 0 ends it."""
    out: list[int] = []
    for w in words:
        b = list(w.encode())
        b[-1] |= 0x80
        out += b
    return [*out, 0]


KWLIST = [s or lst for lst, s in PAGE0]  # g_kwlist: the one-byte tokens, as QBasic spells them



def c_gen_text() -> str:
    """gen.h: gen.inc's tables for the C port, defined where GEN_DATA is (io/console.c)."""
    tabs = [("cons_msgs", [int(x, 16) for x in MSGS.read_text().split()]), ("cons_kwlist", kwlist(KWLIST)),
            ("cons_kwlist1", kwlist(PAGE1))]
    out = ["/* generated by tools/gen_dfa.py: the messages (io/gen_msgs.hex), the one-byte",
           "   tokens' keywords (g_kwlist) and TK_X n's (g_kwlist1), each word's last byte with bit 7,",
           "   0 ending the list. One array, as gen.inc lays them out: a message refers to entries past",
           "   the messages, the keywords'. cons_starts: each entry's offset, by list (its _at).",
           "   Defined where GEN_DATA is (io/console.c). */",
           "#ifndef IO_GEN_H", "#define IO_GEN_H", "#include <stdint.h>", "",
           f"extern const uint8_t cons_lists[{sum(len(v) for _, v in tabs)}];"]
    at = 0
    for n, v in tabs:
        out.append(f"#define {n} (cons_lists + {at})")
        at += len(v)
    starts, at = [], 0
    for n, v in tabs:  # each entry's offset in cons_lists, a list's first entry at its index
        out.append(f"#define {n}_at (cons_starts + {len(starts)})")
        starts += [at] + [at + i + 1 for i, b in enumerate(v[:-1]) if b & 0x80 and v[i + 1]]
        at += len(v)
    out.append(f"extern const uint16_t cons_starts[{len(starts)}];")
    out += ["", "#ifdef GEN_DATA"]
    out += c_rows("uint8_t", "cons_lists", [f"0x{b:02X}" for _, v in tabs for b in v])
    out += c_rows("uint16_t", "cons_starts", [str(a) for a in starts])
    return "\n".join([*out, "#endif", "#endif"]) + "\n"


def check(T: dict) -> int:
    """Every keyword scans to its token, and other words to none; the sparse tables give each
    state and class the dense one's entry."""
    nk, npc = T["KW_NC"], T["PS_NC"]
    sparse = [("kw", s, c) for s in range(T["nstates"][0]) for c in range(nk)
              if kw_step(T, s, c) != T["kw_tab"][s * nk + c]]
    sparse += [("ps", s, c) for s in range(T["nstates"][1]) for c in range(npc)
               if ps_step(T, s, c) != T["ps_tab"][s * npc + c]]
    print(f"sparse tables: {len(sparse)} differ")
    bad = [w for w, t in TOKENS.items() if scan_word(T, w) != t] + sparse
    rnd = random.Random(7)
    names = ["A", "X1", "I", "COUNT", "FNA", "REMARK", "ENDX", "PRINTX", "TO2", "MID", "INPU", "U.V"]
    names += ["".join(rnd.choice("ABCDEFGHIJKLMNOPRSTUVWXYZ") for _ in range(rnd.randint(1, 7))) for _ in range(5000)]
    bad += [w for w in names if w not in TOKENS and scan_word(T, w) != 0]
    for w in bad[:10]:
        print(f"DIFF {w}")
    print(f"scanner: {len(bad)} differ")
    return 1 if bad else 0


if __name__ == "__main__":
    T = tables()
    if sys.argv[1:] == ["check"]:
        bad = check(T)
        stale = [p for p, t in ((COUT, c_emit(T)), (CGEN, c_gen_text()))
                 if not p.exists() or p.read_text() != t]
        for p in stale:
            print(f"{p.relative_to(ROOT)} is not what the generator writes: run tools/gen_dfa.py")
        sys.exit(1 if stale else bad)
    COUT.write_text(c_emit(T))
    CGEN.write_text(c_gen_text())
