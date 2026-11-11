; ==========================================================================
; ui/ui.inc - the editor's constants and structures; its state, g_ui, is in
; ui.asm
; ==========================================================================

HLPSZ   = 2048                  ; BASIC.HLP, the menus' help lines, at g_hlpb
VIDEO   = 0B8000h               ; page 0 of the text screen, linear
AC_MAX  = 48                    ; completions listed at most
AC_SZ   = 16                    ; one: its length, its text
AC_EVN  = 8
LNBUF   = 260                   ; a line buffer's text
FLDMAX  = 80                    ; a dialog field's characters at most

; mouse.asm
MS_PRESS = 1
MS_DRAG  = 2
MS_UP    = 3
MS_KEY   = 0FFFEh
; compl.asm
AM_ST   = 1                             ; statements
AM_FN   = 2                             ; functions, NOT
AM_OP   = 4                             ; the words after an operand
AM_VR   = 8                             ; variables
AM_LB   = 10h                           ; line labels
AP_ST   = 0                             ; ac_pv: a statement's start
AP_ID   = 1                             ;   an operand
AP_SEP  = 2                             ;   an operator, a bracket
AP_COM  = 3                             ;   a comma, a semicolon
AC_ROWS = 8                             ; the list's rows
AC_LEN  = 10                            ; a name's characters at most
AK_KW   = 0                             ; kinds: a keyword
AK_FN   = 1                             ;   a function: '(' put after
AK_VR   = 2                             ;   a variable
AK_LB   = 3                             ;   a line label
AT_ANY  = 0                             ; ac_ty: any type
AT_STR  = 1                             ;   a string
AT_NUM  = 2                             ;   a number
AC_PREV = 40                            ; a label's box: columns
; text.asm
TXMAX   = 0FF00h                ; the text's bytes at most
LNMAX   = 255                   ; a line's characters at most
; draw.asm
ED_ROWS = 18
ED_COLS = 78
ED_R0   = 2
IM_R0   = 22
IM_ROWS = 2
A_ED    = 17h
A_SEL   = 71h
A_TTL   = 71h                  ; the window's colours swapped
A_BAR   = 70h
A_HOT   = 7Fh
A_STAT  = 30h
; menu.asm
MF_GRAY = 1
MF_SEP  = 2
MF_CHK  = 4                     ; a mark when its flag (mn_chk) is on
MF_SEL  = 8                     ; gray without a selection
MF_CLP  = 16                    ; gray with the Clipboard empty
MF_ED   = 32                    ; gray in the immediate window
NMENU   = 8
A_MSEL  = 07h
A_MHOT  = 0Fh
A_GRAY  = 78h
A_NEW   = 1
A_OPEN  = 2
A_SAVE  = 3
A_SAVEAS = 4
A_EXIT  = 5
A_CUT   = 6
A_COPY  = 7
A_PASTE = 8
A_CLEAR = 9
A_OUT   = 10
A_FIND  = 11
A_FIND3 = 12
A_CHANGE = 13
A_START = 14
A_CONT  = 15
A_SYNTAX = 16
A_ABOUT = 17
A_NONE  = 18
; dlg.asm
CT_FLD  = 1
CT_LST  = 2
CT_BTN  = 3
CT_CHK  = 4
CT_TXT  = 5
CF_NOF  = 1                     ; takes no focus
A_DLG   = 70h
A_DSEL  = 07h
A_DFOC  = 7Fh
DO_OK   = 4
DS_OK   = 3
DF_OK   = 3
DC_FV   = 4
; ui.asm
IMN     = 10                    ; the immediate window's lines kept
IMW     = 80                    ; their size: length, text
LSMAX   = 128                   ; a dialog list's names at most

TLBUF    struc                   ; a line being edited
    col     dw      ?               ; the cursor's column
    len     dw      ?
    dty     db      ?               ; changed
    txt     db      LNBUF dup(?)
TLBUF    ends

TDLG     struc                   ; a dialog, its controls (TCTL) after it
    row     db      ?
    col     db      ?
    rows    db      ?
    cols    db      ?
    ttl     dd      ?               ; the title, 0 none
    n       db      ?               ; the controls
    foc     db      ?               ; the one with the focus
    sep     db      ?               ; the separator's row, 0 none
    def     db      ?               ; the default button
    spare   dw      ?
TDLG     ends

