"""The architecture diagrams: python3 tools/diagrams/make.py docs/img"""
import sys
from svg import Svg, THEME, LINE, MUTED

OUT = sys.argv[1]
W = 1000


def legend(s, y):
    s.arrow([(32, y), (72, y)])
    s.text(80, y + 4, "calls (depends on)", size=12, fill=MUTED)
    s.arrow([(232, y), (272, y)], color=THEME["front"][1], dash=True)
    s.text(280, y + 4, "reports up through a hook struct", size=12, fill=MUTED)


def layers():
    s = Svg(W, 840, "Layers", "Dependencies point down only. Upward calls go through hook structs the front end installs.")
    L, R = 32, W - 32
    full = R - L

    s.panel(L, 90, full, 64, "front", "app", "wiring only: picks a FrontEnd")
    s.chips(L + 120, 108, ["basic32", "ide32"], "front")

    s.panel(L, 180, full, 84, "front", "ui", "IDE only")
    s.chips(L + 16, 220, ["widget tree", "editor", "windows", "menus", "dialogs", "debug pane", "themes"], "front")

    s.panel(L, 290, full, 84, "engine", "session · io/repl · io/dialect", "every module's state · the READY loop · the dialects")
    s.chips(L + 16, 330, ["repl_call", "repl_run", "session_step", "session_frames", "session_eval",
                          "Dialect", "dial_program", "dial_line"], "engine")

    third = (full - 32) / 3
    s.panel(L, 400, third, 152, "engine", "comp", "QBasic, a line at a time")
    s.chips(L + 16, 440, ["lexer", "automaton", "sema", "fold", "codegen", "fuse", "link", "export"], "engine",
            maxw=third - 32)
    s.panel(L + third + 16, 400, third, 152, "engine", "qr", "Quickr, a file at a time")
    s.chips(L + third + 32, 440, ["qlex", "qpar", "qsem*", "qcg*", "qlink", "qrt"], "engine", maxw=third - 32)
    s.panel(L + 2 * third + 32, 400, third, 152, "engine", "vm · io")
    s.chips(L + 2 * third + 48, 440, ["exec_run", "ops.def", "frames", "events", "channels", "files"], "engine",
            maxw=third - 32)

    s.panel(L, 578, full, 64, "base", "mem · num · util", "no malloc")
    s.chips(L + 210, 596, ["arena · Addr", "strings, arrays", "SINGLE, DOUBLE", "bump arenas"], "base")

    s.panel(L, 668, full, 120, "base", "plat", "plat/*.h interfaces")
    s.chips(L + 16, 706, ["pscr", "pkey", "pmou", "pclk", "pfil", "psys", "pmem", "pfpu"], "base")
    s.box(L + 16, 744, 200, 32, "base", r=6, dash=True)
    s.text(L + 116, 765, "dos32/  the product", size=12, anchor="middle", mono=True)
    s.box(L + 232, 744, 220, 32, "base", r=6, dash=True)
    s.text(L + 342, 765, "host/  tests, natively", size=12, anchor="middle", mono=True)

    cx = W / 2
    for y0, y1 in [(154, 180), (264, 290), (642, 668)]:
        s.arrow([(cx, y0), (cx, y1 - 2)])
    qx, vx = L + third + 16, L + 2 * third + 32
    s.arrow([(L + third / 2, 374), (L + third / 2, 398)])
    s.arrow([(vx + third / 2, 374), (vx + third / 2, 398)])
    for x in (L + third / 2, qx + third / 2, vx + third / 2):
        s.arrow([(x, 552), (x, 576)])
    s.arrow([(qx, 470), (L + third + 2, 470)])
    s.text(qx + 16, 538, "uses comp's code · fuse · link · export", size=12, fill=MUTED, italic=True)
    c = THEME["front"][1]
    s.arrow([(qx + third / 2, 374), (qx + third / 2, 398)], color=c, dash=True)
    s.text(qx + third / 2 + 8, 390, "its Dialect table", size=12, fill=c)
    s.arrow([(R - 60, 290), (R - 60, 266)], color=c, dash=True)
    s.text(R - 72, 284, "FrontEnd · DebugHooks", size=12, fill=c, anchor="end")
    legend(s, 818)
    s.save(f"{OUT}/layers.svg")


