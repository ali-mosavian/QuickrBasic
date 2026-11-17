# ui: asm → C

Headers in `src/core/ui/`, prefixes in PORTING.md. `ui.h` holds the shared records and
`Uimn uimn` (the asm's `g_ui`); a `g_ui` field one module alone used is that module's static.
`*` marks a label another module uses (`retired/asm32/ui/syms.inc`); the rest are statics, their C names the
implementer's unless given. Platform: `plat/pscr.h` (screen), `plat/pmou.h` (mouse), new.

## Conventions

- A screen place `DH, DL` is `row, col` parameters; `ms_pos` is `uimn.mouse.row, col`.
- An attribute slot `A_ED ..` is `SL_ED ..` (ui.h): comp's `dfa.h` has an `A_STR`. Menu commands
  `A_NEW ..` are `ACT_NEW ..` (menu.h).
- A key `AX` is a `uint16_t`, scan << 8 | char; `0FFFFh` `KEY_ALT`, `MS_KEY` `KEY_MOUSE`.
- A flag result is a `bool`, true the success: `CF set: none` → false. Each prototype says which.
- `TLBUF` and `TEDBUF` are one `LineBuf` whose `text` points at its storage (`uimn.edline_text`
  ..); `uimn_start` sets them (uimn stays zeroed, out of the image's data), view.c and debugpane.c
  their own fields'.
- The clipboard `g_clpb` (regions.asm, IDE only) is `uimn.text.clip[CLIPSZ]`: no Addr reaches it,
  so it is not carved from the arena. `g_scrb` is screen.c's static `PscrCell[2000]`.
- `g_state.io.ocap = 80h; sbx = 0; ...; ocap = 0` (capture to sbuf) is `cons.capture = CAP_SBUF;
  cons.sblen = 0; ...; cons.capture = 0`; `g_sbuf` `cons.sbuf`, `g_inbuf` `cons.inbuf`, `g_cbuf`
  `comp_cbuf`, `g_xbuf` `comp_xbuf`.
- Dialog tables (`TDLG` + `TCTL`s) are a `Dlog` with `ctl` pointing at a `DlogCtl[]`, defined
  where the asm defines them (static, not const: `focus`, `row` change).

## The core's references to UI names (rename them)

| C now | file | C name |
|---|---|---|
| `extern bool ui_on` | io/repl.c, vm/chain.c | `uimn.on` (`#include "ui/uimain.h"` under `#if IDE`) |
| `ui_start()` | io/repl.c | `uimn_start()` |
| `ui_ready()` | io/repl.c | `uimn_ready()` |
| `ui_err(code)` | io/repl.c | `uimn_error(code)` |
| `lm_init()` | main.c | `lmap_init()` (`ui/linemap.h`) |
| `dbug.hook` (vm/debug.h) | `DebugHooks.trace` | installed by `udbg_init` via `session_debug` |
| `dbug.emit` | | set by `udbg_init` |
| `lexr_colof`, `lexr.col`, `lexr.all` (comp/lexer.h, IDE) | | used by edit (`ui_dxerr`), synt, udbg |
| `link_walk` (comp/link.h, IDE) | | used by edit (`ui_canon`'s `cm_walk`) |
| `decl_proc` (comp/declare.h, IDE) | | used by view (`dc_proc`) |
| `decl_tail`, `decl_step` (comp/declare.h, AC) | | used by cmpl |
| `list_body` (comp/listing.h) | | used by text, edit (`ls_body`) |
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
| INT 10h AH=0 (mode 3), AX=1112h | `pscr_text(rows)` |
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

## scr → screen

In `screen.c`'s header.

## mouse → mouse

In `mouse.c`'s header.

## theme → theme

In `theme.c`'s header.

## syntax → highlight

In `highlight.c`'s header.

## lmap → linemap

In `linemap.c`'s header.

## text → text

In `text.c`'s header.

## edit → edit

In `edit.c`'s header.

## draw → draw

In `draw.c`'s header.

## win → window

In `window.c`'s header.

## view → view

In `view.c`'s header.

## menu → menu

In `menu.c`'s header.

## dlg → dialog

In `dialog.c`'s header.

## dbg → debugui

In `debugui.c`'s header.

## dpane → debugpane

In `debugpane.c`'s header.

## compl → complete

In `complete.c`'s header.

## ui → uimain

In `uimain.c`'s header.

## Platform declarations added

- `plat/psys.h`: `psys_self(buf, cap)` (the program's own path, for BASIC.HLP beside it).
- `plat/pscr.h`, `plat/pmou.h`: new.
