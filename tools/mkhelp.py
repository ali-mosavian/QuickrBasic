#!/usr/bin/env python3
"""mkhelp.py [--check] [--stats] [-o OUT] [DIR]: help/*.md and help/status.txt compiled to BASIC.HLP.

The format (read by src/core/ui/helpfile.c):

  header   48 bytes: "QBHL", version, topics, keys, dictionary bytes, the ids of contents, index, using
           help, status and survival, the topics Ctrl+F1 steps through, then six u32: the sections'
           offsets and the file's size. Little-endian.
  topics   per topic: u32 offset in the data, u16 compressed bytes, u16 bytes.
  keys     per key, sorted: u16 offset in the pool, u16 topic. The pool: u8 length, the key's capitals.
  dict     the text the topics' compression starts from.
  data     each topic's text, compressed alone: LZSS over dict + the text so far, so it opens without
           the rest. Flag byte (bit set: a literal byte), else a u16 match: (distance - 1) << 3 | (length
           - 3), length 7 more: a byte, plus 10. A topic's text: its title, 0x0A, then lines, 0x0A apart; a
           line's 0x03 first makes it a heading; 0x01 id(u16) text 0x02 a hot link (id 0xFFFF: back).
"""

import argparse
import re
import struct
import sys
from dataclasses import dataclass, field
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MAGIC = b"QBHL"
WIDTH = 76  # a line's columns at most, a link's arrows counted
WINDOW = 8192  # LZSS's distance
DICT_MAX = 6000
RAW_MAX = 60000
NONE = 0xFFFF
BACK = 0xFFFF
LINK_RE = re.compile(r"\[([^\]]+)\]\(([^)]+)\)")
GLYPHS = {"◄": 0x11, "►": 0x10, "▲": 0x18, "▼": 0x19, "→": 0x1A, "←": 0x1B, "•": 0x07, "♦": 0x04}


class HelpError(Exception):
    pass


@dataclass
class Topic:
    path: str
    title: str = ""
    keys: list[str] = field(default_factory=list)
    lines: list[tuple[str, str]] = field(default_factory=list)  # (kind, text): "text" "head" "code"
    built: list[bytes] = field(default_factory=list)  # lines made as bytes (the Index's)
    raw: bytes = b""
    part: str = ""  # "": a whole topic; "Details" or "Example": a part of `main`
    ids: dict[str, int] = field(default_factory=dict)  # QuickSCREEN, Details, Example: topic ids


def norm(key: str) -> str:
    """A key as stored and looked up: capitals, single spaces."""
    key = " ".join(key.split()).upper()
    if not key or not key.isascii():
        raise HelpError(f"bad key {key!r}")
    return key


def cp437(text: str, where: str) -> bytes:
    out = bytearray()
    for ch in text:
        if ch in GLYPHS:
            out.append(GLYPHS[ch])
        elif ord(ch) < 0x80:
            if ord(ch) < 0x20:
                raise HelpError(f"{where}: control character {ord(ch)}")
            out.append(ord(ch))
        else:
            try:
                out += ch.encode("cp437")
            except UnicodeEncodeError:
                raise HelpError(f"{where}: {ch!r} is not in code page 437") from None
    return bytes(out)


def parse(path: Path, rel: str) -> Topic:
    where = rel
    lines = path.read_text(encoding="utf-8").expandtabs(4).split("\n")
    if not lines or not lines[0].startswith("# "):
        raise HelpError(f"{where}: the first line must be '# Title'")
    t = Topic(rel, lines[0][2:].strip())
    i = 1
    t.keys.append("~" + Path(rel).stem)
    if i < len(lines) and lines[i].startswith("keys:"):
        t.keys += [k.strip() for k in lines[i][5:].split(",") if k.strip()]
        i += 1
    fence = False
    for n, line in enumerate(lines[i:], i + 1):
        line = line.rstrip()
        if line.startswith("```"):
            fence = not fence
        elif fence or line.startswith("    "):
            t.lines.append(("code", line))
        elif line.startswith("## "):
            t.lines.append(("head", line[3:]))
        else:
            if "`" in line:
                line = line.replace("`", "")
            t.lines.append(("text", line))
    while t.lines and not t.lines[-1][1]:
        t.lines.pop()
    while t.lines and not t.lines[0][1]:
        t.lines.pop(0)
    if fence:
        raise HelpError(f"{where}: a code fence is never closed")
    return t


PARTS = ("Details", "Example")
PLAIN = {"contents", "using-help", "survival"}  # the topics with no Details and Example


