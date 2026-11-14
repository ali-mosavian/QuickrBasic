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

| asm | C |
|---|---|
| `oplist`, `O_*`, `O_NOPS` | `ops.def` `OP(...)`, `O_*`, `O_NOPS` |
| `qbklist`, `K_*`, `K_N`, `K_FN`, `K_LEFT`, `K_MID2` | `ops.def` `K(...)`, `K_*`, `K_N`, `K_FN` (= `K_FN_SGN`), `K_LEFT`, `K_MID2` |
| `FX FV FB` | same |
| `TJUMP`, `JSIZE` | `Jump`, `JSIZE` |
| `TERR` | `ErrOp` |
| `TLREF` | `LineRef` |
| `CELL`, `dpush`, `dpop` | `Cell`; `exec_push`, `exec_pushi/a/d`, `exec_pop`, `exec_popi/a/d` |
| `dispatch` | `exec_run`'s loop |
| `vslot`, `bvar` | `exec_u16()` id, `symb_cell(id)` |
| `ipload` | `flow_place(line, off)` |
| `subchk` | `subscript` (exec.c) |
| `tspclr` | `strg.tmp_used = strg.tmp_floor` |
| `stopchk` | `exec_stopchk()` |
| `a1get` | inside `exec_*_a1` |

## state.inc TRUN (`g_state.rn`) → `exec` (exec.h) or locals

