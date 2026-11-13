"""The flat build's modules: their shared symbols as EXTERNDEFs.

    python3 tools/x86syms.py [IDE]         write src/x86f/syms.inc (or ui/)
    python3 tools/x86syms.py check         syms.inc as written? constants and
                                           macros shared but not in a header?
                                           an @-label in two modules?
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent / 'src' / 'x86f'
CORE = ['main.asm', 'vm/exec.asm', 'vm/flow.asm', 'vm/calls.asm', 'vm/sys.asm', 'vm/bin.asm',
        'vm/builtins.asm', 'vm/bc.asm', 'comp/lex.asm', 'comp/decl.asm', 'comp/block.asm', 'comp/ctl.asm', 'comp/comp.asm',
        'comp/expr.asm', 'comp/link.asm', 'comp/list.asm', 'mem/far.asm', 'mem/alloc.asm', 'mem/tab.asm',
        'mem/prog.asm', 'mem/sym.asm', 'mem/scope.asm', 'mem/str.asm', 'mem/arr.asm',
        'io/io.asm', 'io/errmsg.asm', 'io/diag.asm', 'io/srd.asm', 'io/repl.asm', 'io/chan.asm', 'io/fio.asm', 'vm/using.asm', 'io/plat.asm',
        'float/flt.asm', 'float/ftr.asm', 'float/fpu.asm', 'float/dbl.asm', 'vm/dbg.asm', 'vm/event.asm', 'vm/chain.asm',
        'regions.asm']
UI = ['ui/scr.asm', 'ui/mouse.asm', 'ui/compl.asm', 'ui/text.asm',
      'ui/edit.asm', 'ui/draw.asm', 'ui/menu.asm', 'ui/dlg.asm',
      'ui/lmap.asm', 'ui/dbg.asm', 'ui/dpane.asm', 'ui/win.asm', 'ui/view.asm', 'ui/theme.asm',
      'ui/syntax.asm', 'ui/ui.asm']
HEADERS = ['x86f.inc', 'def.inc', 'state.inc', 'vm/isa.inc', 'vm/vm.inc', 'comp/comp.inc',
           'comp/dfa.inc', 'ui/uibuf.inc', 'ui/ui.inc']
TYPES = {'db': 'byte', 'dw': 'word', 'dd': 'dword', 'df': 'fword',
         'dq': 'qword', 'dt': 'tbyte'}
ID = r'[A-Za-z_@$?][\w@$?]*'


def code(text):
    """The lines without comments or strings."""
    out = []
    for line in text.split('\n'):
        q, s = None, []
        for c in line:
            if q:
                if c == q:
                    q = None
                continue
            if c in '\'"':
                q = c
                s.append(' ')
                continue
            if c == ';':
                break
            s.append(c)
        out.append(''.join(s))
    return out


def macro_labels(lines):
    """The labels macros make, as 'bc_&nm:' in a macro, for each call."""
    makes, out, cur = {}, [], None
    for l in lines:
        m = re.match(r'^(%s)\s+macro\b(.*)' % ID, l, re.I)
        if m:
            cur = (m.group(1), [a.strip() for a in m.group(2).split(',') if a.strip()])
            makes[cur[0]] = (cur[1], [])
            continue
        if cur and re.match(r'^\s+endm\b', l, re.I):
            cur = None
            continue
        if cur:
            m = re.match(r'^([\w&]+)::?', l)
            if m and '&' in m.group(1):
                makes[cur[0]][1].append(m.group(1))
            continue
        m = re.match(r'^\s+(%s)\s+(.*)' % ID, l)
        if m and m.group(1) in makes and makes[m.group(1)][1]:
            params, labels = makes[m.group(1)]
            args = [a.strip() for a in m.group(2).split(',')]
            for lab in labels:
                for pn, av in zip(params, args):
                    lab = lab.replace('&' + pn, av)
                out.append(lab + ':')
    return out


def defs(lines):
    """Labels: name -> type ('near' or a data type); and constants, macros."""
    lines = lines + macro_labels(lines)
    lab, const, mac = {}, set(), set()
    seg = 'code'
    for l in lines:
        s = l.strip().lower()
        if s in ('.code',):
            seg = 'code'
        elif s.startswith(('.data', '.const')):
            seg = 'data'
        m = re.match(r'^(%s)::?' % ID, l)
        if m:
            lab[m.group(1)] = 'near'
            continue
        m = re.match(r'^(%s)\s+(db|dw|dd|df|dq|dt)\b' % ID, l, re.I)
        if m:
            lab[m.group(1)] = TYPES[m.group(2).lower()]
            continue
        m = re.match(r'^(%s)\s+label\s+(\w+)' % ID, l, re.I)
        if m:
            lab[m.group(1)] = m.group(2).lower()
            continue
        m = re.match(r'^(%s)\s+(\w+)\s*<' % ID, l)
        if m and m.group(2) not in ('equ', 'textequ'):
            lab[m.group(1)] = m.group(2)       # a structure
            continue
        m = re.match(r'^(%s)\s*(=|equ\b|textequ\b)' % ID, l, re.I)
        if m:
            const.add(m.group(1))
            continue
        m = re.match(r'^(%s)\s+(macro|struc|struct|record)\b' % ID, l, re.I)
        if m:
            mac.add(m.group(1))
    return lab, const, mac


def uses(lines):
    return set(re.findall(ID, '\n'.join(lines)))


def source(f):
    """A module's text, its own includes (not the header) inlined."""
    out = []
    for l in (ROOT / f).read_text().split('\n'):
        m = re.match(r'^include\s+(\S+)', l, re.I)
        if m and m.group(1) not in ('x86f.inc',) and (ROOT / m.group(1)).exists():
            out.append(source(m.group(1)))
        else:
            out.append(l)
    return '\n'.join(out)