def split_parts(t: Topic) -> list[Topic]:
    """A topic's `## Details` and `## Example` sections as topics of their own, the rest QuickSCREEN."""
    quick: list[tuple[str, str]] = []
    got: dict[str, list[tuple[str, str]]] = {}
    cur = quick
    for kind, text in t.lines:
        if kind == "head" and text in PARTS:
            if text in got:
                raise HelpError(f"{t.path}: two '## {text}' sections")
            cur = got.setdefault(text, [])
        else:
            cur.append((kind, text))
    if Path(t.path).stem in PLAIN:
        if got:
            raise HelpError(f"{t.path}: a plain topic has no Details or Example")
        return []
    for name in PARTS:
        if not any(x[1].strip() for x in got.get(name, [])):
            raise HelpError(f"{t.path}: no '## {name}' section")
    t.lines = quick
    return [Topic(t.path, f"{t.title} {name}", lines=got[name], part=name) for name in PARTS]


def link(topic: int, text: str) -> bytes:
    return b"\x01" + struct.pack("<H", topic) + text + b"\x02"


def plain_len(row: bytes) -> int:
    """A row's columns: its link markers take none."""
    out, i = 0, 0
    while i < len(row):
        if row[i] == 1:
            i += 3
        elif row[i] == 2:
            i += 1
        else:
            out += 1
            i += 1
    return out


def render(t: Topic, keymap: dict[str, int], index_id: int, contents_id: int, errors: list[str]) -> None:
    out = [cp437(t.title, t.path)]
    nav = b""
    if t.ids:  # QuickSCREEN, Details, Example, Contents, Index, spread over the row
        cells = [(3, "QuickSCREEN", t.ids["QuickSCREEN"]), (18, "Details", t.ids["Details"]),
                 (32, "Example", t.ids["Example"]), (46, "Contents", contents_id), (62, "Index", index_id)]
    else:
        cells = [(3, "Contents", contents_id), (28, "Index", index_id), (56, "Back", BACK)]
    for col, name, tid in cells:
        nav += b" " * (col - plain_len(nav)) + link(tid, b"\x11" + name.encode() + b"\x10")
    out.append(nav)
    out.append(b" " + b"\xc4" * 78)  # the rule under it
    for kind, text in t.lines:
        where = f"{t.path}: {text[:30]!r}"
        if kind == "code":
            body = (b" " if text else b"") + cp437(text, where)
            width = len(body)
        else:
            bullet = re.match(r"^(\s*)[-*] (.*)$", text)
            if bullet:  # a list item: QBasic's small square
                text = bullet.group(1) + "  \u25a0 " + bullet.group(2)
            body = b""
            width = 0
            pos = 0
            for m in LINK_RE.finditer(text):
                before = cp437(text[pos : m.start()], where)
                body += before
                key = norm(m.group(2))
                if key not in keymap:
                    errors.append(f"{t.path}: link to unknown key {m.group(2)!r}")
                    pos = m.end()
                    continue
                label = b"\x11" + cp437(m.group(1), where) + b"\x10"
                body += link(keymap[key], label)
                width += len(before) + len(label)
                pos = m.end()
            tail = cp437(text[pos:], where)
            body += tail
            width += len(tail)
            if body:
                body = b" " + body  # QBasic's one-column margin
                width += 1
            if kind == "head":
                body = b"\x03" + body
        if width > WIDTH:
            errors.append(f"{t.path}: line of {width} columns: {text[:40]!r}")
        out.append(body)
    out += t.built
    t.raw = b"\n".join(out)
    if len(t.raw) > RAW_MAX:
        errors.append(f"{t.path}: {len(t.raw)} bytes")


def index_topic(entries: list[tuple[str, int]]) -> Topic:
    """Every key found, three columns, as hot links."""
    t = Topic("<index>", "Index")
    t.lines.append(("text", "Type a letter to jump to the next keyword that starts with it."))
    t.lines.append(("text", ""))
    cells = [e for e in entries if len(e[0]) <= 22]
    for r in range(0, len(cells), 3):
        row = b"  "
        for k, (name, tid) in enumerate(cells[r : r + 3]):
            label = b"\x11" + cp437(name, "index") + b"\x10"
            row += link(tid, label) + b" " * (24 - len(label))
        t.built.append(row.rstrip())
    return t


def collect(src: Path) -> tuple[list[Topic], str]:
    topics = []
    for p in sorted(src.rglob("*.md")):
        rel = p.relative_to(src).as_posix()
        if p.name == "README.md":
            continue
        topics.append(parse(p, rel))
    status = (src / "status.txt").read_text(encoding="utf-8", errors="surrogateescape")
    return topics, status