TCTL     struc                   ; a dialog's control
    kind    db      ?               ; CT_*
    row     db      ?               ; from the dialog's corner
    col     db      ?
    cols    db      ?
    rows    db      ?
    flg     db      ?               ; CF_*
    data    dd      ?               ; a field's TLBUF, a list's TLIST, a text
    cb      dd      ?               ; the callback, a check box's flag
    lbl     dd      ?               ; the label, 0 none
    lrow    db      ?               ; the label's place
    lcol    db      ?
TCTL     ends

TLIST    struc                   ; a dialog's list
    n       dw      ?
    sel     dw      ?               ; the one selected
    top     dw      ?               ; the first shown
    items   dd      ?
    isz     db      ?               ; an item's bytes
    spare   db      ?
TLIST    ends

TBTN     struc                   ; a message box's button
    txt     dd      ?
    cols    db      ?               ; its width, 0 its text's +4
TBTN     ends

TACE     struc                   ; a completion
    len     db      ?
    txt     db      AC_LEN+1 dup(?)
    kd      db      ?               ; its kind (AK_*)
    rk      db      ?               ; its rank
    xt      dw      ?               ; its token or line
TACE     ends
.erre size TACE eq AC_SZ

TIMH     struc                   ; an immediate window line
    len     db      ?
    txt     db      IMW-1 dup(?)
TIMH     ends

TMSGD    struc                   ; the message box: lines, buttons
    dlg     TDLG    <>
    ctl     TCTL    12 dup(<>)
TMSGD    ends

TTEXT    struc                   ; the text, the clipboard
    hl_len  dw      ?               ; BASIC.HLP's bytes, at hlpb
    tx_end  dw      ?
    tx_n    dw      ?
    tx_ci   dw      ?               ; a line looked up last, its offset
    tx_co   dw      ?
    cb_len  dw      ?
    cb_ln   db      ?
    tx_mod  db      ?               ; the text changed since the program was made
    tx_chg  db      ?               ; since it was saved
    syn_on  db      ?
TTEXT    ends

TEDIT    struc                   ; the program window, the selection
    ed_line dw      ?               ; the cursor's line, the first shown, the
    ed_top  dw      ?               ; first column shown
    ed_left dw      ?
    ed_ins  db      ?               ; 1 insert, 0 overtype
    ed_ind  dw      ?
    ed_nlen dw      ?
    ed_pfx  db      ?               ; Ctrl+Q waiting for its second key
    sel_on  db      ?
    sel_l   dw      ?
    sel_c   dw      ?
    sel_a   dw      ?
    sel_b   dw      ?
TEDIT    ends

TFIND    struc                   ; a line made canonical; Find and Change
    uc_pre  dd      ?
    uc_err  db      ?
    uc_src  dd      ?
    uc_n    dw      ?
    uc_all  db      ?
    uc_f    db      ?               ; Change found one
    uc_sl   dw      ?
    uc_sc   dw      ?
    us_l    dw      ?
    us_c    dw      ?
    fd_case db      ?
    fd_whole db      ?
TFIND    ends

TSYNC    struc                   ; the text made the program
    sy_md   db      ?
    sy_tg   dw      ?
    sy_i    dw      ?
    sy_prev dw      ?
    sy_on   db      ?               ; making the program: an error is at sy_i
    sy_hid  db      ?
TSYNC    ends

TIMM     struc                   ; the immediate window
    im_n    dw      ?
    im_i    dw      ?
    im_top  dw      ?
TIMM     ends

TLOOP    struc                   ; the key loop, the screen
    ideon   db      ?               ; the editor runs
    ui_foc  db      ?               ; 1: the immediate window
    ui_help dd      ?               ; the status bar's help, or 0
    ui_pos  db      12 dup(?)
    ui_kf   db      ?
    alt_t   db      ?
    ui_rf   db      ?               ; a run: always wait for a key
    ui_cpos dw      ?
    ui_rpos dw      ?
    ui_ecode db      ?
    ui_wrap db      ?
    ul_new  db      ?
    dr_sk   db      ?
    dr_ln   dw      ?
TLOOP    ends

TMENU    struc                   ; the menus
    mn_c    db      ?               ; the menu, its item
    mn_i    db      ?
    mn_open db      ?
    mn_on   db      ?
    mn_cnt  db      ?
    mn_w    db      ?
    mn_l    db      ?
    mn_d    db      ?
    mn_q    db      ?
TMENU    ends