def scan(files):
    info = {}
    for f in files:
        if (ROOT / f).exists():
            ls = code(source(f))
            info[f] = (defs(ls), uses(ls))
    return info


def externs(files):
    info = scan(files)
    hdr = set()
    for h in HEADERS:
        if (ROOT / h).exists():
            hdr |= uses(code((ROOT / h).read_text()))
    info = {f: (d, us | hdr) for f, (d, us) in info.items()}
    owner = {}
    for f, ((lab, _, _), _) in info.items():
        for n, t in lab.items():
            if not n.startswith('@@'):
                owner.setdefault(n, (f, t))
    shared = {}
    for f, (_, us) in info.items():
        for n in us:
            if n in owner and owner[n][0] != f:
                shared[n] = owner[n]
    return shared, info


def emit(files, title):
    shared, _ = externs(files)
    by = {}
    for n, (f, t) in shared.items():
        by.setdefault(f, []).append((n, t))
    out = ['; generated by tools/x86syms.py: %s' % title]
    for f in files:
        if f in by:
            out.append('; %s' % f)
            for n, t in sorted(by[f]):
                out.append('externdef %s:%s' % (n, t))
    return '\n'.join(out) + '\n'


def unshared(files):
    """Constants and macros a module defines but another uses: they belong
    in a header."""
    info = scan(files)
    hdr = set()
    for (_, c, m), _ in scan([h for h in HEADERS if (ROOT / h).exists()]).values():
        hdr |= c | m
    bad = []
    for f, ((_, const, mac), _) in info.items():
        for n in (const | mac) - hdr:
            for g, (_, us) in info.items():
                if g != f and n in us:
                    bad.append('%s: %s used in %s' % (f, n, g))
                    break
    return bad


def falls(files):
    """Modules whose code runs off their end into the next one's."""
    bad = []
    for f in files:
        p = ROOT / f
        if not p.exists():
            continue
        last = None
        seg = 'code'
        for l in code(p.read_text()):
            s = l.strip()
            if not s:
                continue
            if s.lower() == '.code':
                seg = 'code'
            elif s.lower().startswith('.data'):
                seg = 'data'
            elif seg == 'code' and not s.lower().startswith(('end', 'include', '.')) \
                    and not re.match(r'\w+\s*(=|equ\b)', s, re.I):
                last = s
        if last and not re.match(r'(jmp|ret|iret|retf|dispatch)\b', last, re.I):
            bad.append('%s ends with: %s' % (f, last))
    return bad


TARGETS = [('syms.inc', CORE, 'the core'),
           ('ui/syms.inc', CORE[:-1] + UI + CORE[-1:], 'the core and the editor')]


def clashes(files):
    """@-labels two modules both define: jwasm makes them public, the link
    keeps one."""
    seen, bad = {}, []
    for f in files:
        for l in code((ROOT / f).read_text()):
            m = re.match(r'^(@\w+)::?', l)
            if m and m.group(1) not in ('@@',):
                lab = m.group(1)
                if lab in seen and seen[lab] != f:
                    bad.append('%s in %s and %s' % (lab, seen[lab], f))
                seen.setdefault(lab, f)
    return bad


def main():
    if len(sys.argv) > 1 and sys.argv[1] == 'check':
        bad = []
        for out, files, title in TARGETS:
            if (ROOT / out).exists() and (ROOT / out).read_text() != emit(files, title):
                bad.append('%s differs from tools/x86syms.py' % out)
        bad += unshared(CORE) + unshared(CORE + UI) + clashes(CORE + UI)
        for b in sorted(set(bad)):
            print(b)
        sys.exit(1 if bad else 0)
    if len(sys.argv) > 1 and sys.argv[1] == 'report':
        for b in sorted(set(unshared(CORE + UI))) + falls(CORE + UI):
            print(b)
        return
    for out, files, title in TARGETS:
        (ROOT / out).write_text(emit(files, title))


if __name__ == '__main__':
    main()
