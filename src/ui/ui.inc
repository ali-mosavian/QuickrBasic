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
MS_MOVE  = 4                            ; moved, no button, ms_hov set
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
; draw.asm; the attributes are slots, the theme's (theme.asm)
ED_COLS = 78
A_ED    = 0
A_SEL   = 1
A_TTL   = 2                     ; the window's colours swapped
A_BAR   = 3
A_HOT   = 4
A_STAT  = 5
A_MSEL  = 6
A_MHOT  = 7
A_GRAY  = 8
A_BPL   = 9                     ; a breakpoint's line
A_CUR   = 10                    ; the statement a stop or trace is at
A_DLG   = 11
A_DSEL  = 12
A_DFOC  = 13
A_SHD   = 14                    ; a box's shadow
A_THB   = 15                    ; a scroll bar's thumb
A_KW    = 16                    ; syntax.asm's: keywords, strings, numbers,
A_STR   = 17                    ; comments, names; a SUB's or FUNCTION's name
A_NUM   = 18                    ; where it is declared, its parameters,
A_COM   = 19                    ; types, operators
A_NAM   = 20
A_FNM   = 21
A_PAR   = 22
A_TYP   = 23
A_OP    = 24
NATTR   = 25
NTHEME  = 9
TH_SZ   = NATTR+48              ; a theme: its attributes, 16 colours' RGB
; menu.asm
MF_GRAY = 1
MF_SEP  = 2
MF_CHK  = 4                     ; a mark when its flag (mn_chk) is on
MF_SEL  = 8                     ; gray without a selection
MF_CLP  = 16                    ; gray with the Clipboard empty
MF_ED   = 32                    ; gray in the immediate window
MF_SUB  = 64                    ; opens a sub-menu, its command mn_it's index
NMENU   = 8
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
A_STEP  = 18
A_PSTEP = 19
A_TRACE = 20
A_BP    = 21
A_BPCLR = 22
A_SETNX = 23
A_RESTART = 24
A_SPLIT = 25
A_SUBS  = 26
A_NSUB  = 27
A_NFN   = 28
A_THQB  = 29
A_THDK  = 30
A_SYNHL = 31
A_VSPLIT = 32
A_TH2   = 33                    ; themes 2 .. NTHEME-1
A_SOUT  = A_TH2+NTHEME-2
A_NONE  = A_SOUT+1
; dlg.asm
CT_FLD  = 1
CT_LST  = 2
CT_BTN  = 3
CT_CHK  = 4
CT_TXT  = 5
CF_NOF  = 1                     ; takes no focus
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
    tx_n    dw      ?               ; the view's lines, from tx_v0 of tx_all
    tx_v0   dw      ?
    tx_all  dw      ?
    tx_hook dd      ?               ; a line added, removed (view.asm)
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
    ed_vw   db      ?               ; the view shown (view.asm)
    ed_r0   dw      ?               ; the window's first text row, its rows
    ed_rows dw      ?
    ed_x    db      ?               ; its frame's first column, its text's
    ed_tw   dw      ?               ; columns (the frame's less 2)
TEDIT    ends

TFIND    struc                   ; a line made canonical; Find and Change
    uc_pre  dd      ?
    uc_err  db      ?
    uc_eop  dd      ?               ; its ERR op
    uc_ecol dw      ?               ; the column it is at
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
    sy_cb   dd      ?               ; sy_walk's routine
TSYNC    ends

NVIEW   = 64                    ; views at most (view.asm)
VNAME   = 40                    ; a procedure's name's characters at most
VLSZ    = VNAME+4               ; the SUBs box's list item

TVIEW    struc                   ; a view: the module, a procedure
    vfirst  dw      ?               ; its first line in the text, its lines
    vn      dw      ?
    vkind   db      ?               ; 0 the module, VK_SUB, VK_FN
    vline   dw      ?               ; the cursor's place left there
    vtop    dw      ?
    vcol    dw      ?
    vname   db      VNAME+1 dup(?)
TVIEW    ends

TVIEWS   struc
    vw_n    db      ?
    vw_nlen dw      ?               ; vw_add's line's length
    vw_tab  TVIEW   NVIEW dup(<>)
TVIEWS   ends

TUDBG    struc                   ; the debugger's UI (ui/dbg.asm)
    stopd   db      ?               ; 1: stopped, CONT goes on (not after END)
    hl_on   db      ?               ; 1: hl_ln's columns hl_c0 to hl_c1 lit
    hl_ln   dw      ?
    hl_c0   dw      ?
    hl_c1   dw      ?
    ud_utl  dw      ?               ; F7's line + 1, 0 none
    ud_tgt  dw      ?               ; ud_numof's line, its number
    ud_num  dw      ?
TUDBG    ends

NWIN    = 4                     ; the windows, top down (win.asm)
W_HELP  = 0
W_ED1   = 1
W_ED2   = 2
W_IMM   = 3
W_DBG   = NWIN                  ; the debug pane, beside them: not in win

TWIN     struc                   ; a window
    wh      db      ?               ; its rows with its title, 0 hidden
    wcol    dw      ?               ; an edit window's state while not active
    wed     TEDIT   <>
TWIN     ends

TWINS    struc
    win     TWIN    NWIN dup(<>)
    wcols   db      ?               ; the edit windows' columns: 80, 50
    wact    db      ?               ; the active edit window
    wmax    db      ?               ; 1: Ctrl+F10's, wsav the heights before
    wvert   db      ?               ; 1: ED2 beside ED1, its rows ED1's
    wvsav   db      ?               ; wvert before Ctrl+F10
    wsav    db      NWIN dup(?)
TWINS    ends

DP_COL  = 50                    ; the debug pane's first column (dpane.asm)
DP_W    = 30                    ; its columns
DP_TW   = DP_W-2                ; a row's text
NDP     = 3                     ; its windows: Watch, Locals, Call stack
DP_STK  = 2
NDFRM   = 16                    ; the call stack's frames shown at most
NDVAR   = 64                    ; Locals' variables shown at most
NWATCH  = 8                     ; watches at most
WXLEN   = 41                    ; a watch's expression, 0-ended

TDFRM    struc                   ; a call stack's frame
    df_ln   dw      ?               ; its text line, FFFFh the immediate's
    df_fp   dd      ?               ; its procedure's cstk frame, 0 module
    df_vw   db      ?               ; its view, FFh none
    df_txt  db      DP_TW+1 dup(?)  ; its row
TDFRM    ends

TDPANE   struc                   ; the debug pane (dpane.asm)
    dp_on   db      ?               ; 1: shown, a stop
    dp_foc  db      ?               ; its window with the focus
    dp_frm  db      ?               ; the frame selected, 0 the stop's
    dp_nf   db      ?               ; the frames
    dp_sel  db      NDP dup(?)      ; each window's row selected
    dp_top  db      NDP dup(?)      ; its first row shown
    dp_row  db      NDP dup(?)      ; its title's row, its rows with it
    dp_h    db      NDP dup(?)
    dp_nw   db      ?               ; the watches: expressions, rows; kept
    dp_wx   db      NWATCH*WXLEN dup(?)
    dp_wv   db      NWATCH*DP_TW dup(?)
    dp_fld  TLBUF   <>              ; Ins's expression
    tt_ms   db      ?               ; 1: the tooltip by the mouse, at tt_pos
    tt_pos  dw      ?
    dp_vid  dw      NDVAR dup(?)    ; Locals' variables' ids
    dp_nv   db      ?               ; Locals' variables, their rows
    dp_vt   db      NDVAR*DP_TW dup(?)
    dp_fr   TDFRM   NDFRM dup(<>)
TDPANE   ends

TIMM     struc                   ; the immediate window
    im_n    dw      ?
    im_i    dw      ?
    im_top  dw      ?
    im_r0   dw      ?               ; the window's first row, its rows
    im_rows dw      ?
TIMM     ends

TLOOP    struc                   ; the key loop, the screen
    ideon   db      ?               ; the editor runs
    ui_foc  db      ?               ; 1: the immediate window, 2 the debug pane
    ui_help dd      ?               ; the status bar's help, or 0
    ui_pos  db      12 dup(?)
    ui_kf   db      ?
    alt_t   db      ?
    ui_rf   db      ?               ; a run: always wait for a key
    ui_cpos dw      ?
    ui_rpos dw      ?
    ui_ecode db      ?
    ui_eln  db      ?               ; 1: the cursor is at the error's line
    ui_wrap db      ?
    ul_new  db      ?
    dr_sk   db      ?
    dr_ln   dw      ?
    dr_ina  db      ?               ; 1: drawing an edit window without the focus
TLOOP    ends

TMENU    struc                   ; the menus
    mn_c    db      ?               ; the bar's menu
    mn_open db      ?
    mn_on   db      ?
    mn_d    db      ?
    mn_q    db      ?
    mn_sub  db      ?               ; 1: a sub-menu open
    mn_m    db      ?               ; the box shown: its mn_it index, its item,
    mn_i    db      ?               ; count, width, column, top row; mn_swap's
    mn_cnt  db      ?               ; 6 bytes
    mn_w    db      ?
    mn_l    db      ?
    mn_top  db      ?
    mn_sv   db      6 dup(?)        ; the parent's, a sub-menu open
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
    ls_v    TLIST   <>              ; the SUBs box's
TDIALOG  ends

TMOUSE   struc                   ; the mouse
    ms_on   db      ?
    ms_b    db      ?
    ms_ev   db      ?
    ms_pos  dw      ?
    ms_hov  db      ?               ; 1: a move is an event (the key loop's)
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
    msg_txt db      640 dup(?)      ; a message box's lines
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
    ui_ebuf db      600 dup(?)      ; an error's message, '|' between lines
    vw_ln   db      LNBUF dup(?)    ; vw_kind's line, 0-ended
    vw_new  db      LNBUF dup(?)    ; vw_add's
    vw_fld  TLBUF   <>              ; New SUB's name
    vw_list db      NVIEW*VLSZ dup(?)   ; the SUBs box's
    ud_txt  db      LNBUF dup(?)    ; ud_hlset's line, its body
    ud_tok  db      LNBUF*5 dup(?)  ; a digit becomes 5 bytes
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
    ud      TUDBG   <>
    dp      TDPANE  <>
    wn      TWINS   <>
    vw      TVIEWS  <>
    bf      TUIBUF  <>
TUI      ends
