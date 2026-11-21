"""The engine design's diagrams: python3 tools/diagrams/engine.py docs/img"""

import sys

from svg import LINE, MUTED, THEME, Svg

OUT = sys.argv[1]
W = 1000
UP = THEME["front"][1]


def legend(s: Svg, y: int) -> None:
    s.arrow([(32, y), (72, y)])
    s.text(80, y + 4, "imports, calls", size=12, fill=MUTED)
    s.arrow([(202, y), (242, y)], color=UP, dash=True)
    s.text(250, y + 4, "reports through the event queue", size=12, fill=MUTED)


def layers() -> None:
    s = Svg(
        W, 790, "Layers", "Imports point down only. Play reports up through the event queue and calls nothing above it."
    )
    left, right = 32, W - 32
    full = right - left
    game = 716
    half = (game - 16) / 2
    other = left + game + 16

    s.panel(left, 90, full, 64, "front", "demos", "wiring only: allot the World, call Main")
    s.chips(left + 120, 108, ["CHARLIE.BAS", "MOTHLITE.BAS", "RUSTLINE.BAS"], "front")

    s.panel(left, 180, game, 84, "front", "barkley · main, flow", "the only code that takes the World")
    s.chips(left + 16, 220, ["Main", "Advance", "Tick", "title", "intro", "save"], "front")
    s.chip(left + 430, 220, "Deliver", "front")

    s.panel(left, 290, half, 112, "engine", "barkley · play", "changes state, draws nothing")
    s.chips(left + 16, 330, ["hero", "things", "touch", "zones", "camera", "parts"], "engine", maxw=half - 32)
    s.panel(left + half + 16, 290, half, 112, "engine", "barkley · scene", "reads state, changes nothing")
    s.chips(left + half + 32, 330, ["backdrop", "tiles", "things", "hero", "parts", "hud"], "engine", maxw=half - 32)

    s.panel(left, 428, game, 84, "engine", "barkley · model, world, assets", "records, and what fills them")
    s.chips(left + 16, 468, ["Hero", "Game", "Level", "Ground", "layout", "pictures", "tunes"], "engine")
    s.chip(left + 548, 468, "Kinds", "engine")

    s.box(other, 180, full - game - 16, 332, "front", dash=True)
    s.text(other + 16, 206, "another demo's game", size=15, weight=700, fill=THEME["front"][2])
    for i, t in enumerate(["its own records and", "rules, over the same", "engine"]):
        s.text(other + 16, 236 + i * 20, t, size=13)

    s.panel(left, 538, full, 84, "base", "engine", "knows no game")
    s.chips(left + 16, 578, ["Clock", "screen", "Store", "Keys", "Deck", "data file"], "base")
    s.chip(left + 470, 578, "Events", "base")

    s.panel(left, 648, full, 64, "base", "BASIC", "the language's own")
    s.chips(left + 120, 666, ["SCREEN 13", "PUT, GET", "PLAY", "INP(&H60)", "TIMER", "files"], "base")

    cx = left + game / 2
    s.arrow([(cx, 154), (cx, 178)])
    s.arrow([(cx - 120, 264), (cx - 120, 288)])
    s.arrow([(cx + 120, 264), (cx + 120, 288)])
    s.arrow([(cx - 120, 402), (cx - 120, 426)])
    s.arrow([(cx + 120, 402), (cx + 120, 426)])
    s.arrow([(cx, 512), (cx, 536)])
    s.arrow([(W / 2, 622), (W / 2, 646)])
    ox = other + (full - game - 16) / 2
    s.arrow([(ox, 154), (ox, 178)])
    s.arrow([(ox, 512), (ox, 536)])
    s.arrow([(left + 40, 290), (left + 40, 266)], color=UP, dash=True)
    s.text(left + 52, 283, "Events", size=12, fill=UP)
    legend(s, 760)
    s.save(f"{OUT}/engine-layers.svg")


def rows(s: Svg, x: int, y: int, w: int, items: list[tuple[str, list[str], list[str]]], theme: str) -> int:
    """Each subsystem on its line: its record, the routines that change it, then those not written yet."""
    for name, have, new in items:
        s.text(x + 20, y + 19, name, size=12, fill=MUTED, weight=600)
        cx = x + 136
        for label in have:
            cx += s.chip(cx, y, label, theme) + 8
        for label in new:
            cx += s.chip(cx, y, label, "warn") + 8
        y += 42
    return y