TDIALOG  struc                   ; the dialog running, the message box
    dl_cur  dw      ?
    dl_cp   dd      ?
    dl_hf   db      ?
    dl_new  db      ?
    dl_fs   dw      ?
    dl_bx   dd      ?
    dl_res  db      ?
    msg_nl  db      ?
    msg_bt  dd      ?
    msg_bw  dw      ?
    msg_b0  db      ?
    msg_adj db      ?               ; 1: the box off the cursor's line
    dl_pad  dw      ?               ; a button's spaces, left and right
    dl_alt  db      ?               ; Alt was down: the hotkeys lit
    dl_hr   db      ?
    ls_f    TLIST   <>              ; the Files list
    ls_d    TLIST   <>              ; the Dirs list
TDIALOG  ends

TMOUSE   struc                   ; the mouse
    ms_on   db      ?
    ms_b    db      ?
    ms_ev   db      ?
    ms_pos  dw      ?
TMOUSE   ends

TCOMPL   struc                   ; Tab completion
    ac_n    dw      ?               ; ac_list's names
    ac_sel  dw      ?               ; the one lit
    ac_top  dw      ?               ; the first shown
    ac_pn   dw      ?               ; the word's length
    ac_ps   dw      ?               ; its column
    ac_at   dw      ?               ; the list's box: row, column
    ac_m    db      ?               ; what fits (AM_*)
    ac_st   db      ?               ; the statement's keyword
    ac_pv   db      ?               ; what came last (AP_*, a token)
    ac_lc   db      ?               ; the word in small letters
    ac_bw   db      ?               ; the box's columns
    ac_bn   db      ?               ; its rows of names
    ac_pfx  db      AC_SZ dup(?)    ; the word in capitals
    ac_tmp  db      2*AC_SZ dup(?)
    ac_ty   db      ?               ; the type the expression wants (AT_*)
    ac_lt   db      ?               ; the last operand's type
    ac_fn   db      ?               ; the function whose ( is open, FFh an array
    ac_flt  db      ?               ; its value's type
    ac_plk  db      ?               ; the line before's last statement
    ac_cur  db      ?               ; ac_evs on the word's own line
    ac_dp   dw      ?               ; NEXTs not yet matched
    ac_kd   db      ?               ; ac_add: the kind; a word with ac_rk
    ac_rk   db      ?               ;   the rank
    ac_xt   dw      ?               ;   the token or line
    ac_nm   db      AC_SZ dup(?)    ; a name: its length, it
    ac_on   db      AC_SZ dup(?)    ; the open FOR's variable
    ac_sigb db      32 dup(?)       ; a function's form
TCOMPL   ends

TUIBUF   struc                   ; the big buffers
    uc_line db      260 dup(?)      ; a line got from the text
    sy_buf  db      260 dup(?)      ; a line being built
    im_h    TIMH    IMN dup(<>)     ; the immediate window's lines
    msg_txt db      256 dup(?)      ; a message box's lines
    msg_d   TMSGD   <>              ; the message box
    ls_fl   db      LSMAX*13 dup(?) ; the Files list
    ls_dl   db      LSMAX*13 dup(?) ; the Dirs list
    ed_lb   TLBUF   <>              ; the cursor's line
    im_lb   TLBUF   <>              ; the immediate line
    dl_fn   TLBUF   <>              ; a dialog's file name field
    fd_what TLBUF   <>              ; Find what
    fd_to   TLBUF   <>              ; Change to
    ac_ev   db      AC_EVN*AC_SZ dup(?) ; ac_evs: a line's FORs and NEXTs
    ui_hbuf db      80 dup(?)       ; a help line, from BASIC.HLP
    ui_fn   db      80 dup(?)       ; the file's name, 0 none
    ui_ttl  db      82 dup(?)       ; the window's title
    dl_mask db      80 dup(?)       ; the Open box's mask
    dl_path db      80 dup(?)       ; the current directory
    ui_ebuf db      64 dup(?)       ; an error's message
    dl_dta  db      44 dup(?)       ; the directory search's DTA
    dl_name db      80 dup(?)       ; the file chosen
    ac_list TACE    AC_MAX dup(<>)  ; the completions
TUIBUF   ends

TUI      struc
    tx      TTEXT   <>
    ed      TEDIT   <>
    fd      TFIND   <>
    sy      TSYNC   <>
    im      TIMM    <>
    lp      TLOOP   <>
    mn      TMENU   <>
    dg      TDIALOG <>
    ms      TMOUSE  <>
    ac      TCOMPL  <>
    bf      TUIBUF  <>
TUI      ends
