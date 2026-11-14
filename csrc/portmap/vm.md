# vm: asm → C

Headers in `csrc/core/vm/`. `ESI` is `vm.ip` (`uint8_t *`), `ESP` the data stack `vm.sp`
(`Cell *`, grows down, carved from the arena). `jmp ex_lp` / `dispatch` in a handler is a
`return` with `vm.ip` set; `stopchk` is `if (exec_stopchk()) return;`. A handler is
`void h(void)`, listed only in `vm/ops.def`: an op `OP(name, handler, size, flags)`, a CALL
routine `K(name, handler)`. `O_*`, `K_*`, `FX FV FB` are `vm/exec.h`'s enums from it. Lines are
program-store offsets (`uint32_t`, mem's `prog_line`). `CSTOP`, `CSLIM` are mem's
`allo.cs_top`, `allo.cs_lim`; `mm.cachebase` is `allo.cache`.

Errors: `repl_error` unwinds; `ex_trap` is `flow_trap`, which longjmps into `exec_run`'s loop
(`vm.loop`). The probe (`dbug_eval`) runs a nested `exec_run`; its end or error comes back by
`dbug_done` / `dbug_fail`. Interrupts only set `vm.stop` bits (`SF_BRK`; `SF_TICK` from
`pclk_init`/`pkey_init`); the STOP check polls them.

## isa.inc, vm.inc → ops.def, exec.h

In `ops.def`'s and `exec.h`'s headers.

## state.inc → exec.h

In `exec.h`'s header.

## exec.asm → exec.c

In `exec.c`'s header.

## bc.asm → operands.c

In `operands.c`'s header.

## flow.asm → flow.c

In `flow.c`'s header.

## calls.asm → stmts.c

In `stmts.c`'s header.

## builtins.asm → funcs.c

In `funcs.c`'s header.

## sys.asm → system.c

In `system.c`'s header.

## bin.asm → convert.c

In `convert.c`'s header.

## using.asm → using.c

In `using.c`'s header.

## dbg.asm → debug.c

In `debug.c`'s header.

## event.asm → event.c

In `event.c`'s header.

## chain.asm → chain.c

In `chain.c`'s header.

## Platform (declared by vm)

| asm | C |
|---|---|
| INT 21h 2Ah 2Bh 2Ch 2Dh | `pclk_date`, `pclk_set_date`, `pclk_time`, `pclk_set_time` |
| `ds:[46Ch]` | `pclk_ticks()` |
| INT 1Ch's `ev_tick` call | `ptick.asm` sets `SF_TICK` in `vm.stop`; `pclk_init(&vm.stop, SF_TICK)` hooks it. No C runs in interrupts: `psys_init(stop, bit)` has no tick callback (io: psys.h changed) |
| tick unhook | `ptick_unhook()` in `psys_exit` (io's); `pclk_done` the same, unused |
| jiffies | `pclk_jiffies`, `pclk_set_jiffies` (with `toff`, for cons) |
| INT 16h 10h 11h | `pkey_read`, `pkey_ready` |
| `ds:[417h]` | `pkey_shift` |
| `plt_kflush` | `pkey_flush` |
| INT 16h 00h, DOS 0Bh/07h | `pkey_wait`, `pkey_poll` (for cons) |
| `ev_tick`'s key buffer scan | `pkey_init(&vm.stop, SF_TICK)`, `pkey_trap(traps)`, `pkey_taken()` |
| `in`, `out` | `psys_inp`, `psys_outp`: defined in `plat/dos32/ptick.asm`, not psys.c |
| `sc_sys`'s call | `psys_call(a)` (ptick.asm) |

## Start-up

`exec_init`, `flow_init` (the RESUME buffer), `stmt_init` (READ's buffer), `syst_init`: main.c calls each. io: clear `vm.running` where an error lands at `repl.top`, else a trap outside the loop longjmps to a dead `vm.loop`.
