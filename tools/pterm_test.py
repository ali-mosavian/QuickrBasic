"""The host's basic on a terminal (plat/host/pterm.c), driven through a pseudo-terminal."""
import os
import pty
import select
import sys
import time


def session(binary, keys):
    """The binary's output with keys typed one write at a time; killed if still running."""
    pid, fd = pty.fork()
    if pid == 0:
        os.execv(binary, [binary])
    out = b''

    def drain(seconds):
        nonlocal out
        end = time.time() + seconds
        while time.time() < end:
            if select.select([fd], [], [], 0.05)[0]:
                try:
                    out += os.read(fd, 4096)
                except OSError:
                    return

    for k in keys:
        drain(0.3)
        os.write(fd, k)
    drain(0.5)
    if not os.waitpid(pid, os.WNOHANG)[0]:
        os.kill(pid, 9)
        os.waitpid(pid, 0)
    return out.decode('latin1')


CASES = [
    # INPUT in a run: the runtime's signals interrupted the wait, read as Ctrl+C: BREAK IN 10.
    ('input_in_a_run', [b'10 INPUT a$: PRINT a$ + "!"\r', b'RUN\r', b'hi\r', b'\x04'], 'hi!'),
    # SHELL's child got the raw terminal: no echo, no lines.
    ('shell_gets_a_cooked_terminal', [b'SHELL "/bin/stty -a"\r', b'\x04'], ' icanon'),
    ('ctrl_c_breaks_a_loop', [b'10 GOTO 10\r', b'RUN\r', b'\x03', b'\x04'], 'BREAK IN 10'),
]


def main():
    failed = 0
    for name, keys, want in CASES:
        out = session(sys.argv[1], keys)
        if want not in out:
            failed += 1
            print(f'FAIL {name}: no {want!r} in\n{out}')
    print(f'pterm {len(CASES) - failed}/{len(CASES)}')
    sys.exit(1 if failed else 0)


main()
