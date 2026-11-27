#!/usr/bin/env python3
# /// script
# requires-python = ">=3.12"
# dependencies = ["unicorn"]
# ///
"""mem_asm_test.py: the DOS memcpy, memset (mem.asm) and memmove (string.asm), run on an x86 emulator.

Both builds of mem.asm, registers (the runtime's) and stack (-DCDECL, gfx's slice and the drivers), against
Python's, for every size to past SMALL and some long ones, at every dst and src alignment; memmove over
every overlap. Each call stores its dwords aligned, and leaves the bytes around dst, ebx esi edi ebp, the
stack and the direction flag as they were. Run: uv run --script tools/mem_asm_test.py
"""

import os
import sys
import struct
import tempfile
import subprocess
from pathlib import Path

from unicorn import Uc
from unicorn import UC_MODE_32
from unicorn import UC_ARCH_X86
from unicorn import UC_HOOK_MEM_WRITE
from unicorn.x86_const import UC_X86_REG_EAX
from unicorn.x86_const import UC_X86_REG_EBP
from unicorn.x86_const import UC_X86_REG_EBX
from unicorn.x86_const import UC_X86_REG_ECX
from unicorn.x86_const import UC_X86_REG_EDI
from unicorn.x86_const import UC_X86_REG_EDX
from unicorn.x86_const import UC_X86_REG_ESI
from unicorn.x86_const import UC_X86_REG_ESP
from unicorn.x86_const import UC_X86_REG_EFLAGS

ROOT = Path(__file__).resolve().parent.parent
JWASM = os.environ.get("JWASM", str(ROOT / "build/deps/bin/jwasm"))
CODE, DATA, STACK, DONE = 0x100000, 0x200000, 0x300000, 0x0F0000
SPAN = 0x4000
SIZES = [*range(40), 63, 64, 65, 255, 1000, 4099]
SAVED = {UC_X86_REG_EBX: 0xB0B0B0B0, UC_X86_REG_ESI: 0x51515151, UC_X86_REG_EDI: 0xD1D1D1D1, UC_X86_REG_EBP: 0xB9B9B9B9}
DF = 1 << 10
REL32 = 0x14


def text(source: str, *defines: str) -> tuple[bytes, dict[str, int], list[tuple[int, str]]]:
    """source assembled as COFF: its .text, its symbols' offsets in it, and its REL32 fixups' sites and targets."""
    with tempfile.TemporaryDirectory() as tmp:
        obj = Path(tmp) / "t.obj"
        subprocess.run([JWASM, "-q", "-coff", *defines, f"-Fo{obj}", str(ROOT / source)], check=True)
        b = obj.read_bytes()
    nsec, symtab, nsym = struct.unpack_from("<2xHxxxxII", b)
    strings = symtab + nsym * 18

    def name(raw: bytes) -> str:
        if raw[:4] == b"\0\0\0\0":
            at = strings + struct.unpack_from("<I", raw, 4)[0]
            return b[at : b.index(b"\0", at)].decode()
        return raw.rstrip(b"\0").decode()

    syms, i = [], 0
    while i < nsym:
        raw, value, sec, _, _, aux = struct.unpack_from("<8sIhHBB", b, symtab + i * 18)
        syms.append((name(raw), value, sec))
        syms.extend([("", 0, 0)] * aux)
        i += 1 + aux
    for s in range(nsec):
        sname, _, _, size, raw_at, rel_at, _, nrel = struct.unpack_from("<8sIIIIIIH", b, 20 + s * 40)
        if sname.rstrip(b"\0") == b".text":
            fixups = []
            for r in range(nrel):
                site, sym, kind = struct.unpack_from("<IIH", b, rel_at + r * 10)
                assert kind == REL32, kind
                fixups.append((site, syms[sym][0]))
            return b[raw_at : raw_at + size], {n: v for n, v, sec in syms if sec == s + 1}, fixups
    raise AssertionError(f"{source}: no .text")


