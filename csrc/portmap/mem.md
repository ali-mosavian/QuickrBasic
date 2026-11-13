# mem: asm → C

Headers in `csrc/core/mem/`. A program-store line is a `uint32_t` offset in the store (the asm's
GS offsets); `prog_line(off)` is its `Line *`. A string value is the `Addr` of its Desc. Selectors
(FS, GS, `alloc_sel`, `sel_set`, `progsel`, `vsel`) are gone: `fs:[id*8].TVAR` is `symb_var(id)`,
`gs:[x]` is `prog_line(x)` / `at(prog.base + x)`. `mm.dslin` is 0 and dropped: a DS offset is an
`Addr`.

## Regions (regions.asm, main.asm's rt_start)

| asm | C |
|---|---|
| `plt_heap`'s hbot/htop | `pmem_init`, then `allo_init(lo, hi)` |
| `g_dsb+0BC00h` `HDRBASE`, `HDRLIM` | `allo.hdr_base`, `allo.hdr_base + HDRSZ` |
| `CSLIM`, `CSTOP` | `allo.cs_lim`, `allo.cs_top` |
| `g_pokeb` | `allo.poke` (POKESZ) |
| `g_tmpd`, `sg.tmplin` | `strg.tmp` (carved by `strg_init`) |
| `g_stkb`, `g_lsav`, `g_inbuf`, `g_clpb` | not mem's: carve with `allo_carve` if an Addr reaches them |
| `mm.peekseg` | dropped (always 0) |
| `alloc_new` cache + `mm.cachebase` | `allo.cache = allo_base(allo_new(0x10000, nullptr))` |
| `alloc_new` prog + `prg_place` | `allo_new(0x10000, prog_place); prog_place()` |

## copy (far.asm)

| asm | C |
|---|---|
| `mem_cpy`, `mem_up` | `copy_mem(dst, src, len)` (memmove) |
| `mem_zero` | `Addr copy_zero(a, len)` → a+len |
| `mem_pmov` | `copy_prog(dst, src, len)`, store offsets; ESI/EDI out: src+len, dst+len |
| `mem_movs` | `memmove` |
| `mem_line`, `mem_fs`, `mem_lin` | dropped (unused, identity) |

## allo (alloc.asm)

