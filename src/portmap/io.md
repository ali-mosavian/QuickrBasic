# io: asm → C

Headers in `src/core/io/` and `src/plat/` (pfil, psys). `cons` holds io.asm and plat.asm's
console half; plat.asm's machine half goes to `plat/dos32` behind pfil, psys, pkey, pclk, pmem.

## Errors and unwinding

`err_raise` is `[[noreturn]] repl_error(code)`; `err_xx` is `repl_error(E_XX)`; `ch_err` is
`repl_qberr(qb)` (E_USR with `vm.err.user = qb`; `QbErr`, defs.h). repl_error clears `lexr.more`, then longjmps to the
first that holds: dbug's probe (`dbg_ever`); `comp_fail(code)` while `comp.busy`; `flow_trap`
when a handler takes it; else `repl.top` with `REPL_ERROR` (message, READY / batch exit /
`ui_err`). `vm.rsp`'s asm role as the C stack's reset point is `repl.top`; `rpl_ready` and
every `jmp rpl_ready` (ex_end, ex_ready) is `repl_ready()`, a longjmp there.

## Regions (each init carves its own with `allo_carve`)

| asm | C |
|---|---|
| `g_inbuf` | `cons.inbuf` (INMAX+1, `cons_init`; strg_item, strg_tods read it by Addr) |
| `g_sbuf` | `cons.sbuf` (SBMAX+1, `cons_init`) |
| `g_recb` | `file.recs` (NCH*RECMAX, `file_init`: FIELD's Descs point in) |
| `g_cbuf` | comp's `comp_cbuf` (comp.h) |
| `g_obuf` | `cons.obuf` (plain array) |

## main.asm → main.c

| asm | C |
|---|---|
| `rt_start` | `main()`: pmem_init, allo_init, the carving inits, the blocks, inits, `repl_main()` |
| `g_state` | gone: each part its module's struct (`cons`, `repl`, `chnl`, `exec`, ...) |
| `g_bss_s`/`g_bss_e` zeroing | C statics start zeroed |
| `g_plat.pspsel`, `g_plat.dsv` | gone (psys_cmdline, psys_environ) |

## io/io.asm → console

In `console.c`'s header.

## io/plat.asm → console

In `console.c`'s header.

## io/errmsg.asm → errmsg

In `errmsg.c`'s header.

## io/diag.asm → diag

In `diag.c`'s header.

## io/srd.asm → source

In `source.c`'s header.

## io/repl.asm → repl

In `repl.c`'s header.

## io/chan.asm → channel

In `channel.c`'s header.

## io/fio.asm → file

In `file.c`'s header.