def subsystems() -> None:
    s = Svg(
        W,
        572,
        "Subsystems",
        "A subsystem is one record and the routines that change it. Nothing else assigns its fields.",
    )
    s.panel(32, 90, 452, 310, "base", "engine", "shared by the demos")
    engine = [
        ("Clock", ["Pace", "Steps"], []),
        ("the screen", ["OpenScreen", "Flip"], []),
        ("Store", ["Grab", "MaskGrab", "Flop", "Blit"], []),
        ("Keys", ["Poll", "Held2"], []),
        ("Deck", ["LoadSong", "StartSong", "FeedSong"], []),
        ("Events", ["Post", "Taken"], []),
    ]
    rows(s, 32, 132, 452, engine, "base")

    s.panel(500, 90, 468, 310, "engine", "barkley", "the game")
    game = [
        ("Hero", ["MoveHero", "Hurt", "Push"], []),
        ("Things", ["AddObj", "Think", "React"], []),
        ("Kinds, read only", ["LoadKinds"], []),
        ("Level, Ground", ["LoadLevel", "LoadTables"], []),
        ("Game", ["GoStage", "StartGame", "Deliver"], []),
        ("Part, Loose", ["Emit", "Burst", "Puff", "Particles"], []),
    ]
    rows(s, 500, 132, 468, game, "engine")

    s.panel(32, 416, 936, 132, "base", "rules")
    rules = [
        "one owner per record: only its module assigns its fields",
        "a routine takes its own record first, and no more than it changes or reads",
        "subsystems do not call sideways: the orchestrator passes results, or play posts an event",
        "only the orchestrator, main and flow, takes the World",
    ]
    for i, r in enumerate(rules):
        s.text(52, 462 + i * 22, "•  " + r, size=13)
    s.save(f"{OUT}/engine-subsystems.svg")


def frame() -> None:
    s = Svg(
        W,
        560,
        "One frame",
        "Steps are 1/60 s whatever the frame rate. Play posts events; Deliver hands each to its owner.",
    )
    green, base = THEME["engine"], THEME["base"]

    def stage(x: float, y: float, label: str, note: str, theme: str = "base", w: float = 104) -> None:
        s.box(x, y, w, 52, theme, r=8)
        s.text(x + w / 2, y + 22, label, size=13, weight=700, anchor="middle", mono=True, fill=THEME[theme][2])
        s.text(x + w / 2, y + 40, note, size=11, anchor="middle", fill=MUTED)

    y = 100
    before = [("Poll", "keys"), ("Pace", "time passed"), ("ReadPad", "keys to a Pad")]
    for i, (label, note) in enumerate(before):
        stage(32 + i * 120, y, label, note)
        s.arrow([(32 + i * 120 + 104, y + 26), (32 + (i + 1) * 120 - 2, y + 26)])
    s.text(392, y + 30, "Steps", size=13, weight=700, mono=True, fill=base[2])
    s.arrow([(436, y + 26), (476, y + 26), (476, 198)], color=LINE)

    s.panel(32, 200, 936, 232, "engine", "one step", "0, 1 or more a frame")
    play = [
        ("MoveHero", "the hero"),
        ("Think", "each thing's rule"),
        ("Touch", "hero on things"),
        ("zone", "tide, chase, gale"),
    ]
    for i, (label, note) in enumerate(play):
        stage(52 + i * 128, 244, label, note, "engine", 112)
        if i:
            s.arrow([(52 + (i - 1) * 128 + 112, 270), (52 + i * 128 - 2, 270)], color=green[1])
    s.box(52, 352, 496, 52, "base", r=8)
    s.text(300, 374, "Events", size=13, weight=700, anchor="middle", mono=True, fill=base[2])
    s.text(300, 392, "sound · score · burst · shake", size=11, anchor="middle", fill=MUTED)
    for i in range(4):
        x = 52 + i * 128 + 56
        s.arrow([(x, 296), (x, 350)], color=UP, dash=True)

    stage(600, 352, "Deliver", "empties the queue", "base", 112)
    s.arrow([(548, 378), (598, 378)])
    owners = [("Deck", "a sound, a song"), ("Game", "score, shake"), ("Parts", "sparks, then their step")]
    for i, (label, note) in enumerate(owners):
        stage(760, 236 + i * 62, label, note, "engine", 188)
    s.arrow([(712, 378), (736, 378), (736, 262), (758, 262)])
    s.arrow([(736, 324), (758, 324)])
    s.arrow([(736, 378), (736, 386), (758, 386)])

    y = 468
    after = [
        ("Follow", "the camera moves"),
        ("Render", "draws the state"),
        ("Flip", "shows the page"),
        ("FeedSong", "the next half bar"),
    ]
    s.arrow([(84, 432), (84, y - 2)])
    for i, (label, note) in enumerate(after):
        stage(32 + i * 136, y, label, note, "base", 120)
        if i:
            s.arrow([(32 + (i - 1) * 136 + 120, y + 26), (32 + i * 136 - 2, y + 26)])
    legend(s, 542)
    s.save(f"{OUT}/engine-frame.svg")


