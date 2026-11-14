# io: asm → C

Headers in `csrc/core/io/` and `csrc/plat/` (pfil, psys). `cons` holds io.asm and plat.asm's
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

## io/io.asm → cons

| asm | C |
|---|---|
| `io_putc` | `cons_putc(c)` |
| `io_space` | `cons_space()` |
| `io_spaces` | `cons_spaces(n)` |
| `io_nl` | `cons_nl()` |
| `io_fresh` | `cons_fresh()` |
| `io_int` | `cons_int(v)` |
| `io_u24` | `cons_uint(v)` |
| `io_num` | `cons_uint(v)` (a 16-bit value) |
| `io_hex4` | `cons_hex4(v)` |
| `io_puts` | `cons_puts(s)` |
| `io_msg` | `cons_msg(id)` |
| `io_kw` | `cons_kw(tok)` |
| `io_kw1` | `cons_kw1(n)` |
| `io_nth` | static |
| `io_prompt` | `cons_prompt()` |
| `io_read` | `cons_read()` |
| `io_field` | `cons_field()` |
| `g_msgs`, `g_kwlist`, `g_kwlist1` (gen.inc) | `cons_msgs`, `cons_kwlist`, `cons_kwlist1` (io/gen.h, x86dfa.py; defined in console.c) |

### `g_state.io` (TIO)