# ---- compression


def lz_encode(dictionary: bytes, text: bytes) -> bytes:
    buf = dictionary + text
    start = len(dictionary)
    heads: dict[bytes, list[int]] = {}
    for i in range(0, start - 2):
        heads.setdefault(buf[i : i + 3], []).append(i)
    out = bytearray()
    items: list[bytes] = []
    flags = 0
    nbits = 0
    pos = start
    n = len(buf)

    def flush() -> None:
        nonlocal flags, nbits, items
        if nbits:
            out.append(flags)
            for it in items:
                out.extend(it)
        flags, nbits, items = 0, 0, []

    while pos < n:
        best = 0
        bdist = 0
        if pos + 3 <= n:
            for cand in reversed(heads.get(buf[pos : pos + 3], ())):
                dist = pos - cand
                if dist > WINDOW:
                    break
                ln = 3
                while pos + ln < n and ln < 265 and buf[cand + ln] == buf[pos + ln]:
                    ln += 1
                if ln > best:
                    best, bdist = ln, dist
                    if ln == 265:
                        break
        if best >= 3:
            v = (bdist - 1) << 3
            if best < 10:
                items.append(struct.pack("<H", v | (best - 3)))
            else:
                items.append(struct.pack("<HB", v | 7, best - 10))
            adv = best
        else:
            flags |= 1 << nbits
            items.append(bytes([buf[pos]]))
            adv = 1
        for j in range(pos, pos + adv):
            if j + 3 <= n:
                heads.setdefault(buf[j : j + 3], []).append(j)
        pos += adv
        nbits += 1
        if nbits == 8:
            flush()
    flush()
    return bytes(out)


def lz_decode(dictionary: bytes, data: bytes, size: int) -> bytes:
    buf = bytearray(dictionary)
    i = 0
    while len(buf) - len(dictionary) < size:
        flags = data[i]
        i += 1
        for bit in range(8):
            if len(buf) - len(dictionary) >= size:
                break
            if flags >> bit & 1:
                buf.append(data[i])
                i += 1
            else:
                v = data[i] | data[i + 1] << 8
                i += 2
                ln = (v & 7) + 3
                if ln == 10:
                    ln += data[i]
                    i += 1
                src = len(buf) - ((v >> 3) + 1)
                for k in range(ln):
                    buf.append(buf[src + k])
    return bytes(buf[len(dictionary) : len(dictionary) + size])


def make_dictionary(raws: list[bytes]) -> bytes:
    """The strings that most bytes would be saved by, best last (nearest the text)."""
    count: dict[bytes, int] = {}
    for raw in raws:
        seen = set()
        for m in re.finditer(rb"[A-Za-z$%&!#]{3,}(?: [A-Za-z$%&!#]{2,}){0,3}", raw):
            words = m.group().split(b" ")
            for a in range(len(words)):
                for b in range(a, len(words)):
                    seen.add(b" ".join(words[a : b + 1]))
        for s in seen:
            count[s] = count.get(s, 0) + 1
    scored = sorted(
        ((c - 1) * (len(s) - 1) - len(s), s) for s, c in count.items() if c > 2 and len(s) >= 4
    )
    picked: list[bytes] = []
    total = 0
    for score, s in reversed(scored):
        if score <= 0 or total + len(s) + 1 > DICT_MAX:
            continue
        if any(s in p for p in picked):
            continue
        picked.append(s)
        total += len(s) + 1
    return b" ".join(reversed(picked)) + b" "


# ---- the file


