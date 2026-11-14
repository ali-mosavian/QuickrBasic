# ui: asm → C

Headers in `csrc/core/ui/`, prefixes in PORTING.md. `ui.h` holds the shared records and
`Uimn uimn` (the asm's `g_ui`); a `g_ui` field one module alone used is that module's static.
`*` marks a label another module uses (`src/ui/syms.inc`); the rest are statics, their C names the
implementer's unless given. Platform: `plat/pscr.h` (screen), `plat/pmou.h` (mouse), new.

## Conventions

- A screen place `DH, DL` is `row, col` parameters; `ms_pos` is `uimn.mouse.row, col`.
- An attribute slot `A_ED ..` is `SL_ED ..` (ui.h): comp's `dfa.h` has an `A_STR`. Menu commands
  `A_NEW ..` are `ACT_NEW ..` (menu.h).
- A key `AX` is a `uint16_t`, scan << 8 | char; `0FFFFh` `KEY_ALT`, `MS_KEY` `KEY_MOUSE`.
- A flag result is a `bool`, true the success: `CF set: none` → false. Each prototype says which.
- `TLBUF` and `TEDBUF` are one `LineBuf` whose `text` points at its storage (`uimn.edline_text`
  ..); `uimn_start` sets them (uimn stays zeroed, out of the image's data), view.c and dpan.c
  their own fields'.
- The clipboard `g_clpb` (regions.asm, IDE only) is `uimn.text.clip[CLIPSZ]`: no Addr reaches it,
  so it is not carved from the arena. `g_scrb` is scrn.c's static `PscrCell[2000]`.
- `g_state.io.ocap = 80h; sbx = 0; ...; ocap = 0` (capture to sbuf) is `cons.capture = CAP_SBUF;
  cons.sblen = 0; ...; cons.capture = 0`; `g_sbuf` `cons.sbuf`, `g_inbuf` `cons.inbuf`, `g_cbuf`
  `comp_cbuf`, `g_xbuf` `comp_xbuf`.
- Dialog tables (`TDLG` + `TCTL`s) are a `Dlog` with `ctl` pointing at a `DlogCtl[]`, defined
  where the asm defines them (static, not const: `focus`, `row` change).

## The core's references to UI names (rename them)

| C now | file | C name |
|---|---|---|
| `extern bool ui_on` | io/repl.c, vm/chai.c | `uimn.on` (`#include "ui/uimn.h"` under `#if IDE`) |
| `ui_start()` | io/repl.c | `uimn_start()` |
| `ui_ready()` | io/repl.c | `uimn_ready()` |
| `ui_err(code)` | io/repl.c | `uimn_error(code)` |
| `lm_init()` | main.c | `lmap_init()` (`ui/lmap.h`) |
| `dbug.hook` (vm/dbug.h) | `DebugHooks.trace` | installed by `udbg_init` via `session_debug` |
| `dbug.emit` | | set by `udbg_init` |
| `lexr_colof`, `lexr.col`, `lexr.all` (comp/lexr.h, IDE) | | used by edit (`ui_dxerr`), synt, udbg |
| `link_walk` (comp/link.h, IDE) | | used by edit (`ui_canon`'s `cm_walk`) |
| `decl_proc` (comp/decl.h, IDE) | | used by view (`dc_proc`) |
| `decl_tail`, `decl_step` (comp/decl.h, AC) | | used by cmpl |
| `list_body` (comp/list.h) | | used by text, edit (`ls_body`) |
| `comp.check` (`cm.cchk`) | | set by edit around `comp_line` |

## Core names the UI uses

| asm | C |
|---|---|
| `g_state.rn.contln`, `contcode`, `cstk`, `line`, `opip`, `stpflg` | `vm.cont.line`, `vm.cont.code`, `vm.cstk`, `vm.line`, `vm.opip`, `vm.stop` |
| `CSTOP` | `allo.cs_top` |
| `g_state.mm.cachebase`, `progbase`, `progend`, `addfl`, `addat`, `preptr`, `prelen` | `allo.cache`, `prog.base`, `prog.end`, `prog.add_flags`, `prog.add_at`, `prog.pre`, `prog.pre_len` |
| `g_state.rn.lnum` (tx_make) | `prog_add`/`prog_ins`'s `num` |
| `g_state.mm.bodyptr`, `bodylen` | their `body`, `len` |
| `g_state.io.sbx`, `ocap`, `pcol`, `isdev`, `lmode` | `cons.sblen`, `cons.capture`, `cons.col`, `cons.stdin_dev`, `repl.loading` |
| `g_state.cm.cchk`, `cproc` | `comp.check`, `comp.proc` |
| `g_state.lx.nsyms` | `symb.nvars` |
| `g_state.dbg.*` | `dbug.*` (vm.md) |
| `g_lxcol`, `g_lxall` | `lexr.col`, `lexr.all` |
| `lx_line`, `lx_colof`, `dc_line`, `dc_proc`, `cm_line`, `cm_walk`, `cm_cache`, `bk_proc`, `ls_body` | `lexr_line`, `lexr_colof`, `decl_line`, `decl_proc`, `comp_line`, `link_walk`, `link_cache`, `blok_scope`, `list_body` |
| `dx_at`, `dx_what`, `dx_todo`, `dx_shape`, `er_text` | `diag_at`, `diag_what`, `diag_todo`, `diag_shape`, `emsg_text` |
| `prg_init`, `prg_add`, `prg_ins`, `prg_cut`, `prg_drop`, `prg_body`, `prg_room`, `mem_pmov`, `var_clr`, `alloc_new`, `alloc_grow` | `prog_init`, `prog_add`, `prog_ins`, `prog_cut`, `prog_drop`, `prog_body`, `prog_room`, `copy_prog`, `symb_clear`, `allo_new`, `allo_grow` |
| `rpl_enter`, `rpl_run`, `rpl_cont`, `rpl_relink`, `rpl_ready` | `repl_enter(len)`, `repl_run()`, `repl_cont()`, `repl_relink()`, `repl_ready()` |
| `io_putc`, `io_fresh`, `plt_flush`, `plt_exit` | `cons_putc`, `cons_fresh`, `cons_flush`, `cons_exit` |
| `dbg_depth`, `dbg_pos`, `dbg_eval` | `dbug_depth`, `dbug_pos`, `dbug_eval` |
| `TERR` | `ErrOp` (vm/exec.h) |
| `F_PRC`, `F_LSP0`, `F_LSP1`, `TFRAME.line` | `ProcFrame.proc`, `.save0`, `.save1`, `Frame.line` |

## Platform

| asm | C |
|---|---|
| INT 10h AH=0 (mode 3) | `pscr_mode(3)` |
| INT 10h AH=5 (0500h, 0501h) | `pscr_page(0)`, `pscr_page(1)` |
| INT 10h AH=2, AH=3 | `pscr_cursor(page, row, col)`, `pscr_where(page, &row, &col)` |
| INT 10h AH=1 | `pscr_shape(lines)` (`scrn_shape`) |
| INT 10h AH=6 (0600h, attr 07h, 0..184Fh) | `pscr_scroll(0, 0, 0, 24, 79, 0x07)` |
| INT 10h AX=0920h BX=0107h CX=80 at 1800h | `pscr_fill(1, 24, 0, 0x0720, 80)` |
| INT 10h AX=0E07h | `pscr_beep()` |
| port 3DAh wait, `rep movsw` to B800h | `pscr_retrace()`, `pscr_put(0, 0, 0, cells, 2000)` |
| ports 3C8h/3C9h with `g_acpal` | `pscr_palette(rgb)` (the BIOS map is dos32's) |
| INT 33h AX=0 (and the null-vector test), 1, 2, 3 | `pmou_init`, `pmou_show`, `pmou_hide`, `pmou_state` (cells) |
| INT 16h AH=2, 10h, 11h | `pkey_shift`, `pkey_read`, `pkey_ready` (vm's) |
| `ds:[46Ch]` | `pclk_ticks()` (vm's) |
| INT 21h 3Dh 3Fh 3Ch 40h 3Eh | `pfil_open`, `pfil_read`, `pfil_create`, `pfil_write`, `pfil_close` |
| INT 21h 4400h on handle 1 (bit 80h) | `pfil_isdev(PFIL_STDOUT)` |
| INT 21h 4300h | `pfil_getattr` |
| INT 21h 1Ah 4Eh 4Fh (`dl_dta`) | `pfil_first`, `pfil_next` (`PfilFind`) |
| INT 21h 19h, 47h, 3Bh | `pfil_curdrive`, `pfil_curdir`, `pfil_chdir` |
| the PSP's tail (`g_plat.pspsel`, 80h) | `psys_cmdline` |
| the environment's tail: the program's path (`hl_load`, `ui_lin`) | `psys_self(buf, cap)` (declared by ui in plat/psys.h; dos32 to implement) |
| `ui_lin` (DPMI 0006h) | gone |

## scr → scrn

| asm | C |
|---|---|
| `sc_fill`* | `scrn_fill(row, col, slot, ch, n)` → col past |
| `sc_str`* | `scrn_str(row, col, s, slot, hot)` → col past |
| `sc_strn`* | `scrn_strn(row, col, s, n, slot)` → col past |
| `sc_slen`* | `scrn_slen(s)` |
| `sc_box`* | `scrn_box(row, col, rows, cols, slot, title)` |
| `sc_sep`* | `scrn_sep(row, col, cols, slot)` |
| `sc_attr`* | `scrn_attr(row, col, n, slot)` |
| `sc_shadow`* | `scrn_shadow(row, col, rows, cols)` |
| `sc_cur`* | `scrn_cursor(row, col)` |
| `sc_shape`* | `scrn_shape(lines)` |
| `sc_flip`* | `scrn_flip()` |
| `sc_at` | static |
| `g_scrb` | static `PscrCell` cells[25 * 80] |

## mouse → mous

| asm | C |
|---|---|
| `ms_init`*, `ms_show`*, `ms_hide`* | `mous_init()`, `mous_show()`, `mous_hide()` |
| `ms_poll`* (CF) | `bool mous_poll()` |
| `ms_title`* (DL → AL, CF set none) | `bool mous_title(col, &menu)` |
| `ms_main`* | `mous_main()` |
| `MS_PRESS MS_DRAG MS_UP MS_MOVE` | same (mous.h) |
| `MS_KEY` | `KEY_MOUSE` (ui.h) |
| `g_ui.ms.ms_on ms_b ms_ev ms_pos ms_hov` | `uimn.mouse.on button event row,col hover` |

## theme → them

| asm | C |
|---|---|
| `th_set` | `them_set(n)` |
| `th_qb`*, `th_dark`*, `th_pick`* (AL) | `them_qbasic()`, `them_dark()`, `them_pick(action)` |
| `th_in`*, `th_out`* | `them_in()`, `them_out()` |
| `th_ah`* (AH) | `them_attr(slot)` (inline) |
| `g_thn`* | `them.cur` |
| `g_tha` | `them.attr[NSLOT]` |
| `g_ths`, `g_acpal`, `zeda` | static `themes[NTHEME][TH_SZ]`; `g_acpal` is dos32 pscr's |
| `NATTR`, `NTHEME`, `TH_SZ` | `NSLOT` (ui.h), `NTHEME`, `TH_SZ` |

## syntax → synt

| asm | C |
|---|---|
| `hl_toggle`* | `synt_toggle()` |
| `hl_row`* (DX, ESI, ECX) | `synt_row(row, col, text, len)` |
| `g_hlon`* | `synt.on` (starts true) |
| `hl_line`, `g_hls g_hld g_hlen g_hlpa g_hlpc g_hlc`, `HS_*` | static |

## lmap → lmap

| asm | C |
|---|---|
| `lm_init`* | `lmap_init()` |
| `lm_build`* | `lmap_build()` |
| `lm_off`* (EAX → EBX) | `uint32_t lmap_off(line)` |
| `lm_of`* (EBX → EAX, CF) | `bool lmap_of(rec, &line)` |
| `HLMAP` | allo's `HLMAP` |
| `LMINIT` | static |

## text → text

| asm | C |
|---|---|
| `tx_init`* | `text_init()` |
| `tx_whole`*, `tx_view`* (EAX, EDX) | `text_whole()`, `text_view(first, n)` |
| `tx_rec`* | `uint32_t text_rec(line)` |
| `tx_get`* (EAX, EDI → ECX) | `uint32_t text_get(line, dst)` |
| `tx_repl`* (EAX, ESI, CX) | `text_repl(line, src, len)` |
| `tx_insl`* | `text_insert(line, src, len)` |
| `tx_dell`* | `text_delete(line)` |
| `tx_mark`*, `tx_mtog`*, `tx_mclr`* | `bool text_mark(line)`, `text_toggle(line)`, `text_unmark_all()` |
| `cb_put`* (ESI, CX, AL) | `text_clip_put(src, len, lines)` |
| `cb_lines`* (EAX, EDX) | `text_clip_lines(from, to)` |
| `tx_read`* (EDX → CF, AX) | `uint16_t text_read(name)` → PFIL_OK or the DOS error |
| `tx_write`* | `uint16_t text_write(name)` |
| `tx_text`, `tx_make`, `tx_put`, `tx_add`, `tx_tell`, `rd_peek`, `rd_byte`, `wr_flush`, `wr_out` | static |
| `g_rdh g_rderr g_rdpos g_rdcnt g_rdn g_rdb`, `RDSZ` | static |
| `cn_init`, `cn_byte`, `cn_eol` | `srcr_phys_start`, `srcr_phys_byte`, `srcr_phys_end` (io's) |
| `g_ui.tx.tx_n tx_v0 tx_all tx_hook` | `uimn.text.n first all hook` |
| `g_ui.tx.tx_mod tx_chg syn_on tx_asis` | `uimn.text.changed unsaved check asis` |
| `g_ui.tx.cb_len cb_ln`, `g_clpb` | `uimn.text.clip_len clip_lines clip` |
| `g_ui.tx.hl_len` | `uimn.help_len` |

## edit → edit

| asm | C |
|---|---|
| `le_ins`* (BX, AL, DX; CF full) | `bool edit_lb_type(b, c, most)` |
| `le_insn`* (BX, SI, CX; CF) | `bool edit_lb_insert(b, s, n)` |
| `le_cut`* (BX, CX, DX) | `edit_lb_cut(b, from, to)` |
| `le_bs`*, `le_del`* (CF: nothing done) | `bool edit_lb_bs(b)`, `bool edit_lb_del(b)` |
| `le_home`*, `le_end`*, `le_left`*, `le_right`*, `le_wleft`*, `le_wright`* | `edit_lb_home(b)`, `edit_lb_end(b)`, `edit_lb_left(b)`, `edit_lb_right(b)`, `edit_lb_wleft(b)`, `edit_lb_wright(b)` |
| `le_pad`, `le_indent` | static |
| `ed_load`* (AX) | `edit_load(line)` |
| `ed_commit`* (CF: an error) | `bool edit_commit()` (false: a syntax error shown) |
| `ed_goto`* | `bool edit_goto(line)` |
| `ed_up`*, `ed_down`*, `ed_pgup`*, `ed_pgdn`*, `ed_first`*, `ed_last`* | `edit_up()` .. `edit_last()` |
| `ed_ensure`*, `ed_enter`*, `ed_bs`*, `ed_del`*, `ed_tab`*, `ed_ctrly`* | `edit_ensure()`, `edit_enter()`, `edit_bs()`, `edit_del()`, `edit_tab()`, `edit_ctrly()` |
| `sel_mark`*, `sel_get`* (→ AL) | `edit_sel_mark()`, `uint8_t edit_sel()` (`SEL_NONE/PIECE/LINES`) |
| `ed_copy`* (→ AL), `ed_clear`*, `ed_cut`*, `ed_paste`* | `uint8_t edit_copy()`, `edit_clear()`, `edit_cut()`, `edit_paste()` |
| `im_load`*, `im_up`*, `im_down`* | `edit_imm_load()`, `edit_imm_up()`, `edit_imm_down()` |
| `im_next`* (→ AX) | `uint16_t edit_imm_next()` |
| `im_store` | static |
| `ui_body`* (EDI → EDI) | `uint8_t *edit_body(text)` |
| `ui_canon`* (SI, CX; CF) | `bool edit_canon(text, len)` |
| `ui_dxerr`* (ESI, EBX → EAX) | `uint16_t edit_dxerr(text, op)` |
| `ui_emsg`* (AL) | `edit_emsg(code)` |
| `ui_chkop` | static (`link_walk`'s each) |
| `g_ui.fd.uc_pre uc_err uc_eop uc_ecol uc_src uc_n` | static (ui_canon's state; `uc_ecol` read by `edit_commit`) |
| `g_ui.dg.wr_ind` (set before `dl_wrap`) | `dlog_wrap`'s `indent` |
| `g_ui.ed.*` (TEDIT) | `uimn.ed.*` (ui.h `UiEdit`: `ed_line` `line`, `ed_top` `top`, `ed_left` `left`, `ed_ins` `insert`, `ed_ind` `indent`, `ed_nlen` `nlen`, `ed_pfx` `prefix`, `sel_on`, `sel_l` `sel_line`, `sel_c` `sel_col`, `sel_a`, `sel_b`, `ed_vw` `view`, `ed_r0` `row0`, `ed_rows` `rows`, `ed_x` `x`, `ed_tw` `width`) |
| `g_ui.im.im_n im_i im_top im_r0 im_rows`, `bf.im_h` | `uimn.imm.n cur top row0 rows hist` |
| `g_ui.bf.ed_lb`, `im_lb` | `uimn.edline`, `uimn.immline` |

## draw → draw

| asm | C |
|---|---|
| `dr_all`* | `draw_all()` |
| `dr_edit`* (with `lp.dr_ina`) | `draw_edit(inactive)` |
| `dr_imm`*, `dr_status`* | `draw_imm()`, `draw_status()` |
| `dr_skeys`* (→ ESI: text, then keys) | `const StatusKeys *draw_keys()` |
| `dr_ttl`* (DH, CL, CH, ESI, BL, BH; `g_ttlx`, `g_ttlw`) | `draw_title(row, col, width, left, right, title, lit, arrow)` |
| `g_ttlx`*, `g_ttlw`* | `draw_title`'s `col`, `width` |
| `dr_hasfoc`, `dr_row`, `dr_vbar`, `dr_barx`, `dr_hbar`, `dr_cursor`, `ui_decn`, `ui_title` | static |
| `g_drls`, `g_drln`, `lp.dr_sk`, `lp.dr_ln`, `lp.ui_pos`, `bf.ui_ttl` | static (`dr_ln` passed to `udbg_row`) |
| `u_imm`, `u_f1`, `u_f1s` | static |

## win → wind

| asm | C |
|---|---|
| `wn_init`*, `wn_layout`*, `wn_draw`* | `wind_init()`, `wind_layout()`, `wind_draw()` |
| `wn_act`* (EBX), `wn_focus`* (EBX) | `wind_act(w)`, `wind_focus(w)` |
| `wn_cur`* (→ EBX) | `uint8_t wind_cur()` |
| `wn_next`*, `wn_prev`* | `wind_next()`, `wind_prev()` |
| `wn_at`* (DX → EBX, CF none) | `bool wind_at(row, col, &w)` |
| `wn_split`*, `wn_vsplit`*, `wn_grow`*, `wn_shrink`*, `wn_max`* | `wind_split()`, `wind_vsplit()`, `wind_grow()`, `wind_shrink()`, `wind_max()` |
| `wn_beside`, `wn_ptr`, `wn_swap`, `wn_curh`, `wn_move`, `wn_shown`, `wn_spare`, `wn_min`, `wn_unmax`, `g_wnord` | static |
| `TWIN wh wcol wed` | `UiWin height col ed` |
| `g_ui.wn.win wcols wact wmax wvert wvsav wsav` | `uimn.win.win cols active maxed vert vert_saved saved` |
| `NWIN W_HELP W_ED1 W_ED2 W_IMM W_DBG` | same (ui.h) |

## view → view

| asm | C |
|---|---|
| `vw_reset`*, `vw_apply`* | `view_reset()`, `view_apply()` |
| `vw_goline`* (AX → AX) | `uint32_t view_goline(line)` |
| `vw_of`* (AX → EBX, EDI) | `uint8_t view_of(line)`; `const View *view_at(v)` |
| `vw_title`* (EDI past the 0) | `view_title(title)` (appends) |
| `vw_next`*, `vw_prev`*, `vw_commit`*, `vw_newsub`*, `vw_newfn`*, `vw_subs`* | `view_next()`, `view_prev()`, `view_commit()`, `view_newsub()`, `view_newfn()`, `view_subs()` |
| `vw_kind`, `vw_order`, `vw_scan`, `vw_shift` (the hook), `vw_ptr`, `vw_show`, `vw_cpy`, `vw_find`, `vw_add`, `vw_del` | static |
| `TVIEW vfirst vn vkind vline vtop vcol vname` | `View first n kind line top col name` |
| `g_ui.vw.vw_n vw_nlen vw_tab`, `bf.vw_new vw_fld vw_list`, `dg.ls_v` | static |
| `NVIEW VNAME`; `VLSZ` | view.h; static |
| `u_kwsub .. u_delp`, `dg_vnew`, `dg_subs` | static |

## menu → menu

| asm | C |
|---|---|
| `mn_bar`* | `menu_bar()` |
| `mn_run`* (AL, AH) | `menu_run(m, open)` |
| `mn_alt`* (AH; CF set: not one) | `bool menu_alt(scan)` |
| `mn_altl`* (data) | `uint8_t menu_altchar(scan)` |
| `mn_x`*, `mn_tt`* | `menu_x[]`, `menu_title[]` |
| `ui_upper`* | `upcase(c)` (base.h); `is_letter`, `is_digit`, `is_blank` beside it |
| `mn_skip`, `mn_help`, `mn_off`, `mn_item`, `mn_size`, `mn_drop`, `mn_box`, `mn_move`, `mn_unalt`, `mn_swap`, `mn_find`, `mn_chk` | static |
| `mn_flags`, `mn_scan`, `mn_alta`, `mn_it`, `t_*`, `m_*` | static |
| `g_ui.mn.*` (TMENU), `bf.ui_hbuf` | static |
| `MF_*`, `NMENU`; `A_NEW .. A_NONE` | menu.h; `ACT_NEW .. ACT_NONE` |

## dlg → dlog

| asm | C |
|---|---|
| `dl_run`* (BX → AL) | `uint8_t dlog_run(d)` |
| `dl_msg`* (SI, BX → AL) | `uint8_t dlog_msg(text, buttons)` |
| `dl_ok`*, `dl_ync`* (→ AL) | `dlog_ok(text)`, `uint8_t dlog_ync(text)` |
| `dl_err`* | `dlog_err(text)` (writes a 0 at its first '\|') |
| `dl_wrap`* (ECX, ESI, EDI, `wr_ind` → EDI) | `uint8_t *dlog_wrap(dst, src, len, indent)` |
| `dl_open`*, `dl_saveas`*, `dl_find`* (CF set: cancelled) | `bool dlog_open()`, `bool dlog_saveas()`, `bool dlog_find()` |
| `dl_conf`*, `dl_change`* (→ AL) | `uint8_t dlog_conf()`, `uint8_t dlog_change()` |
| `dl_fok`* (CF set: stays) | `bool dlog_fileok(why)` |
| `dl_bhelp`* | `bool dlog_nohelp(why)` |
| `ui_ledit`* (BX, AX) | `dlog_ledit(b, key)` |
| `dl_ctl`, `dl_pos`, `dl_draw`, `dl_one`, `dl_dfld`, `dl_dlst`, `dl_dbtn`, `dl_hk`, `dl_acc`, `dl_dchk`, `dl_hit`, `dl_step`, `dl_run0`, `dl_bw`, `dl_files`, `dl_fsel`, `dl_dsel`, `dl_name1` | static |
| `dg_open`, `dg_save`, `dg_find`, `dg_conf`, `dg_chg` | static `Dlog`s |
| `TDLG row col rows cols ttl n foc sep def` | `Dlog row col rows cols title n focus sep def` (+ `ctl`) |
| `TCTL kind row col cols rows flg data cb lbl lrow lcol` | `DlogCtl kind row col cols rows flags field/list/text hook check label lrow lcol` (`cb` is `hook`, or a check box's `check`) |
| `TLIST n sel top items isz` | `DlogList n sel top items size` |
| `TBTN txt cols`, `bt_ok`, `bt_ync` | `MsgButton text cols`; static lists |
| `TMSGD`, `g_ui.dg.*` but `ls_v`, `bf.msg_txt msg_d ls_fl ls_dl dl_mask dl_path dl_dta` | static (`ls_f`, `ls_d` statically initialised with their item buffers: ui_start's setup) |
| `g_ui.bf.dl_fn`, `dl_name` | `uimn.fname`, `uimn.chosen` |
| `g_ui.bf.fd_what`, `fd_to`, `g_ui.fd.fd_case`, `fd_whole` | `uimn.find_what`, `uimn.change_to`, `uimn.match_case`, `uimn.whole_word` |
| `CT_* CF_NOF DO_OK DS_OK DF_OK DC_FV` | same (dlog.h); `DLOG_ESC` FFh |
| `u_open u_saveas u_find u_change u_fname u_files u_dirs u_skip u_chg1 u_fwhat u_cto u_case u_whole u_fverify u_chall u_mbas u_bas u_star u_yes u_no` | static |
| `DL_WRAP` | static (44) |

## dbg → udbg

| asm | C |
|---|---|
| `ud_init`*, `ud_prep`*, `ud_stop`* | `udbg_init()`, `udbg_prep()`, `udbg_stop()` |
| `ud_row`* (DX, `lp.dr_ln`) | `udbg_row(row, col, line)` |
| `ud_step`*, `ud_pstep`*, `ud_sout`*, `ud_restart`* | `udbg_step()`, `udbg_pstep()`, `udbg_sout()`, `udbg_restart()` (noreturn) |
| `ud_trace1`*, `ud_bp`*, `ud_bpclr`*, `ud_until`*, `ud_setnx`* | `udbg_tron()`, `udbg_bp()`, `udbg_bpclear()`, `udbg_until()`, `udbg_setnext()` |
| `ud_hlset`, `ud_trace` (DebugHooks.trace) | static |
| `g_ui.ud.stopd` | `uimn.stopped` |
| `g_ui.ud.hl_on hl_ln hl_c0 hl_c1 ud_utl` | static |
| `u_bnext`, `u_cncont` | static |

## dpane → dpan

| asm | C |
|---|---|
| `dp_open`*, `dp_close`*, `dp_draw`* | `dpan_open()`, `dpan_close()`, `dpan_draw()` |
| `dp_key`* (AX) | `dpan_key(key)` |
| `dp_mouse`* (DX), `dp_hover`* (DX) | `dpan_mouse(row, col)`, `dpan_hover(row, col)` |
| `dp_tip`* | `dpan_tip()` |
| every other `dp_*` | static |
| `g_ui.dp.dp_on`, `tt_ms` | `uimn.pane_on`, `uimn.tip_mouse` |
| `g_ui.dp.*` else (`TDPANE`, `TDFRM`), `g_dp*`, `u_dp*`, `dg_dpw` | static; `DP_STK NDFRM NDVAR NWATCH WXLEN NDP` static |
| `DP_COL DP_W DP_TW` | ui.h |

## compl → cmpl (AC)

| asm | C |
|---|---|
| `ac_tab`* (CF set: nothing fits) | `bool cmpl_tab()` |
| every other `ac_*`, `ac_vst ac_ops ac_lpar ac_args ac_x` | static |
| `g_ui.ac.*` (TCOMPL), `bf.ac_ev`, `bf.ac_list`, `TACE` | static |
| `AM_* AP_* AC_ROWS AC_LEN AK_* AT_* AC_PREV AC_MAX AC_SZ AC_EVN` | static (cmpl.c) |

## ui → uimn

| asm | C |
|---|---|
| `ui_start`* | `uimn_start()` |
| `ui_ready`* | `uimn_ready()` (noreturn) |
| `ui_err`* (AL) | `uimn_error(code)` (noreturn) |
| `ui_loop` | static (noreturn) |
| `ui_getkey`* | `uint16_t uimn_getkey()` |
| `ui_key`* (AX) | `uimn_key(key)` |
| `ui_act`* (AL) | `uimn_act(action)` |
| `ui_stopped`* (ZF set: so) | `bool uimn_stopped()` |
| `ui_cancont`* (CF set: cannot) | `bool uimn_cancont()` (true: can) |
| `ui_runp`*, `ui_contp`* | `uimn_run()`, `uimn_cont()` (noreturn) |
| `ui_selw`* (BX) | `uimn_selword(col)` |
| `ui_dec`* (AX, DI → DI) | `char *uimn_dec(dst, v)` |
| `ui_lin` | gone (`psys_self`) |
| `hl_load`, `ui_win`, `ui_nohelp`, `im_key`, `ed_key`, `ui_syntax`, `ui_about`, `ui_asksave`, `ui_reset`, `ui_new`, `ui_open`, `ui_load`, `ui_save`, `ui_saveas`, `ui_exit`, `ui_view`, `ui_in`, `ui_out`, `ui_outclr`, `ui_sync`, `ui_atline`, `ui_exec`, `ui_dxrun`, `ui_find`, `ui_find3`, `ui_search`, `ui_match`, `ui_wordc`, `ui_fdef`, `ui_change` | static |
| `ed_ws`, `ed_wsq`, `ed_moves`, `ui_acts` | static tables |
| `u_kedit`*, `u_krun`*, `u_kimm`* (text, then last column, routine) | `uimn_keys_edit`, `uimn_keys_run`, `uimn_keys_imm` (`StatusKeys`) |
| `u_ok`*, `u_cancel`*, `u_bhelp`*, `u_untl`*, `u_mbar`*, `u_dlg`* | `uimn_ok`, `uimn_cancel`, `uimn_helpbtn`, `uimn_untitled`, `uimn_menu_help`, `uimn_dlog_help` |
| `u_imm`*, `u_f1`*, `u_f1s`* | draw.c's statics |
| `u_open` .. `u_star`* (dlg's), `bt_ok`*, `bt_ync`* | dlog.c's statics |
| `u_oom`, `u_range`, `u_order` | unused, dropped |
| `u_notfound u_overw u_nomatch u_chdone u_notsaved u_access u_wordp u_nohelp u_about u_welcome u_anykey u_esc`, `bt_wel` | static |
| `g_ui`* | `uimn` (ui.h) |
| `g_hlpb`* | `uimn.help_text` |
| `g_ui.lp.ideon ui_foc ui_help ui_kf` | `uimn.on focus help keyflags` |
| `g_ui.lp.alt_t ui_rf ui_cpos ui_rpos ui_ecode ui_eln ui_wrap ul_new` | static (`ui_wrap` starts true) |
| `g_ui.fd.uc_all uc_f uc_sl uc_sc us_l us_c` | static |
| `g_ui.bf.uc_line`, `sy_buf`, `ui_fn`, `ui_ebuf` | `uimn.line`, `uimn.build`, `uimn.file`, `uimn.errmsg` |
| `IMN IMW ED_COLS DLBUF FLDMAX TXMAX LNMAX LNBUF HLPSZ` | ui.h |
| `VIDEO` | dos32 pscr's |
| `LSMAX` | dlog.c's |

## Platform declarations added

- `plat/psys.h`: `psys_self(buf, cap)` (the program's own path, for BASIC.HLP beside it).
- `plat/pscr.h`, `plat/pmou.h`: new.