| asm | C |
|---|---|
| `ocap` | `cons.capture` (`CAP_SBUF` 80h, `CAP_VERIFY` 40h, `CAP_DIFF` 1) |
| `pcol` | `cons.col` |
| `sbx` | `cons.sblen` |
| `inlen` | `cons.inlen` |
| `inpos` | `cons.inpos` |
| `infl` bit 7 | `cons.from_file` |
| `isdev` | `cons.stdin_dev` |
| `iost` | `cons.status` |
| `oblen` | `cons.oblen` |
| `cin` | `chnl.in` (FFh standard input is channel 0) |
| `inh`, `fb` | gone (`PFIL_STDIN`, plat's own byte) |
| `toff` | pclk's (`pclk_jiffies`) |
| `fline` | `repl.fline` |
| `lmode` | `repl.loading` |
| `batch` | `repl.batch` |
| `lq` | comp's list (list.h), not io's |
| `lsv`, `outh`, `fln`, `fdv`, `fsa`, `fnam` | unused; dropped |

## io/plat.asm → cons and plat

| asm | C |
|---|---|
| `plt_putc` | `cons_out(c)` |
| `plt_flush` | `cons_flush()` |
| `plt_getline` | `cons_getline()` (-1: the end, or the STOP key) |
| `plt_getc` | `cons_getc()` |
| `plt_getb` | `cons_getb()` (-1 the end) |
| `plt_raw` | static (pfil_read of `PFIL_STDIN`) |
| `plt_exit` | `cons_exit(code)`: cons_flush, psys_exit |
| `plt_init` | `cons_init()` (stdin's kind) + `psys_init(&vm.stop, SF_BRK)` |
| `plt_brk`, `plt_tick` | dos32 `pstop.asm` (the tick's Ctrl+C scan); ON TIMER's tick is vm's `ptick.asm` |
| `plt_heap` | `pmem_init` (exists) |
| `plt_kflush` | `pkey_flush()` (vm's) |
| `plt_rdtim` | `pclk_jiffies()` (vm's) |
| `plt_settim` | `pclk_set_jiffies(j)` (vm's; no caller) |
| `plt_exec` | `psys_exec(path, tail, len, env, envlen)`; the caller flushes cons first |
| `g_plat` (TPLAT): `dsv`, `dsbase`, `pspsel`, `old23`, `old1c` | gone: dos32's statics (pstop.asm), pmem's base, llrm's `LL$PSP` |
| `g_xrm`, `g_xfail`, `g_xenvn`, `g_xenv`, `g_xseg`, `g_xsel` | dos32 psys's statics |
| `p_nomem` | main.c's "Out of memory" |

Keys: `pkey_wait` (INT 16h AH=0) for INPUT's line and INPUT$, `pkey_poll` (DOS 0Bh/07h) for
`cons_getc`.

## io/errmsg.asm → emsg

| asm | C |
|---|---|
| `er_text` | `emsg_text(code)` |
| `g_qberr` | `emsg_errnum[]` |
| `g_qbmsg`, `g_qbunp` | static |

## io/diag.asm → diag; io/msg.inc → io/msg.h

| asm | C |
|---|---|
| `dx_at` | `diag_at(text, col, code, dg, state)` |
| `dx_raise` | `diag_raise(code, dg, at, line, other, len)` |
| `dx_say` | `diag_say()` |
| `dx_what` | `diag_what()` |
| `dx_todo` | `diag_todo()` (bool: printed) |
| `dx_shape` | `diag_shape()` (bool: printed) |
| `dx_back`, `dx_stmt`, `dx_alpha`, `dx_upper`, `dx_name`, `dx_isw`, `dx_streq`, `dx_skip`, `dx_form`, `dx_find`, `dx_res`, `dx_syn`, `dx_sent`, `dx_fill`, `dx_p*` | static |
| `g_dx*` data | static |
| `g_dxr` | static ErrOp (exec.h) |
| `DG_*` (msg.inc) | `DG_*` (msg.h, `tools/x86msg.py`) |
| `g_dxmsg`, `g_dxfrm` | `diag_msgs`, `diag_forms` (msg.h under `DIAG_DATA`) |

## io/srd.asm → srcr

| asm | C |
|---|---|
| `sr_line` | `srcr_line()` (-1 the end) |
| `sr_more` | `srcr_more(&keep)` (a `LexrMore`) |
| `sr_mem` | dropped: no caller |
| `sr_fill` | static |
| `cn_init` | `srcr_phys_start()` |
| `cn_byte` | `srcr_phys_byte(c)` |
| `cn_eol` | `srcr_phys_end()` |
| `g_srfe` | `srcr.end` (a pointer into cons.inbuf) |
| `g_srpend`, `g_srend`, `g_srany` | `srcr.pend`, `srcr.done`, `srcr.any` |
| `g_srmem`, `g_cn*` | static |

## io/repl.asm → repl

| asm | C |
|---|---|
| `err_raise` | `repl_error(code)` |
| `err_sn`, `err_id`, `err_rg`, `err_us`, `err_cn`, `err_tm`, `err_st`, `err_iq`, `err_om`, `err_os`, `err_od` | `repl_error(E_SN)` etc. |
| `er_diag`, `rpl_mod` | static |
| `rpl_ready` | `repl_ready()` (longjmp to `repl.top`); its loop is `repl_main()` |
| `rpl_run` | `repl_run()` |
| `rpl_cont` | `repl_cont()` |
| `rpl_file` | static, run by `repl_main` at the start |
| `rpl_load` | `repl_load()` |
| `rpl_enter` | `repl_enter(len)` (`ENTER_DIRECT`, `ENTER_EMPTY`, `ENTER_STORED`) |
| `rpl_relink` | `repl_relink()` |
| `ent_line`, `rpl_incl`, `rpl_line`, `rpl_read`, `rpl_inc`, `rpl_noinc` | static |
| `g_fnmb` | `repl.file` |
| `g_finc` | `repl.incl` |
| `r_*` data | static |

## io/chan.asm → chnl

A channel is a `Chan *` (`&chnl.ch[i]`); the asm's EBX index is `c - chnl.ch`.

| asm | C |
|---|---|
| `ch_init` | `chnl_init()` |
| `ch_look` | `chnl_look(num)` (nullptr none) |
| `ch_find` | `chnl_find(num)` |
| `ch_err` | `repl_qberr(qb)` |
| `ch_doserr` | `chnl_doserr(dos)` |
| `ch_open` | `chnl_open(num, name, mode, reclen)` |
| `ch_close` | `chnl_close(c)` |
| `ch_clall` | `chnl_closeall()` |
| `ch_out` | `chnl_out(c)` |
| `ch_in` | `chnl_in(c)` |
| `ch_clr` | `chnl_clear()` |
| `ch_putc` | `chnl_putc(ch)` |
| `ch_getb` | `chnl_getb()` (-1 the end) |
| `ch_skiplf` | `chnl_skiplf()` |
| `ch_peek` | `chnl_peek(c)` (-1 none) |
| `ch_tell` | `chnl_tell(c)` |
| `ch_seek` | `chnl_seek(c, at)` |
| `ch_lof` | `chnl_lof(c)` |
| `ch_rw` AH=3Fh / 40h | `chnl_read(c, p, n)` / `chnl_write(c, p, n)` |
| `ch_ptr`, `ch_dev`, `ch_swcol`, `ch_wb`, `ch_flush`, `ch_drop`, `ch_at`, `ch_e52` | static |
| `g_ch` | `chnl.ch` |
| `g_och` | `chnl.out` |
| `g_ich` | `chnl.in` |
| `g_dose`, `g_devs` | static |

### TCHAN → Chan

| asm | C |
|---|---|
| `cnum` | `num` |
| `ckind` | `kind` |
| `cmode` | `mode` |
| `ccol` | `col` |
| `chnd` | `handle` |
| `creclen` | `reclen` |
| `cbn` | `count` |
| `cbp` | `next` |
| `cdirty` | `dirty` |
| `ceof` | `eof` |
| `cpos` | `pos` |
| `clast` | `last` |
| `cfld` | `field` |
| `cbuf` | `buf` |

## io/fio.asm → file

Handlers (`void h(void)`, ops.def's K entries).

| asm | C |
|---|---|
| `sc_odef` | `file_opendef` |
| `sc_omi`, `sc_omo`, `sc_oma`, `sc_omr`, `sc_omb` | `file_mode_in`, `file_mode_out`, `file_mode_app`, `file_mode_rnd`, `file_mode_bin` |
| `sc_orec` | `file_reclen` |
| `sc_open` | `file_open` |
| `sc_open2` | `file_open2` |
| `sc_close` | `file_close` |
| `sc_clall` | `file_closeall` |
| `sc_fiout` | `file_out` |
| `sc_fiin` | `file_in` |
| `sc_ficlr` | `file_console` |
| `sc_sline` | `file_lineinput` |
| `sc_wi`, `sc_wf`, `sc_wd`, `sc_ws`, `sc_wcom` | `file_write_int`, `file_write_flt`, `file_write_dbl`, `file_write_str`, `file_write_comma` |
| `sc_eof` | `file_eof` |
| `sc_lof` | `file_lof` |
| `sc_loc` | `file_loc` |
| `sc_seekf` | `file_seekpos` |
| `sc_seek` | `file_seek` |
| `sc_freef` | `file_freefile` |
| `sc_inps` | `file_input_str` |
| `sc_fattr` | `file_attr` |
| `sc_kill` | `file_kill` |
| `sc_name` | `file_rename` |
| `sc_mkdir`, `sc_rmdir`, `sc_chdir` | `file_mkdir`, `file_rmdir`, `file_chdir` |
| `sc_files` | `file_files` |
| `sc_gnv` | `file_novar` |
| `sc_get`, `sc_put` | `file_get`, `file_put` |
| `sc_fldb` | `file_field` |
| `sc_fld` | `file_fielditem` |
| `sc_lset`, `sc_rset` | `file_lset`, `file_rset` |
| `sc_bsave`, `sc_bload` | `file_bsave`, `file_bload` |
| `sc_lock` | `file_lock` |
| `sc_drop` | `file_drop` |
| `sc_nfa` | `file_noadv` |
| `fx_*` | static |
| `g_omode`, `g_orec` | `file.mode`, `file.reclen` |
| `g_recb` | `file.recs` |
| `g_rwdir`, `g_dosfn`, `g_rwbase`, `g_rwtmp`, `g_fldch`, `g_fn2`, `g_dta`, `g_omodes`, `u_*` | static |