| asm | C |
|---|---|
| `TALLOC`, `g_state.alloc.tab[h]` | `AlloBlock`, `allo.block[h]` (`.base .cap .moved`) |
| `alloc.nb` | `allo.nblocks` |
| `NBLKH HCACHE HPROG HLMAP` | same |
| `mm.hbot`, `mm.htop` | `allo.bot`, `allo.top` |
| `mm.cachebase`, `cblea`, `cbcmp` | `allo.cache` (+ K) |
| `alloc_new` (ECX, EBX → EAX, EDX) | `uint32_t allo_new(size, moved)`; base `allo_base(h)` |
| `alloc_grow` (EAX, ECX; CF) | `bool allo_grow(h, size)`: true grown (CF clear) |
| `alloc_sel`, `sel_set` | dropped |
| — | `allo_init(lo, hi)`, `Addr allo_carve(size)` (new: the arena's fixed regions) |

## tabl (tab.asm)

| asm | C |
|---|---|
| `TTAB` | `Table` (`.block .cap .max .base .narr .size[] .at[]`); `TMAXA` |
| `tab_new` (ESI, ECX, EBX) | `tabl_new(t, entries, moved)` |
| `tab_room` (ESI, ECX; CF) | `bool tabl_room(t, entries)`: true room |
| `tab_place` (ESI → EDX) | `Addr tabl_place(t)` → the old base |

## prog (prog.asm)

| asm | C |
|---|---|
| `mm.progbase` | `prog.base` |
| `mm.progfirst`, `mm.progsel` | dropped (`prog.base + PROGBASE`) |
| `mm.progend` | `prog.end` |
| `mm.enterat` | `prog.entering` |
| `mm.preptr`, `mm.prelen` | `prog.pre`, `prog.pre_len` |
| `mm.addfl`, `mm.addat` | `prog.add_flags`, `prog.add_at` |
| `mm.bodyptr`, `mm.bodylen` | parameters `body`, `len` |
| `prg_findw` (rn.lnum, lnk.cursor → EBX, CF) | `bool prog_find(num, &line)`: true found |
| `prg_put` (lnum, ESI, CX) | `prog_put(num, body, len)` |
| `prg_add` (lnum, ESI, CX) | `prog_add(num, body, len)` |
| `prg_at` → EDX | `uint32_t prog_at()` |
| `prg_body` (EBX, EDI → EDI) | `uint8_t *prog_body(line, dst)` → past |
| `prg_cut` (EBX → EAX) | `uint32_t prog_cut(line)` |
| `prg_ins` (EBX, lnum, bodyptr/len → EAX) | `uint32_t prog_ins(line, num, body, len)` |
| `prg_drop`, `prg_init`, `prg_place` | `prog_drop()`, `prog_init()`, `prog_place()` |
| `prg_room` (EDX) | `prog_room(size)` |
| `prg_fill`, `prg_size` | static |

## symb (sym.asm)

| asm | C |
|---|---|
| `TVAR` (`vt nam typ info`) | `Var` (`.vt .name .type .info`); info bits `VI_ARR`, `SK_DBL`, `VK_*` |
| `TVX` (`next nloc scp dyn`) | `VarX` (`.next .nloc .scope .dyn`) |
| `TNAM` (`text flag typ prc`) | `Name` (`.text .flags .rec .proc`) |
| `TSYM`, `g_state.sym` | `Symb symb` |
| `sym.vars`, `sym.nams`, `sym.ntext` | `symb.vars`, `symb.names`, `symb.text` |
| `sym.tfree` | `symb.text_free` |
| `sym.vsel` | dropped |
| `lx.nsyms`, `lx.nnams` | `symb.nvars`, `symb.nnames` (declared by mem; not lexr's) |
| `lx.nptr`, `lx.nlen` | `symb_find_name`'s parameters |
| `cm.cscope`, `cm.cstat` | `symb_find_var`'s `scope`, `stat` |
| `g_plh` | `symb.first_local[]` |
| `g_deft` | `symb.deft[]` |
| `g_vhead`, `g_nhead` | static |
| `fs:[id*8].TVAR`, `vxat`, `nmat`, `cellof` | `symb_var(id)`, `symb_varx(id)`, `symb_name(id)`, `symb_cell(id)` |
| `nmtest id,bits` | `symb_name(id)->flags & bits` |
| `[ntext.at + TNAM.text]` | `symb_text(id)` (length, then letters) |
| `sym_new` | `symb_new()` |
| `var_size` | `symb_size()` |
| `var_clr` | `symb_clear()` |
| `var_cell` (EBX → ECX) | `Addr symb_cell(id)` |
| `sym_init` | `symb_init()` |
| `var_init` | `symb_vars_init()` |
| `nam_find` (nptr, nlen → EAX) | `uint16_t symb_find_name(text, len)` |
| `var_find` (AX, CL, DL, cscope, cstat → EAX) | `uint16_t symb_find_var(name, type, kind, scope, stat)` |
| `var_place`, `nam_place`, `txt_place` | static (moved routines) |
| `VINIT NINIT IDMAX VSLACK` | static |

## scop (scope.asm)

| asm | C |
|---|---|
| `B_SC B_NAM B_NXT B_FLG B_ET B_REC B_FIX B_LINE`, `BSZ` | `Binding` (`.scope .name .next .flags .et[2] .rec[2] .fix[2] .line`); the fields read by `scop_type`, `scop_rec`, `scop_fix` |
| `g_bnd`, `g_nbnd`, `g_bhead`, `g_lnbit`, `g_scs`, `g_scn`, `g_bzero` | static |
| `scp_init` | `scop_init()` |
| `scp_get` (AX, BX → EDI, CF) | `Binding *scop_get(scope, name)`, nullptr none |
| `scp_bind` | `Binding *scop_bind(scope, name)` |
| `scp_proc` (EAX) | `scop_proc(proc)`; sets comp's `cm.cproc` |
| `scp_own` → EAX | `uint16_t scop_own()` |
| `scp_look` (BX → EDI, EAX) | `const Binding *scop_look(name, &scope)`, never nullptr |
| `scp_decl` (EAX, CL, DL → ECX) | `scop_type(name, array)`, `scop_rec(name, array)`, `scop_fix(name, array)` |
| `scp_sfx` (EAX, ECX, DL → ECX) | `uint8_t scop_sfx(name, type, array)` |
| `scp_con` (CF set: is) | `bool scop_con(name)` |
| `scp_known` (CF set: not) | `bool scop_known(name)`: true bound |
| `scp_mshr` (EBX; CF set: is) | `bool scop_mshr(name)` |
| `scp_label` (AX, BX, DX) | `scop_label(scope, key, line)` |
| `scp_lfind` (AX, BX → EAX) | `uint32_t scop_lfind(scope, key)` |
| `scp_arec` (EAX → ECX) | `scop_rec(name, true)` |
| `scp_enter`, `scp_leave`, `scp_frame`, `scp_hash`, `scp_lany` | static |

## strg (str.asm)

| asm | C |
|---|---|
| `TSTR`, `g_state.sg` | `Strg strg` |
| `sg.tsp`, `sg.tspb` | `strg.tmp_used`, `strg.tmp_floor` (bytes, DSZ each) |
| `sg.tmplin`, `g_tmpd` | `strg.tmp` |
| `mm.sfr` | `strg.low` |
| `sg.sln`, `sg.spt` (str_get's out) | the `Desc` returned (`.len`, `.ptr`) |
| `sg.dpv`, `sg.sn` (str_alloc's out) | `strg_alloc`'s return, its `len` |
| `sg.bpo` | `strg_set`'s `desc` |
| `sg.s0v`, `sg.s1v` | `strg_sub`'s `start`, `most` |
| `sg.sac sg.sab sg.gq sg.gd` | locals |
| `g_nbuf` | static (`strg_near`'s buffer) |
| `str_get`, `str_getd` (EAX → sln, spt, ZF) | `Desc strg_get(desc)`; empty: `.len == 0` |
| `str_push` (sln, spt → EAX) | `Addr strg_push(s)` |
| `str_alloc` (AX → dpv, sn) | `Addr strg_alloc(len)` |
| `str_new` (dpv, sn → EAX) | `Addr strg_new(chars, len)` |
| `str_own` (sln, spt, EAX) | `strg_own(chars, len, owner)` |
| `str_gc` | `strg_gc()`; FRE is `strg.low - arry.end` after it |
| `str_set` (EAX, bpo) | `strg_set(value, desc)` |
| `str_near` (sln, spt → ESI) | `const uint8_t *strg_near(s)`; length `min(s.len, 255)` |
| `str_print` (sln, spt) | `strg_print(s)` |
| `str_item` (SI → EAX, SI) | `Addr strg_item(&pos)`, pos an Addr |
| `str_skq` | `const uint8_t *strg_skipq(p)` |
| `str_span` (SI, AL, AH → AX, BX, SI) | `uint32_t strg_span(&p, stop1, stop2)`; start = p before |
| `str_dlen` | `uint16_t strg_len(desc)` |
| `str_cat` (EDX, EAX) | `Addr strg_cat(left, right)` |
| `str_cmp` (EDX, EAX → CL 1/2/4) | `uint8_t strg_cmp(left, right)` → `SCMP_GT/EQ/LT` |
| `str_sub` (EAX, s0v, s1v) | `Addr strg_sub(desc, start, most)` |
| `str_tods` (spt, dpv, sn) | `Addr strg_tods(chars, src, len)` |
| `str_buftmp` (AX) | `Addr strg_buftmp(len)` |
| `str_lit`, `str_scan`, `str_fit`, `str_cpy`, `str_cpyd` | static |

## arry (arr.asm)

| asm | C |
|---|---|
| `mm.aend`, `mm.ahd` | `arry.end`, `arry.hdr` |
| `mm.aid` | `id` parameters |
| `mm.asz`, `g_afh g_afsz g_afend g_kn g_kesz g_kcnt g_ki g_kmul g_kid` | locals |
| `rn.narr` (arr_* input) | `n` parameters |
| `g_dimbuf`, `g_cntbuf`, `g_lbbuf` | `arry.subs`, `arry.counts`, `arry.lbounds` (ADIMS) |
| `g_dyn` | `dyn` parameter of `arry_dim`, `arry_new` |
| `g_afrep` | `arry_free`'s `rep`: 0 or `AREP` |
| `g_afhs` | `arry_free`'s return |
| `g_pold`, `g_pnew` | `arry_keep`'s `old`, `fresh` |
| `AH_PTR AH_FAST AH_CNT` | `ArrayHdr` (defs.h) `.data .fast .count[]`, lower bounds after the counts |
| `arr_ref` (EBX, narr, dimbuf → EDI) | `Addr arry_ref(id, n)` |
| `arr_bs` | `arry_bad_sub()` |
| `arr_esz` (EBX → ECX) | `uint32_t arry_esize(id)` |
| `arr_dim` (EBX, narr, dimbuf, g_dyn) | `arry_dim(id, n, dyn)` |
| `arr_new` (aid, narr, cntbuf, lbbuf, g_dyn → EDI) | `Addr arry_new(id, n, dyn)` |
| `arr_size` (EBX, EDI → EAX) | `uint32_t arry_size(id, hdr)` |
| `arr_free` (EBX, afrep) | `uint32_t arry_free(id, rep)` → header bytes |
| `arr_keep` (EBX, pold, pnew) | `arry_keep(id, old, fresh)` |
| `arr_rep` (EBX) | `arry_rep(id)` |
| `arr_chk` | static |
