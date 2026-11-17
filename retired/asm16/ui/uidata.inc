; ==========================================================================
; uidata.inc - the editor's variables, zeroed at start (IDE=1 only)
; ==========================================================================

AC_MAX  =       48              ; completions listed at most
AC_SZ   =       16              ; one: its length, its text

ideon   db      ?                ; the editor runs
txseg   dw      ?                ; the text, the clipboard, file buffers
clpseg  dw      ?
wrkseg  dw      ?
hl_len  dw      ?                ; BASIC.HLP's bytes, at HLPSEG
tx_end  dw      ?
tx_n    dw      ?
tx_ci   dw      ?                ; a line looked up last, its offset
tx_co   dw      ?
cb_len  dw      ?
cb_ln   db      ?
tx_mod  db      ?                ; the text changed since the program was made
tx_chg  db      ?                ; since it was saved
syn_on  db      ?
ed_line dw      ?                ; the cursor's line, the first shown, the
ed_top  dw      ?                ; first column shown
ed_left dw      ?
ed_ins  db      ?                ; 1 insert, 0 overtype
ed_ind  dw      ?
ed_nlen dw      ?
sel_on  db      ?
sel_l   dw      ?
sel_c   dw      ?
sel_a   dw      ?
sel_b   dw      ?
uc_pre  dw      ?
uc_err  db      ?
uc_src  dw      ?
uc_n    dw      ?
uc_all  db      ?
uc_f    db      ?                ; Change found one
uc_sl   dw      ?
uc_sc   dw      ?
sy_md   db      ?
sy_tg   dw      ?
sy_i    dw      ?
sy_prev dw      ?
sy_on   db      ?                ; making the program: an error is at sy_i
sy_hid  db      ?
im_n    dw      ?
im_i    dw      ?
im_top  dw      ?
ui_foc  db      ?                ; 1: the immediate window
ui_help dw      ?                ; the status bar's help, or 0
ui_pos  db      12 dup(?)
ui_kf   db      ?
alt_t   db      ?
ui_rf   db      ?                ; a run: always wait for a key
ui_cpos dw      ?
ui_rpos dw      ?
ui_ecode db     ?
ui_wrap db      ?
ul_new  db      ?
us_l    dw      ?
us_c    dw      ?
dr_sk   db      ?
dr_ln   dw      ?
mn_c    db      ?                ; the menu, its item
mn_i    db      ?
mn_open db      ?
mn_on   db      ?
mn_cnt  db      ?
mn_w    db      ?
mn_l    db      ?
mn_d    db      ?
mn_q    db      ?
dl_cur  dw      ?
dl_cp   dw      ?
dl_hf   db      ?
dl_new  db      ?
dl_fs   dw      ?
dl_bx   dw      ?
dl_res  db      ?
msg_nl  db      ?
msg_bt  dw      ?
msg_bw  dw      ?
msg_b0  db      ?
msg_adj db      ?                ; 1: the box off the cursor's line
dl_pad  dw      ?                ; a button's spaces, left and right
dl_alt  db      ?                ; Alt was down: the hotkeys lit
ed_pfx  db      ?                ; Ctrl+Q waiting for its second key
ls_f    label      word
        db      10 dup(?)
ls_d    label      word
        db      10 dup(?)
fd_case db      ?
fd_whole db     ?
scrseg  dw      ?
ms_on   db      ?
ms_b    db      ?
ms_ev   db      ?
ms_pos  dw      ?
dl_hr   db      ?
if AC
ac_n    dw      ?                ; ac_list's names
ac_sel  dw      ?                ; the one lit
ac_top  dw      ?                ; the first shown
ac_pn   dw      ?                ; the word's length
ac_ps   dw      ?                ; its column
ac_at   dw      ?                ; the list's box: row, column
ac_m    db      ?                ; what fits (AM_*)
ac_st   db      ?                ; the statement's keyword
ac_pv   db      ?                ; what came last (AP_*, a token)
ac_lc   db      ?                ; the word in small letters
ac_bw   db      ?                ; the box's columns
ac_bn   db      ?                ; its rows of names
ac_pfx  db      AC_SZ dup(?)     ; the word in capitals
ac_tmp  db      2*AC_SZ dup(?)
ac_ty   db      ?                ; the type the expression wants (AT_*)
ac_lt   db      ?                ; the last operand's type
ac_fn   db      ?                ; the function whose ( is open, FFh an array
ac_flt  db      ?                ; its value's type
ac_plk  db      ?                ; the line before's last statement
ac_cur  db      ?                ; ac_evs on the word's own line
ac_dp   dw      ?                ; NEXTs not yet matched
ac_kd   label   word
        db      ?                ; ac_add: the kind
ac_rk   db      ?                ;   the rank
ac_xt   dw      ?                ;   the token or line
ac_nm   db      AC_SZ dup(?)     ; a name: its length, it
ac_on   db      AC_SZ dup(?)     ; the open FOR's variable
ac_sigb db      32 dup(?)        ; a function's form
endif

; The big buffers sit in the free space between the arrays' headers and the
; control stack, so the code cache keeps its room.
uc_line =       HDRLIM
sy_buf  =       uc_line+260
im_h    =       sy_buf+260
msg_txt =       im_h+IMN*IMW
msg_d   =       msg_txt+256
ls_fl   =       msg_d+D_CTL+CSZ*12
ls_dl   =       ls_fl+LSMAX*13
ed_lb   =       ls_dl+LSMAX*13   ; line buffers (LB)
im_lb   =       ed_lb+LB_TXT+260
FLDMAX  =       80               ; a dialog field's characters at most
dl_fn   =       im_lb+LB_TXT+260
fd_what =       dl_fn+LB_TXT+FLDMAX+2
fd_to   =       fd_what+LB_TXT+FLDMAX+2
ac_ev   =       fd_to+LB_TXT+FLDMAX+2 ; ac_evs: a line's FORs and NEXTs
AC_EVN  =       8
ui_hbuf =       ac_ev+AC_EVN*AC_SZ ; a help line, from BASIC.HLP
ui_fn   =       ui_hbuf+80       ; the file's name, 0 none
ui_ttl  =       ui_fn+80
dl_mask =       ui_ttl+82
dl_path =       dl_mask+80
ui_ebuf =       dl_path+80
dl_dta  =       ui_ebuf+64
dl_name =       dl_dta+44
UI_HIE  =       dl_name+80
.erre UI_HIE le CSLIM, <the IDE's buffers overrun the control stack>
; the completions: over the control stack, under the data stack's room
ac_list =       CSTOP
.erre ac_list+AC_MAX*AC_SZ le STKTOP-STKROOM, <ac_list overruns the stack>