def ownership():
    s = Svg(W, 510, "Ownership", "One Session owns every module's state. Each function takes Session *S first.")
    groups = [
        ("comp", "engine", ["Lexr", "Comp", "Ast", "Expr", "Decl", "Ctrl", "Blok", "Link", "List", "Code"]),
        ("vm", "engine", ["Vm", "Flow", "StmtState", "Evnt", "Chai", "Usng", "Syst", "Rndm", "Dbug"]),
        ("mem", "base", ["Allo", "Arry", "Prog", "Scop", "Strg", "Symb"]),
        ("io", "base", ["Chnl", "Cons", "File", "Repl", "Srcr", "Diag", "Dial"]),
    ]
    s.panel(32, 90, 640, 372, "engine", "struct Session", "static, in main")
    y = 130
    for name, th, items in groups:
        s.text(52, y + 19, name, size=12, fill=MUTED, weight=600)
        y = s.chips(140, y, items, th, maxw=520) + 14
    s.text(52, y + 18, "hot accessors take only their part:", size=12, fill=MUTED)
    s.chips(52, y + 30, ["exec_push(&S->vm, c)", "symb_var(&S->symb, id)", "prog_line(&S->prog, l)"], "engine", maxw=600)

    s.panel(696, 90, 272, 120, "front", "Uimn uimn", "ui/ui.h")
    for i, t in enumerate(["the IDE's state: widget tree,", "windows, editor buffers, the", "debug pane; beside the Session"]):
        s.text(712, 144 + i * 22, t, size=13)

    s.panel(696, 226, 272, 236, "base", "rules")
    for i, r in enumerate(["one owner per piece of state", "the IDE's state is uimn", "headers expose calls, not fields",
                           "memory is the arena only", "core never calls up: hooks", "a dialect's state: Dial, on use"]):
        s.text(712, 280 + i * 28, "•  " + r, size=13)
    legend(s, 488)
    s.save(f"{OUT}/ownership.svg")


def debugger():
    s = Svg(W, 580, "Debugger", "The IDE asks the session; the VM stops at DBG ops and reports through DebugHooks.")
    cols = [("ui: debugui", "front", 150), ("session · repl", "engine", 500), ("vm", "engine", 850)]
    for name, th, x in cols:
        s.box(x - 110, 88, 220, 40, th, strong=True)
        s.text(x, 113, name, size=14, weight=700, fill=THEME[th][2], anchor="middle")
        s.arrow([(x, 128), (x, 470)], color="#d1d5db", head=False, dash=True, width=1.2)
    E, S, V = 150, 500, 850
    fc, ec = THEME["front"][1], THEME["engine"][1]
    rows = [
        (E, S, "session_step(S, STEP_OVER, stopped)", ec, False),
        (S, V, "dbug.mode = DM_PSTEP, pdep = depth", ec, False),
        (S, V, "repl_cont → exec_run", ec, False),
        (V, V, "DBG op at each statement", ec, False),
        (V, E, "DebugHooks.trace(S, &pos)   (TRON)", fc, True),
        (V, S, "a stop: unwind to repl.top", ec, False),
        (S, E, "repl_call returns E_BRK: udbg_stop", ec, False),
        (E, S, "session_frames · session_cell · session_eval", ec, False),
    ]
    y = 160
    for a, b, label, col, dash in rows:
        if a == b:
            s.arrow([(a, y - 6), (a + 40, y - 6), (a + 40, y + 10), (a + 4, y + 10)], color=col)
            s.text(a - 12, y + 6, label, size=12, fill=col, anchor="end", mono=True)
        else:
            s.arrow([(a, y), (b + (-4 if b > a else 4), y)], color=col, dash=dash)
            s.text((a + b) / 2, y - 8, label, size=12, fill=col, anchor="middle", mono=True)
        y += 40
    s.box(E - 110, y - 10, 220, 40, "front")
    s.text(E, y + 15, "draw pane, light the statement", size=12, anchor="middle")
    legend(s, 556)
    s.save(f"{OUT}/debugger.svg")


