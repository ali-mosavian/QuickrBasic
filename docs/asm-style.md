# Assembly style

The layout rules (columns, labels alone, docstring blocks) are checked by `tools/asmstyle.py`;
`--format` adds the header and docstring checks below.

## Names

A global or file-level code label is `module_verb`: a prefix of at most 3 letters, an
underscore, then the verb. `@locals`, zero-page variables and data tables are not prefixed.

| Prefix | Module |
|---|---|
| `x_` | BASIC statements (`x_list`) |
| `fn_` | BASIC functions, transcendentals included (`fn_call`, `fn_sqr`) |
| `op_` | operators (`op_add`, `op_pow`) |
| `st_` | the statement loop and its line scanning (`st_run`, `st_eos`) |
| `var_` | variable access (`var_get`, `var_store`) |
| `ev_` | the evaluator and value conversion (`ev_toint`, `ev_tofloat`) |
| `lx_` | the tokenizer (`lx_line`) |
| `prg_` | the program store (`prg_find`, `prg_next`) |
| `sym_` | the symbol table |
| `str_` | strings (`str_alloc`, `str_gc`) |
| `arr_` | arrays (`arr_idvp`, `arr_zero`) |
| `flt_` | floats (`flt_add`, `flt_in`, `flt_out`) |
| `int_` | integer maths (`int_mul8`, `int_udiv`) |
| `io_` | console input and output (`io_putc`, `io_field`) |
| `err_` | the error report and every error exit (`err_raise`, `err_sn`) |
| `rpl_` | the direct mode loop (`rpl_ready`) |
| `fil_` | files and channels (`fil_open`) |
| `sys_` | TI, TI$, ST (`sys_read`) |
| `mem_` | memory moves |
| `plt_` | the platform interface, the same in every `src/platform/*` |
| `sim_` | the sim platform's own helpers |
| `rt_` | `rt_init` only |
| `for_` | FOR frames |
| `rd_` | the READ position |
| `ls_` | LIST |
| `ret_` | an `rts` in branch reach |

A stub that only brings a target into branch reach is `<target>_<where>`: `err_sn_def`,
`flt_zero_tr`. A bare `rts` stub is `ret_<where>`: `ret_fil`.

## File header

```
; ==========================================================================
; file.s - one-line summary
; ==========================================================================
; holds:    the routines it exports
; uses:     modules/state it depends on
; data:     data layouts (only if the file has data structures)
; calls:
;   routine
;     callee            note
; ==========================================================================
```

`calls:` is a call tree of the file's main routines.

## Docstring

```
; ::::::::::::::
; name - one-line summary.
;
; What is not obvious from the code: hidden constraints, why it is done this
; way, tricky flag or register use. As long as it needs to be; nothing trivial.
;
; in:     item    note
; out:    item    note
; keeps:  registers kept
; error:  ?MESSAGE  when
; ::::::::::::::
```

A blank `;` line between the summary, the explanation and the fields. One item per line, the
fields in this order, empty fields and an empty explanation left out, one block per entry label.