| asm | C |
|---|---|
| `opip` | `vm.opip` |
| `rsp` | `vm.rsp` |
| `stklim` | `vm.stklim` |
| `cplim` | `comp.depth` (comp's) |
| `cstk` | `vm.cstk` |
| `lsp` | `vm.lsp` |
| `line`, `runln`, `lnum` | `vm.line`, `vm.runln`, `vm.lnum` |
| `stpflg` | `vm.stop` (volatile) |
| `contln`, `contcode` | `vm.cont.line`, `vm.cont.code` |
| `onerr`, `inerr`, `errn`, `erl`, `uerr` | `vm.err.handler`, `vm.err.active`, `vm.err.code`, `vm.err.line`, `vm.err.user` |
| `resln`, `resat`, `resnext` | `vm.res_line`, `vm.res_at`, `vm.res_next` |
| `lfrom`, `lto` | `list_range`'s parameters (comp) |
| `narr` | `arry_*`'s `n` parameter (mem) |
| `stv`, `aret`, `cnarg`, `cop`, `fid`, `flim`, `fstep`, `fop`, `fvp`, `fsv`, `isw_si`, `lk`, `onk`, `runl`, `ppip`, `ppend`, `pres` | locals / statics |
| `dlp`, `dataat`, `dyo`, `inend`, `iny0`, `iredo` | static in stmts.c (READ, INPUT) |
| — | `vm.ip`, `vm.sp`, `vm.stktop`, `vm.lsav`, `vm.loop` (new) |

## exec.asm → exec

| asm | C |
|---|---|
| `ex_lp` | `exec_run()` (noreturn), from `vm.ip` |
| `ex_brk` | `bool exec_brk()` |
| `arr_r1` | `Addr exec_elem1(id, sub)` |
| `arr_pn` | `Addr exec_elem()` |
| `rec_get` | `Addr exec_record(id)` |
| `int_div` | `int32_t exec_idiv(a, b, &rem)` |
| `g_ops` | static table in exec.c from ops.def |
| `g_calls` | static table in exec.c from ops.def |
| `g_bflt` | static in operands.c |
| `g_fntab` | static in funcs.c |
| `op_call` | `exec_call` |
| `op_sldt` | `exec_s_ld_txt` |
| `op_raddr`, `op_raddra`, `op_pref`, `op_vaddr`, `op_vld`, `op_vst` | `exec_r_addr`, `exec_ra_addr`, `exec_p_ref`, `exec_v_addr`, `exec_v_ld`, `exec_v_st` |
| `op_flld`, `op_flst`, `op_ichk16`, `op_sfld`, `op_sfst`, `op_rcopy` | `exec_fl_ld`, `exec_fl_st`, `exec_i_chk16`, `exec_sf_ld`, `exec_sf_st`, `exec_r_copy` |
| `op_ilda1`, `op_flda1`, `op_slda1`, `op_lda`, `op_slda` | `exec_i_ld_a1`, `exec_f_ld_a1`, `exec_s_ld_a1`, `exec_i_ld_a`, `exec_s_ld_a` |
| `op_sstv`, `op_ista1`, `op_fsta1`, `op_ssta1`, `op_istan`, `op_fstan`, `op_sstan` | `exec_s_st_v`, `exec_i_st_a1`, `exec_f_st_a1`, `exec_s_st_a1`, `exec_i_st_a`, `exec_f_st_a`, `exec_s_st_a` |
| `ex_cmpt` | static |
| `op_scmp`, `op_sif`, `op_scat` | `exec_s_cmp`, `exec_s_if`, `exec_s_cat` |
| `g_stkb`, `STKTOP` | carved by `exec_init`; `vm.stktop` |

## bc.asm → oper

| asm | C |
|---|---|
| `bc_X` (every op handler) | `oper_x`, the op's name lower case (`bc_I_ADD_V` → `oper_i_add_v`); `bc_F_LD_V` is `oper_i_ld_v` |
| `bvar`, `fcall`, `isrc`, `fsrc`, `ibin`, `fbin`, `ctail`, `icmp`, `fcmp` | statics / inline helpers |
| compare mask bits 1 2 4 | `OPER_GT`, `OPER_EQ`, `OPER_LT` |

## flow.asm → flow

| asm | C |
|---|---|
| `ex_dcode` (and `ex_line` for the direct line) | `uint8_t *flow_dcode()` |
| `ex_end` | `flow_end()` (noreturn) |
| `ex_erl` (ECX → EDX) | `uint16_t flow_erl(line)` |
| `ex_gsf` (AL → EDI) | `Addr flow_push(mark)` |
| `ex_hnd`, `g_thnd` | `uint32_t flow_handler()` returns it |
| `g_hfr` | static |
| `ex_trap` (AL) | `flow_trap(code, handler)` (noreturn) |
| `ex_jump` (EBX) | `flow_jump(line)` |
| `ex_nextln` | `flow_nextln()`, also NXLN's handler |
| `ex_nodir` | `flow_nodir()` |
| `ex_resolve` (DX, ESI) | `uint8_t *flow_resolve(line, off)` |
| `ex_setcont` | `flow_setcont()` |
| `ex_stmtof` (EBX, EAX → EDI, ECX) | `Stmt *flow_stmtof(line, code, &left)` |
| `ex_target` (→ EBX) | `uint32_t flow_target()` |
| `g_lsav`, `LSAVSZ` | `vm.lsav` (carved by `exec_init`) |
| `ls_mov` (EAX, EDI) | `flow_lsmove(from, to)` |
| `op_size` (ESI → ECX) | `uint32_t flow_opsize(op)` |
| `ex_line`, `ex_eol`, `ex_gosub`, `ex_jumpto`, `ex_sbase`, `for_find`, `for_in`, `for_mki`, `for_any`, `for_lp`, `pr_pop`, `err_nf`, `ipsave` | static |
| `g_pres8`, `g_lres` | static |
| `op_eol`, `op_goto`, `op_gosub`, `op_on`, `op_ret`, `op_for`, `op_nx0`, `op_nextv`, `op_err` | `flow_eol`, `flow_goto`, `flow_gosub`, `flow_on`, `flow_return`, `flow_for`, `flow_next`, `flow_next_v`, `flow_err` |
| `op_jmp`, `op_ijf`, `op_fjf`, `op_ijt`, `op_fjt`, `op_fpop` | `flow_jmp`, `flow_i_jf`, `flow_f_jf`, `flow_i_jt`, `flow_f_jt`, `flow_fpop` |
| `op_pcall`, `op_pret`, `op_onerr`, `op_onlerr`, `op_resume` | `flow_pcall`, `flow_pret`, `flow_onerr`, `flow_onlerr`, `flow_resume` |

## calls.asm → stmt

| asm | C |
|---|---|
| `ex_runfirst` | `stmt_runfirst()` |
| `ex_rungo` (EBX) | `stmt_rungo(line)` |
| `ex_ready` | static |
| `rd_first` | `stmt_restore()` |
| `sc_cont` | `stmt_cont()` |
| `sc_end` | `stmt_end()` (noreturn) |
| `sc_stop` | `stmt_stop()` (noreturn) |
| `g_relbit` | `compared` (operands.c): a compare's relation as `OPER_GT`, `OPER_EQ` or `OPER_LT` |
| `sc_ipr`, `sc_fpr`, `sc_spr`, `sc_pnl`, `sc_pzone`, `sc_ptab` | `stmt_ipr`, `stmt_fpr`, `stmt_spr`, `stmt_pnl`, `stmt_pzone`, `stmt_ptab` |
| `sc_pspc`, `sc_fin`, `sc_sin`, `sc_din`, `sc_fget`, `sc_sget` | `stmt_pspc`, `stmt_fin`, `stmt_sin`, `stmt_din`, `stmt_fget`, `stmt_sget` |
| `sc_inbeg`, `sc_inpr`, `sc_infile`, `sc_inend`, `sc_inln`, `sc_fread` | `stmt_inbeg`, `stmt_inpr`, `stmt_infile`, `stmt_inend`, `stmt_inln`, `stmt_fread` |
| `sc_sread`, `sc_dread`, `sc_res`, `sc_resl`, `sc_dim`, `sc_erase` | `stmt_sread`, `stmt_dread`, `stmt_res`, `stmt_resl`, `stmt_dim`, `stmt_erase` |
| `sc_run`, `sc_runl`, `sc_list`, `sc_clr`, `sc_new`, `sc_nodir` | `stmt_run`, `stmt_runl`, `stmt_list`, `stmt_clr`, `stmt_new`, `stmt_nodir` |
| `sc_poke`, `sc_wait`, `sc_wait2`, `sc_inp`, `sc_out`, `sc_sys` | `stmt_poke`, `stmt_wait`, `stmt_wait2`, `stmt_inp`, `stmt_out`, `stmt_sys` |
| `sc_ipow`, `sc_fpow`, `sc_fti`, `sc_sti`, `sc_ist` | `stmt_ipow`, `stmt_fpow`, `stmt_fti`, `stmt_sti`, `stmt_ist` |
| `io_redo`, `io_item`, `rd_item`, `rd_load`, `rd_scan`, `rd_after`, `rd_sk`, `rd_seek` | static |
| `g_dimst`, `g_dmsk`, `g_rdbuf` | static |

## builtins.asm → func

| asm | C |
|---|---|
| `sc_fn1` | `func_fn1` (all of `K_FN_SGN` .. `K_FN_CHRS`) |
| `sc_left`, `sc_right`, `sc_mid`, `sc_mid2` | `func_left`, `func_right`, `func_mid`, `func_mid2` |
| `sc_fix`, `sc_instr`, `sc_lbnd`, `sc_ubnd`, `sc_space`, `sc_strg` | `func_fix`, `func_instr`, `func_lbnd`, `func_ubnd`, `func_space`, `func_strg` |
| `sc_ltrim`, `sc_rtrim`, `sc_ucase`, `sc_lcase`, `sc_hex`, `sc_oct` | `func_ltrim`, `func_rtrim`, `func_ucase`, `func_lcase`, `func_hex`, `func_oct` |
| `sc_sfix`, `sc_mids`, `sc_vseg`, `sc_sadd`, `sc_err`, `sc_erl` | `func_sfix`, `func_mids`, `func_vseg`, `func_sadd`, `func_err`, `func_erl` |
| `sc_error`, `sc_dval` | `func_error`, `func_dval` |
| `fn_sgn`, `fn_int`, `fn_abs`, `fn_usr`, `fn_fre`, `fn_pos`, `fn_peek`, `fn_len`, `fn_asc`, `fn_chr`, `fn_fstr` | static (rows of the fn1 table, with num's `fmth_sqr` .. `rndm_rnd`) |
| `fn_left`, `fn_right`, `fn_mid`, `str_from` | static |
| `g_fixcw` | gone (`fmth_fix`) |

## sys.asm → syst

| asm | C |
|---|---|
| `sys_init` | `syst_init()` |
| `sx_sret` (ECX) | `syst_ret(len)` |
| `g_env`, `ENVSZ` | `syst.env`, `ENVSZ` |
| `g_cmd`, `g_cmdn` | `syst.cmd`, `syst.cmdlen` |
| `sc_timer`, `sc_date`, `sc_time`, `sc_sdate`, `sc_stime`, `sc_inkey` | `syst_timer`, `syst_date`, `syst_time`, `syst_sdate`, `syst_stime`, `syst_inkey` |
| `sc_shell`, `sc_shell0`, `sc_cmd`, `sc_envs`, `sc_envset`, `sc_setmem` | `syst_shell`, `syst_shell0`, `syst_cmd`, `syst_envs`, `syst_envset`, `syst_setmem` |
| `sc_erdev`, `sc_erdevs`, `sc_sleep`, `sc_rndq`, `sc_rndz` | `syst_erdev`, `syst_erdevs`, `syst_sleep`, `syst_rndq`, `syst_rndz` |
| `sx_d2`, `sx_parts`, `sx_find` | static |
| `g_part`, `g_shtl`, `g_shcs`, `g_shcm`, `g_k100`, `u_seed`, `u_redo` | static |

## bin.asm → mkcv

| asm | C |
|---|---|
| `sc_mki`, `sc_mkl`, `sc_mks`, `sc_mkd`, `sc_mksm`, `sc_mkdm` | `mkcv_mki`, `mkcv_mkl`, `mkcv_mks`, `mkcv_mkd`, `mkcv_mksm`, `mkcv_mkdm` |
| `sc_cvi`, `sc_cvl`, `sc_cvs`, `sc_cvd`, `sc_cvsm`, `sc_cvdm` | `mkcv_cvi`, `mkcv_cvl`, `mkcv_cvs`, `mkcv_cvd`, `mkcv_cvsm`, `mkcv_cvdm` |
| `bn_mk`, `bn_cv` | static |

## using.asm → usng

| asm | C |
|---|---|
| `sc_usb`, `sc_usi`, `sc_usf`, `sc_usd`, `sc_uss`, `sc_use` | `usng_usb`, `usng_usi`, `usng_usf`, `usng_usd`, `usng_uss`, `usng_use` |
| `sc_usen` | `usng_usen` |
| `TUSF`, `us_*`, `g_ufmt` .. `g_utc`, `u_numc` | static |

## dbg.asm → dbug

| asm | C |
|---|---|
| `TDBG`, `g_state.dbg` | `Dbug dbug` |
| `dbg.mode`, `trace`, `bpon`, `utl`, `emit`, `pdep`, `uline`, `hook` | `dbug.mode`, `.trace`, `.bpon`, `.until`, `.emit`, `.pdep`, `.uline`, `.hook` |
| `dbg.wline`, `stmtfrom`, `stmtto` | `DebugPos` `.line`, `.from`, `.to` (returned by `dbug_pos`) |
| `g_dbev` | `dbug.probing` (its jmp_buf static) |
| `op_dbg` | `dbug_dbg` |
| `dbg_depth` | `uint32_t dbug_depth()` |
| `dbg_pos` (EAX, EBX) | `dbug_pos(line, code)` |
| `dbg_eval` (ESI, AL, EDI → AL) | `uint8_t dbug_eval(expr, proc, out)` |
| `dbg_evx` | `dbug_done()` (noreturn) |
| `dbg_ever` (AL) | `dbug_fail(code)` (noreturn) |
| `sc_tron`, `sc_troff` | `dbug_tron`, `dbug_troff` |
| `g_dbsvt`, `g_dbsv`, `g_dlprc` | static (the saved fields as a struct) |

## event.asm → evnt

| asm | C |
|---|---|
| `NEV`, `g_evtab` | `NEV`, `evnt.slot[]` |
| slot states 0 1 2, flags 1 2 | `ES_OFF`, `ES_ON`, `ES_STOP`; `EF_PENDING`, `EF_RUNNING` |
| `g_evti`, `g_evtc`, `g_evuk`, `g_evon` | `evnt.interval`, `evnt.count`, `evnt.ukey[]`, `evnt.on` |
| `g_evsc` | static |
| `ev_clr` | `evnt_clear()` |
| `ev_tick` | `evnt_poll(vm)`: polled at the STOP check on `SF_TICK`; the key buffer's work is the platform's (`pkey_trap`, `pkey_taken`) |
| `ev_run` | `bool evnt_run(vm)` |
| `ev_ret` (EBX) | `evnt_return(vm, slot)` |
| `ev_slot`, `ev_sig` | static |
| `op_onev`, `sc_evst`, `sc_keydef` | `evnt_onev`, `evnt_evst`, `evnt_keydef` |

## chain.asm → chai

| asm | C |
|---|---|
| `chn_start` | `chai_start()` |
| `sc_cmn`, `sc_chain` | `chai_cmn`, `chai_chain` |
| `chn_kind`, `chn_cell`, `chn_elems`, `chn_room`, `chn_sout`, `chn_sin`, `chn_save` | static |
| `g_chr`, `g_che`, `g_chf`, `g_cmn`, `g_cmid`, `g_chb`, `NCMN`, `CHBSZ` | static |

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