def widgets():
    s = Svg(W, 530, "UI: the widget tree", "Keys go down the focus path and bubble up; a parent draws, then its children.")
    fc = THEME["front"]

    def node(x, y, label, modal=False, w=150):
        s.box(x - w / 2, y, w, 34, "front", r=8, dash=modal)
        s.text(x, y + 22, label, size=12, anchor="middle", mono=True,
               fill=fc[2])
        return (x, y)

    def edge(p, c):
        s.arrow([(p[0], p[1] + 34), (p[0], p[1] + 50), (c[0], p[1] + 50), (c[0], c[1] - 2)], color=LINE, head=False)

    sc = node(500, 96, "screen")
    kids = [node(100, 180, "menubar", w=130), node(380, 180, "workspace"), node(620, 180, "statusbar"),
            node(826, 180, "dialog", True), node(100, 264, "menu", True, 120)]
    for k in kids[:4]:
        edge(sc, k)
    s.arrow([(500, 130), (500, 146), (30, 146), (30, 281), (38, 281)], color=LINE, head=False)
    ws = [node(250, 264, "edit 1", w=96), node(350, 264, "edit 2", w=96), node(450, 264, "immediate", w=96),
          node(560, 264, "debug pane", w=110)]
    for k in ws[1:]:
        edge(kids[1], k)
    s.out.append(f'<path d="M380,214 L380,230 L250,230 L250,262" fill="none" stroke="{LINE}" stroke-width="1.6"/>')
    for i, l in enumerate(["watch", "locals", "call stack"]):
        edge(ws[3], node(470 + i * 90, 348, l, w=84))
    for i, l in enumerate(["field", "list", "checkbox", "button"]):
        edge(kids[3], node(712 + i * 76, 264, l, w=70))

    s.box(32, 410, 936, 70, "base")
    for i, (h, t) in enumerate([("keys", "deepest focused widget, then up to screen"),
                                ("mouse", "deepest visible widget under the point"),
                                ("draw", "parent, then its visible children"),
                                ("cursor", "the focused widget places it")]):
        x = 48 + (i % 2) * 468
        y = 436 + (i // 2) * 26
        s.text(x, y, h, size=12, weight=700)
        s.text(x + 56, y, t, size=12, fill=MUTED)
    s.box(32, 492, 28, 16, "front", r=4, dash=True)
    s.text(68, 505, "modal: takes every event while open", size=12, fill=MUTED)
    s.save(f"{OUT}/widgets.svg")


def pipeline():
    s = Svg(W, 300, "A line at a time", "Each edit checks one line; before RUN, link compiles every line.")
    steps = ["line text", "lexer", "automaton", "fold", "codegen"]
    x = 32
    pos = []
    for i, t in enumerate(steps):
        w = 104 if i else 110
        s.box(x, 110, w, 44, "base" if not i else "engine", r=8)
        s.text(x + w / 2, 137, t, size=13, anchor="middle", mono=True)
        pos.append((x, w))
        x += w + 30
    for (x0, w0), (x1, _) in zip(pos, pos[1:]):
        s.arrow([(x0 + w0, 132), (x1 - 2, 132)])
    s.box(x, 100, 170, 64, "engine", strong=True)
    s.text(x + 85, 128, "stored Line", size=14, weight=700, anchor="middle", fill=THEME["engine"][2])
    s.text(x + 85, 148, "tokens; code checked", size=12, anchor="middle", fill=MUTED)
    s.arrow([(pos[-1][0] + pos[-1][1], 132), (x - 2, 132)])
    ln = x + 85
    s.box(ln - 230, 216, 150, 44, "engine", r=8)
    s.text(ln - 155, 243, "listing → editor", size=13, anchor="middle", mono=True)
    s.box(ln - 40, 216, 180, 44, "engine", r=8)
    s.text(ln + 50, 243, "link → code cache", size=13, anchor="middle", mono=True)
    s.arrow([(ln - 20, 164), (ln - 155, 214)])
    s.arrow([(ln + 20, 164), (ln + 50, 214)])
    s.text(pos[2][0] + 60, 186, "an edit: tree and code checked, then dropped", size=12, anchor="middle",
           fill=MUTED, italic=True)
    s.save(f"{OUT}/pipeline.svg")


def vm():
    s = Svg(W, 600, "The VM", "A byte-coded stack machine: one loop calls each op's handler; values live on a stack of 8-byte cells.")
    ec, fc = THEME["engine"], THEME["front"]
    # code cache
    s.panel(32, 90, 300, 400, "engine", "code cache", "grows to fit")
    s.text(48, 134, "x% = 2 * y% + 1", size=12, mono=True, fill=MUTED)
    ops = [("04 02", "I_LD_U8 2"), ("32 01 00 00 00", "I_MUL_V y%"), ("3a 01", "I_ADD_U8 1"),
           ("19 02 00 00 00", "I_ST_V x%"), ("00", "EOL")]
    y = 150
    for i, (b, n) in enumerate(ops):
        cur = i == 1
        s.box(48, y, 268, 36, "front" if cur else "base", r=6, strong=cur)
        s.text(60, y + 23, b, size=12, mono=True)
        s.text(180, y + 23, n, size=12, mono=True, fill=ec[2])
        y += 44
    s.arrow([(352, 204), (320, 204)], color=fc[1], label="opip: the op running", lx=358, ly=208, lanchor="start")
    s.arrow([(352, 222), (320, 222)], color=fc[1], label="ip: its operands, then the next op", lx=358, ly=226, lanchor="start")
    s.text(48, 392, "every line's code, compiled by", size=12, fill=MUTED)
    s.text(48, 410, "link_start before the run", size=12, fill=MUTED)
    s.text(48, 436, "before it: the statement map", size=12, fill=MUTED)
    s.text(48, 454, "(body offset → code offset)", size=12, fill=MUTED)

    # loop
    s.panel(370, 250, 280, 170, "front", "exec_run")
    for i, t in enumerate(["for (;;) {", "    vm.opip = vm.ip;", "    ops[*vm.ip++](S);", "}"]):
        ind = len(t) - len(t.lstrip())
        s.text(390 + ind * 8, 294 + i * 20, t.strip(), size=13, mono=True)
    s.text(390, 392, "256 ops, 68 X ops (ops.def); CALL k:", size=12, fill=MUTED)
    s.text(390, 410, "200 routines (PRINT, INPUT, MID$ ...)", size=12, fill=MUTED)

    # data stack
    s.panel(690, 90, 278, 260, "engine", "data stack", "a push moves sp down")
    cells = ["", "", "", "", "8"]
    for i in range(5):
        y = 132 + i * 36
        top = i == 4
        s.box(740, y, 140, 32, "front" if top else "base", r=4, strong=top)
        if cells[i]:
            s.text(810, y + 21, cells[i], size=13, mono=True, anchor="middle")
    s.arrow([(910, 292), (884, 292)], color=fc[1], label="sp", lx=916, ly=296, lanchor="start")
    s.arrow([(910, 314), (884, 312)], color=MUTED, label="rsp", lx=916, ly=322, lanchor="start")
    s.text(706, 338, "after I_MUL_V; a Cell: INT, SINGLE, Addr, DOUBLE", size=11, fill=MUTED)

    # frames / callvars
    s.panel(690, 370, 278, 120, "base", "control")
    s.chips(706, 408, ["GOSUB", "CALL", "EVENT"], "base")
    s.text(706, 466, "vm.frames; a call's arguments and", size=12, fill=MUTED)
    s.text(706, 482, "locals: its frame on the data stack", size=12, fill=MUTED)
    s.arrow([(650, 320), (688, 230)], color=LINE)
    s.arrow([(650, 360), (688, 420)], color=LINE)

    s.box(32, 512, 936, 56, "warn")
    s.text(48, 536, "leaving the loop", size=13, weight=700, fill=THEME["warn"][2])
    s.text(48, 555, "only by longjmp: an error (repl_error → ON ERROR's handler, or READY), END, STOP, a step.",
           size=12, fill=THEME["warn"][2])
    s.save(f"{OUT}/vm.svg")


def vm_trace():
    s = Svg(W, 330, "One line, op by op", "x% = 2 * y% + 1 with y% = 4: each op takes its operands off the stack and pushes its result.")
    rows = [("I_LD_U8 2", "push 2", ["2"]), ("I_MUL_V y%", "pop, × y%, push", ["8"]),
            ("I_ADD_U8 1", "pop, + 1, push", ["9"]), ("I_ST_V x%", "pop into x%", []),
            ("EOL", "stop check, next line", [])]
    for i, (op, what, st) in enumerate(rows):
        y = 96 + i * 44
        s.box(32, y, 180, 34, "engine", r=6)
        s.text(44, y + 22, op, size=12, mono=True, fill=THEME["engine"][2])
        s.text(232, y + 22, what, size=12, fill=MUTED)
        s.arrow([(420, y + 17), (470, y + 17)], color=LINE)
        if st:
            for j, c in enumerate(st):
                s.box(480 + j * 60, y + 2, 52, 30, "front", r=4)
                s.text(506 + j * 60, y + 22, c, size=13, mono=True, anchor="middle")
        else:
            s.text(480, y + 22, "empty: sp = rsp", size=12, fill=MUTED, italic=True)
    s.text(640, 118, "Operands folded into the op:", size=12, weight=600)
    s.text(640, 140, "_V a variable, _U8 a byte literal.", size=12, fill=MUTED)
    s.text(640, 162, "2 * y% + 1 is 4 ops, not 6.", size=12, fill=MUTED)
    s.text(640, 196, "Constants fold as they compile:", size=12, weight=600)
    s.text(640, 218, "2 * 3 + 4 * 5 is one I_LD_U8 26.", size=12, fill=MUTED)
    s.save(f"{OUT}/vm-trace.svg")


def dialects():
    s = Svg(W, 600, "Dialects", "The core asks a Dialect table; each front end compiles to the same VM.")
    fc, ec = THEME["front"], THEME["engine"]

    s.panel(32, 90, 936, 118, "engine", "the core", "names no dialect but QBasic")
    s.chips(48, 130, ["io/repl: open · run · report", "comp/suggest: suggest", "ui: colour · names · fn_at · syntax",
                      "debugger: spell · print"], "engine", maxw=904)

    s.panel(32, 240, 936, 80, "front", "io/dialect.h: struct Dialect", "io/dialect.c: DIALECTS(X)")
    s.chips(48, 278, ["dial_of_file", "dial_program", "dial_line", "dial_state"], "front")
    s.text(560, 296, "a file, the program, a line: by its first line's mark", size=12, fill=MUTED)

    s.panel(32, 352, 452, 128, "engine", "dialect_qbasic", "io/repl.c")
    for i, t in enumerate(["the core's own: lines stored as tokens,", "checked and compiled a line at a time",
                           "(comp: automaton, sema, fold, codegen)"]):
        s.text(48, 404 + i * 22, t, size=12)
    s.panel(516, 352, 452, 128, "engine", "dialect_quickr", "qr/qr.c")
    for i, t in enumerate(["a text dialect: lines stored as typed (LF_TEXT),", "checked a file at a time",
                           "(qr: qlex, qpar, qsem*, qcg*, qlink)"]):
        s.text(532, 404 + i * 22, t, size=12)

    s.panel(32, 512, 936, 58, "base", "shared back end")
    s.chips(220, 527, ["comp/code · fuse · link", "comp/export: interop", "vm: ops.def", "mem · runtime"], "base")

    for x in (258, 742):
        s.arrow([(x, 208), (x, 238)], color=fc[1], dash=True)
        s.arrow([(x, 320), (x, 350)], color=fc[1], dash=True)
        s.arrow([(x, 480), (x, 510)])
    s.arrow([(516, 450), (486, 450)], color=ec[1])
    s.arrow([(486, 466), (514, 466)], color=ec[1])
    s.text(500, 500, "imports, both ways: comp/export", size=12, fill=ec[1], anchor="middle")
    legend(s, 588)
    s.save(f"{OUT}/dialects.svg")


def bar(s, x, y, per, fields, theme="base", h=40):
    """A byte layout: each field (bytes, label) a box per px a byte wide, its offset above it."""
    at = 0
    for n, label in fields:
        w = n * per
        s.box(x, y, w, h, theme, r=3)
        s.text(x + w / 2, y + h / 2 + 4, label, size=11, anchor="middle", mono=True)
        s.text(x + 2, y - 6, str(at), size=10, fill=MUTED, mono=True)
        x += w
        at += n
    s.text(x + 2, y - 6, str(at), size=10, fill=MUTED, mono=True)


def qbx_file():
    s = Svg(W, 640, "A .qbx file, version 2", "Little-endian. The slices start on 16-byte boundaries; each interface ends with 1Ah, as a source file does.")
    fc, ec, wc = THEME["front"], THEME["engine"], THEME["warn"]

    parts = [("header", "16 bytes", "front", 40), ("entries", "24 bytes each: slices, then parts", "front", 40),
             ("slice dos-i386", "an i386 DLL", "engine", 34), ("slice macos", "a universal dylib", "engine", 34),
             ("slice linux-x86_64", "an ELF .so", "engine", 34), ("slice wasm32", "a side module", "engine", 34),
             ('part ""', "the module's own interface", "warn", 34), ('part "display"', "a package's module", "warn", 34),
             ('part "draw"', "and so on, each + 1Ah", "warn", 34)]
    y, at = 96, {}
    for name, note, theme, h in parts:
        s.box(32, y, 310, h, theme, r=6)
        s.text(48, y + h / 2 + 5, name, size=13, weight=700, fill=THEME[theme][2])
        s.text(326, y + h / 2 + 5, note, size=11, fill=THEME[theme][1], anchor="end")
        at[name] = (y, h)
        y += h + 6
    s.text(48, y + 14, 'a module\'s .qbx has part "" alone; a package\'s, its modules', size=11, fill=MUTED)

    s.text(440, 104, "header", size=14, weight=700, fill=fc[2])
    bar(s, 440, 126, 33, [(4, 'QBX 1Ah'), (2, "format"), (2, "abi"), (2, "minor"), (2, "slices"),
                          (2, "parts"), (2, "0")], "front")
    s.text(440, 188, "16 bits each: format 2; abi, the ABI's major; minor, the least the host needs;", size=11, fill=MUTED)
    s.text(440, 204, "the slices' and the parts' counts. Version 1 had no parts: one interface, last.", size=11, fill=MUTED)

    s.text(440, 242, "entry: a slice's or a part's", size=14, weight=700, fill=fc[2])
    bar(s, 440, 264, 22, [(16, "tag or name, 0-padded"), (4, "at"), (4, "bytes")], "front")
    s.text(440, 326, "a slice's tag: this build's pext_tag is loaded, the rest skipped;", size=11, fill=MUTED)
    s.text(440, 342, 'a part\'s name: "" the module\'s own, else gfx.qbx/draw is "draw"', size=11, fill=MUTED)

    s.text(440, 380, "dos-i386 slice: the DLL as lld-link wrote it (util/image.c)", size=14, weight=700, fill=ec[2])
    x = 440
    for label, w in (("MZ stub", 96), ("PE headers", 132), ("its sections", 184), (".reloc", 104)):
        s.box(x, 402, w, 40, "engine", r=3)
        s.text(x + w / 2, 427, label, size=11, anchor="middle", mono=True)
        x += w + 4
    s.text(440, 476, "placed: each section's bytes at its offset in a zeroed block, the block's", size=11, fill=MUTED)
    s.text(440, 492, "address less the DLL's base added to each HIGHLOW word, qx_init_<name>,", size=11, fill=MUTED)
    s.text(440, 508, "an export, called", size=11, fill=MUTED)

    hy, hh = at["header"]
    s.arrow([(342, hy + hh / 2), (436, 146)], color=fc[1])
    ty, th = at["entries"]
    s.arrow([(342, ty + th / 2), (436, 284)], color=fc[1])
    dy, dh = at["slice dos-i386"]
    s.arrow([(342, dy + dh / 2), (436, 422)], color=ec[1])

    s.box(32, 568, 936, 48, "base")
    s.text(48, 597, "io/qbxfile.c reads it: every part must lie inside the file. tools/qbx.py pack writes it; "
           "qbx.py strip keeps some slices.", size=12)
    s.save(f"{OUT}/qbx-file.svg")


def qbx_build():
    s = Svg(W, 470, "Building a .qbx", "tools/qbx.py writes the glue from the interfaces, each slice is built with it, then pack puts them in one file.")
    fc, ec, wc = THEME["front"], THEME["engine"], THEME["warn"]

    s.panel(32, 96, 220, 96, "warn", "synth.bas", "an interface")
    s.text(48, 146, "TYPEs, CONSTs, DECLARE", size=12, mono=True)
    s.text(48, 166, "... CDECL; a package's: one a part", size=11)
    s.panel(32, 222, 220, 76, "base", "synth.c", "plain C")
    s.text(48, 264, "a function a DECLARE,", size=12)
    s.text(48, 282, "synth_note; gfx_draw_rect", size=12, mono=True)

    s.panel(292, 96, 200, 96, "front", "qbx.py glue")
    s.text(308, 146, "synth_qx.h: CONSTs, TYPEs", size=12, mono=True)
    s.text(308, 166, "synth_qx.c: the entries", size=12, mono=True)
    s.arrow([(252, 144), (290, 144)], color=wc[1])

    rows = [("dos-i386", "clang i386-pc-win32 · lld-link /dll"), ("macos", "clang -arch arm64 -arch x86_64"),
            ("linux-x86_64", "clang x86_64-linux-gnu · ld.lld -shared"), ("wasm32", "clang wasm32 · wasm-ld -shared")]
    for i, (tag, how) in enumerate(rows):
        y = 96 + i * 52
        s.box(532, y, 258, 44, "engine", r=6)
        s.text(546, y + 19, tag, size=12, weight=700, fill=ec[2])
        s.text(546, y + 35, how, size=10, fill=ec[1], mono=True)
        s.arrow([(492, 144), (512, 144), (512, y + 22), (530, y + 22)], color=fc[1])
        s.arrow([(790, y + 22), (822, 222)], color=ec[1])
    s.arrow([(252, 260), (512, 260)], color=LINE)

    s.panel(824, 172, 144, 100, "front", "qbx.py pack")
    s.text(840, 222, "a DOS DLL", size=11)
    s.text(840, 238, "checked;", size=11)
    s.text(840, 254, "the parts after", size=11)
    s.arrow([(252, 120), (272, 120), (272, 84), (896, 84), (896, 170)], color=wc[1], dash=True)
    s.box(824, 300, 144, 44, "warn", strong=True)
    s.text(896, 327, "synth.qbx", size=14, weight=700, fill=wc[2], anchor="middle", mono=True)
    s.arrow([(896, 272), (896, 298)], color=fc[1])

    s.box(32, 384, 936, 60, "base")
    s.text(48, 408, "One Mac builds all four with clang and lld; make builds each module for every target. "
           "A slice imports nothing:", size=12)
    s.text(48, 428, "what it needs of the host comes through the table qx_init_<name> is given.", size=12)
    s.save(f"{OUT}/qbx-build.svg")


def qbx_load():
    s = Svg(W, 600, "Loading a .qbx", "Found and opened at the IMPORT, its interface read as the module, its slice loaded when the program links.")
    fc, ec, wc = THEME["front"], THEME["engine"], THEME["warn"]
    steps = [
        ("front", "1  find", "io/locate.c", ["synth: synth.bas, else synth.qbx; gfx.draw: else the part gfx.qbx/draw.",
                                              "In the program's folder, BASICLIB, then lib/; the same for Quickr"]),
        ("front", "2  open", "io/native.c natv_open", ["the file read whole once a process, kept by its path; qbxf_read checks it.",
                                                        "Another ABI major or a newer minor: Advanced feature unavailable"]),
        ("warn", "3  read", "io/repl.c read_module", ["the part's interface is the module's lines, stored LF_NATIVE;",
                                                       "each DECLARE ... CDECL a Proc with PF_NATIVE and no body (comp/blocks.c)"]),
        ("engine", "4  link", "comp/link.c link_natives", ["natv_module: this build's slice to pext_load (plat/pext.h), then",
                                                            "qx_init_<file>(&host) gives the QxModule; each DECLARE's entry found (draw.rect)"]),
        ("base", "5  run", "vm/ncall.c", ["start() at the run's start; a call is CALL NCALL: its arguments laid out",
                                           "as qx/qx.h says, the entry called, its result pushed; stop() at the end"]),
    ]
    y = 92
    for theme, name, where, lines in steps:
        s.panel(32, y, 640, 88, theme, name, where)
        for i, t in enumerate(lines):
            s.text(48, y + 52 + i * 18, t, size=12)
        if y > 92:
            s.arrow([(352, y - 12), (352, y - 2)], color=LINE)
        y += 100

    s.panel(704, 392, 264, 188, "engine", "pext_load", "once a process")
    loaders = [("DOS", "a heap block, relocated"), ("Mac", "cached dylib, dlopen"),
               ("Linux", "memfd_create, dlopen"), ("wasm", "side module instantiated")]
    for i, (k, v) in enumerate(loaders):
        s.chip(720, 430 + i * 36, k, "engine", w=60)
        s.text(790, 449 + i * 36, v, size=12)
    s.arrow([(672, 436), (702, 436)], color=ec[1])

    s.panel(704, 92, 264, 140, "base", "the file, kept")
    for i, t in enumerate(["NatvFile: path, bytes, its QbxFile", "(abi, minor, this build's slice);",
                           "module: the QxModule, once", "CHAIN and RUN find it again"]):
        s.text(720, 140 + i * 20, t, size=12)
    s.arrow([(672, 236), (702, 200)], color=LINE, dash=True)
    s.save(f"{OUT}/qbx-load.svg")


layers(); ownership(); dialects(); debugger(); widgets(); pipeline(); vm(); vm_trace(); qbx_file(); qbx_build(); qbx_load()
