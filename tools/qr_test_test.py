"""qr_test.py --update rewrote every case's NAME.out with LF endings: 30 files of CRLF output showed
as changed though their text was the same. An update keeps a file's line endings, and a file whose
text is the same is not written."""

import sys
import tempfile
from pathlib import Path

sys.path.append(str(Path(__file__).parent))
import qr_test


def kept_endings() -> None:
    with tempfile.TemporaryDirectory() as d:
        prog = Path(d) / "case.bas"
        out = prog.with_suffix(".out")
        out.write_bytes(b"1\r\n2\r\n")
        qr_test.update("case", prog, "1\n2\n")
        assert out.read_bytes() == b"1\r\n2\r\n", out.read_bytes()
        qr_test.update("case", prog, "1\n3\n")
        assert out.read_bytes() == b"1\r\n3\r\n", out.read_bytes()


def unchanged_unwritten() -> None:
    with tempfile.TemporaryDirectory() as d:
        prog = Path(d) / "case.bas"
        out = prog.with_suffix(".out")
        out.write_text("1\n")
        before = out.stat().st_mtime_ns
        qr_test.update("case", prog, "1\n")
        assert out.stat().st_mtime_ns == before, "an unchanged NAME.out was written"


def differs_past_the_start() -> None:
    """A case's output differing past its first 60 characters showed those alone, the same as wanted."""
    want = "".join(f"line {i}\n" for i in range(20))
    got = want.replace("line 15", "line 51")
    assert qr_test.differs(got, want) == "line 16: got 'line 51', want 'line 15'", qr_test.differs(got, want)
    assert qr_test.differs(want, want) == ""


def main() -> None:
    differs_past_the_start()
    kept_endings()
    unchanged_unwritten()
    print("ok")


if __name__ == "__main__":
    main()