def build(src: Path) -> tuple[bytes, dict[str, int]]:
    topics, status = collect(src)
    errors: list[str] = []
    parts: list[Topic] = []
    for tid, t in enumerate(topics):
        try:
            made = split_parts(t)
        except HelpError as e:
            errors.append(str(e))
            continue
        if made:
            t.ids = {"QuickSCREEN": tid}
            for p in made:
                p.ids = t.ids
            parts += made

    keymap: dict[str, int] = {}
    shown: dict[str, tuple[str, int]] = {}
    for tid, t in enumerate(topics):
        for raw in t.keys:
            hidden = raw.startswith("~")
            k = norm(raw.lstrip("~"))
            if k in keymap and keymap[k] != tid:
                errors.append(f"{t.path}: key {k!r} is also {topics[keymap[k]].path}'s")
                continue
            keymap[k] = tid
            if not hidden and k not in shown:
                shown[k] = (raw.strip(), tid)
    if "INDEX" in keymap:
        errors.append("the key INDEX is the generated Index's")
    index_id = len(topics)
    keymap["INDEX"] = index_id
    ids: dict[str, int] = {}
    for name, key in (("contents", "CONTENTS"), ("using", "USING HELP"), ("survival", "SURVIVAL")):
        if key not in keymap:
            errors.append(f"required topic {key!r} is missing")
        ids[name] = keymap.get(key, NONE)
    entries = sorted(shown.items())
    topics.append(index_topic([(v[0], v[1]) for _, v in entries]))
    for p in parts:  # after the Index: Ctrl+F1 reaches them too
        p.ids["Details" if p.part == "Details" else "Example"] = len(topics)
        topics.append(p)
    shown["INDEX"] = ("Index", index_id)
    keymap_all = dict(keymap)
    for tid, t in enumerate(topics):
        try:
            render(t, keymap_all, index_id, ids["contents"] if ids["contents"] != NONE else 0, errors)
        except HelpError as e:
            errors.append(str(e))
    st = Topic("status.txt", "Status")
    st.raw = status.encode("utf-8", errors="surrogateescape")
    topics.append(st)
    if errors:
        raise HelpError("\n".join(errors))

    dictionary = make_dictionary([t.raw for t in topics])
    blobs = [lz_encode(dictionary, t.raw) for t in topics]
    for t, b in zip(topics, blobs):
        assert lz_decode(dictionary, b, len(t.raw)) == t.raw, t.path
    keys = sorted(keymap_all.items())
    pool = bytearray()
    kents = bytearray()
    for k, tid in keys:
        kents += struct.pack("<HH", len(pool), tid)
        pool += bytes([len(k)]) + k.encode("ascii")
    if len(pool) > 0xFFFF:
        raise HelpError("the key pool is over 64 KB")
    tents = bytearray()
    off = 0
    for t, b in zip(topics, blobs):
        tents += struct.pack("<IHH", off, len(b), len(t.raw))
        off += len(b)
    hdr_len = 48
    off_topics = hdr_len
    off_keys = off_topics + len(tents)
    off_pool = off_keys + len(kents)
    off_dict = off_pool + len(pool)
    off_data = off_dict + len(dictionary)
    total = off_data + off
    status_id = len(topics) - 1
    header = (
        MAGIC
        + struct.pack(
            "<HHHH5HH6I",
            1,
            len(topics),
            len(keys),
            len(dictionary),
            ids["contents"],
            index_id,
            ids["using"],
            status_id,
            ids["survival"],
            status_id,  # the topics Ctrl+F1 steps through: those before the status
            off_topics,
            off_keys,
            off_pool,
            off_dict,
            off_data,
            total,
        )
    )
    assert len(header) == hdr_len, len(header)
    data = header + tents + kents + pool + dictionary + b"".join(blobs)
    stats = {
        "topics": len(topics),
        "keys": len(keys),
        "raw": sum(len(t.raw) for t in topics),
        "dict": len(dictionary),
        "index": hdr_len + len(tents) + len(kents) + len(pool),
        "size": len(data),
    }
    return data, stats


def read_topic(data: bytes, tid: int) -> bytes:
    """Topic tid's text out of a built file (the C reader's job, for the tests)."""
    ntopics, _, dict_len = struct.unpack_from("<HHH", data, 6)[0], 0, struct.unpack_from("<H", data, 10)[0]
    off_topics, _, _, off_dict, off_data, _ = struct.unpack_from("<6I", data, 24)
    off, clen, rlen = struct.unpack_from("<IHH", data, off_topics + tid * 8)
    dictionary = data[off_dict : off_dict + dict_len]
    return lz_decode(dictionary, data[off_data + off : off_data + off + clen], rlen)


def compiled() -> bytes:
    """BASIC.HLP as the repo's help/ makes it, for the tools that put one beside a program."""
    return build(ROOT / "help")[0]


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("src", nargs="?", default=str(ROOT / "help"))
    ap.add_argument("-o", "--out")
    ap.add_argument("--check", action="store_true")
    ap.add_argument("--stats", action="store_true")
    a = ap.parse_args()
    try:
        data, stats = build(Path(a.src))
    except HelpError as e:
        print(e, file=sys.stderr)
        return 1
    if a.stats or a.check:
        print(" ".join(f"{k}={v}" for k, v in stats.items()))
    if a.out and not a.check:
        Path(a.out).parent.mkdir(parents=True, exist_ok=True)
        Path(a.out).write_bytes(data)
    return 0


if __name__ == "__main__":
    sys.exit(main())
