"""Formats C sources: clang-format (.clang-format), then a blank line around each control
statement and before a block's last return, so a function reads in steps.

    python3 tools/cfmt.py FILE...
"""
import re
import subprocess
import sys

CLANG_FORMAT = "/opt/homebrew/opt/llvm/bin/clang-format"
CONTROL = re.compile(r"(if|for|while|switch|do)\b")
DESIGNATOR = re.compile(r"\.\w+\s*=")


def explode_designators(text):
    """A trailing comma in a compound literal or initializer {.a = .., ...} of four or more
    fields: clang-format then puts one field per line. A table's rows stay whole."""
    out, i = [], 0
    for m in re.finditer(r"\{(\s*\.\w+\s*=[^{}]*)\}", text):
        body = m.group(1)
        if len(DESIGNATOR.findall(body)) < 4 or body.rstrip().endswith(","):
            continue
        before = text[: m.start()].rstrip()[-1:]
        if before not in (")", "="):  # a table's row: kept whole
            continue
        out.append(text[i : m.start(1)] + body.rstrip() + ", ")
        i = m.end(1)
    return "".join(out) + text[i:]


def is_comment(s):
    return s.startswith(("/*", "//"))


def comment_top(out, k):
    """The first line of the comments that end just above out[k]; k when none."""
    while k > 0:
        s = out[k - 1].strip()
        if s.startswith("//") or (s.startswith("/*") and s.endswith("*/")):
            k -= 1
        elif s.endswith("*/"):  # the end of a comment of several lines
            k -= 1
            while k > 0 and not out[k].strip().startswith("/*"):
                k -= 1
        else:
            return k
    return k


def kind_of(s):
    """A statement's kind, by its first line: blank lines go where the kind changes."""
    if CONTROL.match(s):
        return "control"
    if re.match(r"(return|break|continue|goto)\b", s):
        return "jump"
    return "plain"


def blank_lines(text):
    """Inside function bodies: a blank line where a block's statements change kind (plain,
    control, jump) and after each braced block, before the comments that lead the statement."""
    out = []
    frames = []  # per open brace: the kind of its last statement, None at its start
    at_start = True  # the line starts a statement
    in_comment = False  # inside a /* .. */ of several lines
    in_define = False  # a directive's line ended with a backslash: this one is its
    for line in text.split("\n"):
        s = line.strip()
        if in_define or s.startswith("#"):
            in_define = s.endswith("\\")
            out.append(line)
            continue
        if in_comment or not s:
            in_comment = in_comment and "*/" not in s
            out.append(line)
            continue
        comment = is_comment(s)
        if s.startswith("/*") and "*/" not in s:
            in_comment = True
        starts = at_start and not comment and not s.startswith(("}", "else"))
        if starts and frames and frames[-1] is not None:
            kind = kind_of(s)
            last = frames[-1]
            plain_then_jump = last == "plain" and kind == "jump"
            if last == "block" or (kind != last and not plain_then_jump):
                k = comment_top(out, len(out))
                if k > 0 and out[k - 1].strip():
                    out.insert(k, "")
        if starts and frames:
            frames[-1] = kind_of(s)
        if s.startswith(("case ", "default:")) and frames:
            frames[-1] = None
        out.append(line)
        if comment:
            continue
        code = re.sub(r'"(\\.|[^"\\])*"|\'(\\.|[^\'\\])*\'', '""', s)
        code = re.sub(r"/\*.*?\*/|//.*", "", code).strip()
        for ch in code:
            if ch == "{":
                frames.append(None)
            elif ch == "}" and frames:
                frames.pop()
                if frames and frames[-1] is not None:
                    frames[-1] = "block"
        at_start = code.endswith((";", "{", "}", ":")) or code == ""
    return "\n".join(out)


def main():
    for path in sys.argv[1:]:
        with open(path) as f:
            text = f.read()
        text = explode_designators(text)
        text = subprocess.run(
            [CLANG_FORMAT, "--style=file", f"--assume-filename={path}"],
            input=text, capture_output=True, text=True, check=True,
        ).stdout
        text = blank_lines(text)
        with open(path, "w") as f:
            f.write(text)


if __name__ == "__main__":
    main()
