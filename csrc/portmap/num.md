# num: float/ → core/num/, plat/pfpu.h

A FLOAT is a `Sngl` (`uint32_t`, packed IEEE single), in a Cell's `.u`; never pass a C `float`
(llrm keeps it at the x87's precision until stored). A DOUBLE is a Cell's `.d`. Binary FLOAT ops
are `fmth_op(a, b)` = a OP b: the asm's EDX (ARG) is `a`, EAX (FAC) is `b`. `err_ov` and the
other `err_*` are `repl_error(E_*)`.

## flt.asm → sngl

| asm | C |
|---|---|
| err_ov | `repl_error(E_OV)` (io/repl) |
| flt_neg (local) | inline `a ^ SNGL_SIGN` unless ±0 |
| flt_abs | `sngl_abs` |
| flt_cmp, flt_key (local) | `sngl_cmp` |
| flt_sgn | `sngl_sgn` |
| flt_norm | `sngl_norm` |
| flt_w16 | `sngl_w16` |
| flt_insg | `sngl_skipsign` |
| num_amp | `sngl_hexoct` |
| flt_in | `sngl_in` |
| flt_dv | `sngl_from_dbl(dubl_in(...))` |
| flt_out | `sngl_print` |

## ftr.asm → rndm

| asm | C |
|---|---|
| g_state.ft.rs | `rndm.seed`; main sets `RNDM_SEED0` |
| fn_rnd | `rndm_rnd` |
| flt_pi, g_kpi | `SNGL_PI` (single.h) |
| g_kone | `SNGL_ONE` (single.h) |

## fpu.asm → fmth, pfpu

| asm | C |
|---|---|
| flt_finit, g_nofpu | `fmth_init` → `pfpu_init` |
| ex_fclr | `fmth_clear` → `pfpu_reset` |
| flt_add, flt_sub, flt_mul, flt_div | `fmth_add`, `fmth_sub`, `fmth_mul`, `fmth_div` |
| fn_pow | `fmth_pow` → `pfpu_pow` |
| flt_from16, flt_fromu16, flt_from32 | `fmth_from_i16`, `fmth_from_u16`, `fmth_from_i32` |
| flt_fix16, flt_fixu16, flt_fix32, flt_xfix (local) | `fmth_to_i16`, `fmth_to_u16`, `fmth_to_i32` |
| flt_int | `fmth_floor` |
| (sc_fix's inline FRNDINT, vm/builtins) | `fmth_fix` |
| fn_sqr, fn_exp, fn_log, fn_sin, fn_cos, fn_tan, fn_atn | `fmth_sqr`, `fmth_exp`, `fmth_log`, `fmth_sin`, `fmth_cos`, `fmth_tan`, `fmth_atn` → `pfpu_fn(PF_*)` |
| fx_exp, fx_x2x, fx_pow, fx_log, fx_sin, fx_cos, fx_tan, fx_trg, fx_atn | inside pfpu (`pfpu_fn`, `pfpu_dfn`, `pfpu_pow`, `pfpu_dpow`) |
| flt_fld, flt_fst, flt_xin, flt_xtst, flt_xfn (local) | gone: C, or inside pfpu |
| g_fcw 007Fh, g_fcwx, g_fcwd | pfpu's; the default is now 027Fh (53 bits), see pfpu.h |
| g_dmax, g_pwa, g_pwb, g_pwi | pfpu's |
| g_state.ft.fpa, fpb, fpi, fpw | gone: locals, or pfpu's |

## dbl.asm → dubl, pfpu

| asm | C |
|---|---|
| dbl_in, g_dval | `dubl_in` returns the value |
| dbl_in's AL | `dubl_in`'s `*kind`: `DK_ANY`, `DK_DBL`, `DK_SNGL` |
| dbl_scale (local) | `pfpu_scale10` |
| dbl_digits, g_ndig, g_dig, g_dp | `dubl_digits(v, ndig, dig, &dp)` |
| dbl_fmt, g_dbuf, ECX | `dubl_fmt(v, out)` returns the length; `DUBL_TXT` bytes |
| flt_fmt, g_dsgl | `dubl_fmtf` |
| num_fmt, g_expc (local) | static in dubl |
| dbl_chk (local) | static in dubl |
| g_dbig, g_dovf | pfpu's |
| g_fcwq, g_fcwc | pfpu's |
| g_p10 | pfpu's (exact powers to 10^22) |
| g_dfn, @dfn_* | `dubl_fn`'s switch on `DF_*` |
| g_dstv, g_dn, g_dm, g_dbcd, g_dtmp, g_dsg, g_dnz | locals |
| op_dldv, op_dstv, op_dldk | `dubl_ldv`, `dubl_stv`, `dubl_ldk` |
| op_dlda1, op_dlda, op_dsta1, op_dstan | `dubl_lda1`, `dubl_lda`, `dubl_sta1`, `dubl_sta` |
| op_dadd, op_dsub, op_dmul, op_ddiv | `dubl_add`, `dubl_sub`, `dubl_mul`, `dubl_div` |
| op_dneg, op_dcmp, op_dnz | `dubl_neg`, `dubl_cmp` (mask `DC_*`), `dubl_nz` |
| op_itod, op_ftod, op_dtof, op_dtoi | `dubl_itod`, `dubl_ftod`, `dubl_tof`, `dubl_toi` |
| op_dfn | `dubl_fn` (operand `DF_*`) |
| op_dpow | `dubl_pow` |
| sc_dpr, sc_dstr | `dubl_print`, `dubl_str` |

## plat/pfpu.h (DOS32: plat/dos32/pfpu.asm)

`pfpu_init`, `pfpu_reset`, `pfpu_fn`, `pfpu_dfn`, `pfpu_pow`, `pfpu_dpow`, `pfpu_scale10`,
`pfpu_digits`; status `PFPU_OK`, `PFPU_IQ`, `PFPU_OV`; functions `PF_SQR` .. `PF_EXP`.