class Machine:
    """The routines placed at CODE, a buffer at DATA, and a call that runs one to its return."""

    def __init__(self, parts: list[tuple[str, tuple[str, ...]]]) -> None:
        self.uc = Uc(UC_ARCH_X86, UC_MODE_32)
        for at in (DONE, CODE, DATA, STACK):
            self.uc.mem_map(at, 0x1000 if at == DONE else 0x100000)
        self.at: dict[str, int] = {}
        placed, sites = CODE, []
        for source, defines in parts:
            code, names, fixups = text(source, *defines)
            self.uc.mem_write(placed, code)
            self.at |= {n: placed + v for n, v in names.items()}
            sites += [(placed + site, target) for site, target in fixups]
            placed += (len(code) + 15) & ~15
        for site, target in sites:
            self.uc.mem_write(site, struct.pack("<i", self.at[target] - (site + 4)))
        self.misaligned = 0
        self.uc.hook_add(UC_HOOK_MEM_WRITE, self.wrote)

    def wrote(self, uc: Uc, access: int, address: int, size: int, value: int, data: object) -> None:
        self.misaligned += size == 4 and address & 3 != 0 and DATA <= address < DATA + SPAN

    def call(self, routine: str, args: tuple[int, int, int], stacked: bool) -> int:
        """routine's eax for args, by registers or on the stack; the saved registers and the stack checked."""
        sp = STACK + 0x80000
        frame = [DONE, *args] if stacked else [DONE]
        sp -= 4 * len(frame)
        self.uc.mem_write(sp, struct.pack(f"<{len(frame)}I", *frame))
        for reg, value in zip((UC_X86_REG_EAX, UC_X86_REG_EDX, UC_X86_REG_ECX), args, strict=True):
            self.uc.reg_write(reg, 0xDEADBEEF if stacked else value)
        for reg, value in SAVED.items():
            self.uc.reg_write(reg, value)
        self.uc.reg_write(UC_X86_REG_ESP, sp)
        self.uc.reg_write(UC_X86_REG_EFLAGS, 0x2)
        self.misaligned = 0
        self.uc.emu_start(self.at[routine], DONE)
        assert not self.misaligned, f"{routine} stored {self.misaligned} dwords off their boundary"
        for reg, value in SAVED.items():
            assert self.uc.reg_read(reg) == value, f"{routine} changed a saved register"
        assert self.uc.reg_read(UC_X86_REG_ESP) == sp + 4, f"{routine} left the stack off"
        assert not self.uc.reg_read(UC_X86_REG_EFLAGS) & DF, f"{routine} left the direction flag set"
        return self.uc.reg_read(UC_X86_REG_EAX)

    def fill(self, pattern: bytes) -> None:
        self.uc.mem_write(DATA, pattern)

    def read(self) -> bytes:
        return bytes(self.uc.mem_read(DATA, SPAN))


def pattern() -> bytes:
    return bytes((i * 7 + 3) & 0xFF for i in range(SPAN))


def check_copy_and_fill(m: Machine, stacked: bool) -> None:
    """memcpy and memset at every alignment and size: dst's bytes as Python's, the rest untouched."""
    base = pattern()
    for n in SIZES:
        for d in range(4):
            dst = 0x100 + d
            for s in range(4):
                src = 0x2000 + s
                m.fill(base)
                assert m.call("_memcpy", (DATA + dst, DATA + src, n), stacked) == DATA + dst
                want = bytearray(base)
                want[dst : dst + n] = base[src : src + n]
                assert m.read() == want, f"memcpy n={n} dst+{d} src+{s} stacked={stacked}"
            for c in (0x00, 0xA5, 0x12345680):
                m.fill(base)
                assert m.call("_memset", (DATA + dst, c, n), stacked) == DATA + dst
                want = bytearray(base)
                want[dst : dst + n] = bytes([c & 0xFF]) * n
                assert m.read() == want, f"memset n={n} dst+{d} c={c:#x} stacked={stacked}"


def check_move(m: Machine) -> None:
    """memmove for every overlap of dst over src: as Python's slice copy."""
    base = pattern()
    for n in SIZES:
        for shift in range(-9, 10):
            src = 0x1000 + (n & 3)
            dst = src + shift
            m.fill(base)
            assert m.call("_memmove", (DATA + dst, DATA + src, n), False) == DATA + dst
            want = bytearray(base)
            want[dst : dst + n] = base[src : src + n]
            assert m.read() == want, f"memmove n={n} shift={shift}"


def main() -> None:
    check_copy_and_fill(Machine([("src/plat/dos32/mem.asm", ())]), stacked=False)
    check_copy_and_fill(Machine([("src/plat/dos32/mem.asm", ("-DCDECL",))]), stacked=True)
    check_move(Machine([("src/plat/dos32/mem.asm", ()), ("src/plat/dos32/string.asm", ())]))
    print("ok")


if __name__ == "__main__":
    sys.exit(main())