def table(s: Svg, x: float, y: float, widths: list[int], head: list[str], body: list[list[str]], theme: str) -> None:
    _, stroke, ink, tint = THEME[theme]
    total = sum(widths)
    s.rect(x, y, total, 26, tint, stroke)
    for r in range(len(body)):
        s.rect(x, y + 26 * (r + 1), total, 26, "#ffffff", stroke)
    cx = x
    for c, w in enumerate(widths):
        s.text(cx + 8, y + 18, head[c], size=12, weight=700, fill=ink)
        for r, row in enumerate(body):
            s.text(cx + 8, y + 26 * (r + 1) + 18, row[c], size=12, mono=True, fill=ink)
        cx += w


def things() -> None:
    s = Svg(
        W, 560, "Things", "A kind is a row. Each pass reads its own columns, and switches on the rule, not on the kind."
    )
    s.panel(32, 90, 936, 196, "engine", "Kind", "one row a kind, read from CHARLIE.DAT")
    head = ["kind", "picture", "box", "frames", "touch", "rest", "mind", "crushed"]
    body = [
        ["spring", "OWN", "", "", "BOUNCE", "8", "REST", "no"],
        ["cat", "OWN", "", "", "OWN", "", "PROWL", "yes"],
        ["hoop", "IHOOP", "12 by 32", "2, pressed", "OWN", "20", "REST", "no"],
        ["bush", "IBUSH", "40 by 20", "1", "none", "", "none", "no"],
    ]
    table(s, 52, 130, [96, 110, 96, 130, 106, 106, 106, 146], head, body, "engine")
    s.text(52, 274, "OWN: the kind has its own routine, one case in that pass.", size=12, fill=MUTED)

    s.panel(32, 306, 340, 196, "engine", "Thing", "one row a thing")
    table(
        s,
        52,
        346,
        [72, 108, 60, 60],
        ["kind", "x, y", "state", "home"],
        [["cat", "912, 140", "1", "912"], ["bone", "960, 96", "1", "960"], ["spring", "1104, 152", "0", "1104"]],
        "engine",
    )
    s.text(52, 486, "sorted by home, as now", size=12, fill=MUTED)

    passes = [
        ("Think", "mind", "moves it, by its rule"),
        ("Touch", "touch, box, up, rest", "what the hero met, and how"),
        ("Draw", "picture, frames", "blits it"),
        ("Ask", "crushed, rides", "a column, not a list of kinds"),
    ]
    for i, (name, cols, does) in enumerate(passes):
        y = 306 + i * 50
        s.box(412, y, 556, 42, "engine", r=8)
        s.text(428, y + 26, name, size=13, weight=700, mono=True, fill=THEME["engine"][2])
        s.text(496, y + 26, cols, size=12, mono=True, fill=THEME["engine"][2])
        s.text(668, y + 26, does, size=12, fill=MUTED)
        s.arrow([(372, y + 21), (410, y + 21)])
    s.save(f"{OUT}/engine-things.svg")


if __name__ == "__main__":
    layers()
    subsystems()
    frame()
    things()
