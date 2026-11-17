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
    s = Svg(W, 800, "Layers", "Dependencies point down only. Upward calls go through hook structs the front end installs.")
    L, R = 32, W - 32
    full = R - L

    s.panel(L, 90, full, 64, "front", "app", "wiring only: picks a FrontEnd")
    s.chips(L + 120, 108, ["basic32", "ide32"], "front")

    s.panel(L, 180, full, 84, "front", "ui", "IDE only")
    s.chips(L + 16, 220, ["widget tree", "editor", "windows", "menus", "dialogs", "debug pane", "themes"], "front")

    s.panel(L, 290, full, 84, "engine", "session · io/repl", "every module's state · the READY loop")
    s.chips(L + 16, 330, ["repl_call", "repl_run", "session_step", "session_until", "session_set_next",
                          "session_frames", "session_eval"], "engine")

    half = (full - 16) / 2
    s.panel(L, 400, half, 112, "engine", "comp", "a line at a time")
    s.chips(L + 16, 440, ["lexer", "automaton", "sema", "fold", "codegen", "fuse", "link", "listing"], "engine", maxw=half - 32)
    s.panel(L + half + 16, 400, half, 112, "engine", "vm · io")
    s.chips(L + half + 32, 440, ["exec_run", "ops.def", "frames", "events", "channels", "files"], "engine", maxw=half - 32)

    s.panel(L, 538, full, 64, "base", "mem · num · util", "no malloc")
    s.chips(L + 210, 556, ["arena · Addr", "strings, arrays", "SINGLE, DOUBLE", "bump arenas"], "base")

    s.panel(L, 628, full, 120, "base", "plat", "plat/*.h interfaces")
    s.chips(L + 16, 666, ["pscr", "pkey", "pmou", "pclk", "pfil", "psys", "pmem", "pfpu"], "base")
    s.box(L + 16, 704, 200, 32, "base", r=6, dash=True)
    s.text(L + 116, 725, "dos32/  the product", size=12, anchor="middle", mono=True)
    s.box(L + 232, 704, 220, 32, "base", r=6, dash=True)
    s.text(L + 342, 725, "host/  tests, natively", size=12, anchor="middle", mono=True)

    cx = W / 2
    for y0, y1 in [(154, 180), (264, 290), (512, 538), (602, 628)]:
        s.arrow([(cx, y0), (cx, y1 - 2)])
    s.arrow([(cx - 120, 374), (cx - 120, 398)])
    s.arrow([(cx + 120, 374), (cx + 120, 398)])
    s.arrow([(L + half * 0.75, 512), (L + half * 0.75, 536)])
    c = THEME["front"][1]
    s.arrow([(R - 60, 290), (R - 60, 266)], color=c, dash=True)
    s.text(R - 72, 284, "FrontEnd · DebugHooks", size=12, fill=c, anchor="end")
    legend(s, 778)
    s.save(f"{OUT}/layers.svg")


def ownership():
    s = Svg(W, 510, "Ownership", "One Session owns every module's state. Each function takes Session *S first.")
    groups = [
        ("comp", "engine", ["Lexr", "Comp", "Ast", "Expr", "Decl", "Ctrl", "Blok", "Link", "List", "Code"]),
        ("vm", "engine", ["Vm", "Flow", "StmtState", "Evnt", "Chai", "Usng", "Syst", "Rndm", "Dbug"]),
        ("mem", "base", ["Allo", "Arry", "Prog", "Scop", "Strg", "Symb"]),
        ("io", "base", ["Chnl", "Cons", "File", "Repl", "Srcr", "Diag"]),
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
                           "memory is the arena only", "core never calls up: hooks"]):
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
    s.text(390, 392, "244 ops (ops.def); CALL k: 185", size=12, fill=MUTED)
    s.text(390, 410, "routines (PRINT, INPUT, MID$ ...)", size=12, fill=MUTED)

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


layers(); ownership(); debugger(); widgets(); pipeline(); vm(); vm_trace()
