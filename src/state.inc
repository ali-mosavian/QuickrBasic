; ==========================================================================
; state.inc - the core's state: a structure per part, all in g_state (main.asm)
; ==========================================================================

TCOMP    struc                   ; the compiler
    cty     db      ?               ; the compiled expression's type: T_INT ..
    cll     dd      ?               ; the last INT load the compiler emitted
    clle    dd      ?               ; its end
    ccmp    dd      ?               ; the last compare
    ccmpe   dd      ?               ; its end
    ccnt    db      ?
    ctv     dw      ?               ; the target: id, then sinfo
    ctk     db      ?
    ctn     db      ?
    cfk     db      ?
    xo      db      ?
    ctxlin  dd      ?               ; linear address of body offset 0, for literals
    cxb     dd      ?               ; where the compiler writes
    cxlim   dd      ?
    ctks    dd      ?               ; its keyword
    cth     db      ?               ; the statement follows a THEN
    wv      dw      ?
    jline   dd      ?               ; a jump's line, a label's
    onp     dd      ?
    cst     dd      ?
    ctok    db      ?
    cfid    dw      ?
    csp     dd      ?
    cbase   dd      ?               ; the body's start
    line    dd      ?               ; its line, DLINE direct
    nstmt   dd      ?               ; statements recorded (cm_mapstmt)
    cmap    dd      ?               ; the map's top: its entries grow down
    csif    db      ?               ; in a one-line IF
    cscope  db      ?               ; var_find's scope: 0 the module, a procedure
    cstat   db      ?               ; var_find makes a STATIC, not a local
    credim  dw      ?               ; 40h: DIM's grammar is REDIM's; 100h PRESERVE
    cchk    db      ?               ; 1: the IDE's line check, unlinked
    cdbg    dd      ?               ; past the statement's DBG, 0 none
    cproc   db      ?               ; the procedure the compiled line is in
    cdg     db      ?               ; the error's DG_ (io/msg.inc), 0: by its text
    cdfa    dw      ?               ; the automaton's state it failed in, FFFFh none
TCOMP    ends

TLINK    struc                   ; the linker and the cache
    cend    dd      ?               ; the cache's next byte, past the program
    elim    dd      ?               ; cm_xlate's limit
    eeol    dd      ?               ; the EOL element
    einb    dd      ?               ; an INPUT's end, to fill in
    linked  db      ?
    cursor  dd      ?
    nblk    dw      ?               ; block entries (comp/block.asm)
    bsp     dw      ?               ; their scan's stack depth
    scanln  dd      ?               ; the line it scans
    nfl     db      ?               ; cache flushes, mod 256
    emap    dd      ?               ; cm_xlate's statement map
    ecode   dd      ?               ; the code after it
    mapnext dd      ?               ; the next statement it maps
    nprc    dw      ?               ; procedures + 1 (g_prc)
    nppar   dw      ?               ; their parameters (g_ppar)
    cprc    dw      ?               ; the procedure the scan is in, 0 none
TLINK    ends

TLEX     struc                   ; the tokenizer
    lxst    db      ?               ; the tokenizer's state
    lxf     db      ?               ; the number is text
    nsyms   dw      ?               ; the next variable
    nnams   dw      ?               ; the next name
    dproc   dw      ?               ; dc_line: the procedure being entered, its name
    ntyps   dw      ?               ; TYPEs (comp/decl.asm), the last's index
    nflds   dw      ?               ; their fields
    dtype   dw      ?               ; dc_line: the TYPE being entered
    nlen    db      ?               ; nam_find's name: its length, its text
    nptr    dd      ?
    lxn     dd      ?
    lxs     dd      ?
    nkey    dw      ?
    nlenp   dd      ?
    lxv     dd      ?               ; a whole number's value
    lxm     dd      ?               ; a name's end
    lxsrc   dd      ?               ; lx_line's text and body, for lxcol
    lxdst   dd      ?
TLEX     ends

TRUN     struc                   ; the executor
    narr    db      ?
    lk      db      ?
    opip    dd      ?               ; the op dispatch ran last
    cop     dw      ?
    stv     dd      ?
    aret    dd      ?
    cnarg   db      ?
    flim    dd      ?
    fstep   dd      ?
    runl    dw      ?
    stklim  dd      ?
    cplim   dd      ?               ; cm_p's stklim: CPROOM under the compile's SP
    rsp     dd      ?
    cstk    dd      ?               ; the control stack's top frame
    lsp     dd      ?               ; the save stack's top (g_lsav)
    ppip    dd      ?               ; PCALL: its ip, its entries' end
    ppend   dd      ?
    pres    dd      ?               ; PRET: a FUNCTION's INT or FLOAT result
    contln  dd      ?
    contcode dd      ?
    line    dd      ?
    lnum    dw      ?
    runln   dd      ?
    fid     dw      ?
    onk     db      ?
    fvp     dd      ?
    fsv     dd      ?
    iredo   db      ?
    stpflg  db      ?
    dlp     dd      ?
    dataat  dd      ?
    dyo     db      ?               ; 1: dlp is before a DATA's first item
    inend   dd      ?               ; where the INPUT in hand ends
    iny0    dd      ?
    isw_si  dd      ?
    lfrom   dw      ?
    lto     dw      ?
    fop     dw      ?
    onerr   dd      ?               ; ON ERROR's line, 0 none
    inerr   db      ?               ; 1: in the handler, till RESUME
    errn    dw      ?               ; ERR, ERL
    erl     dw      ?
    uerr    dw      ?               ; ERROR n's n
    resln   dd      ?               ; RESUME's line, its statement's code offset,
    resat   dd      ?               ; the next's (FFFFFFFFh: the next line)
    resnext dd      ?
TRUN     ends

TSTR     struc                   ; strings
    s0v     dw      ?
    s1v     dw      ?
    sln     dw      ?
    sn      dw      ?
    gq      dd      ?
    gd      dd      ?
    spt     dd      ?
    bpo     dd      ?
    dpv     dd      ?
    sac     dd      ?
    sab     dd      ?
    tsp     dw      ?               ; the temporaries in use, DSZ each
    tspb    dw      ?               ; the procedure's floor of tsp
    tmplin  dd      ?               ; tmpd, linear
TSTR     ends

TMEM     struc                   ; the memory map, the program, the arrays
    dslin   dd      ?               ; DS:0, linear
    progfirst dd      ?               ; PROGBASE, linear
    progbase dd      ?               ; the program's block, its base
    progsel dw      ?               ; its selector, in GS
    cachebase dd      ?               ; the cache's block, its base
    hbot    dd      ?               ; the heap
    htop    dd      ?
    asz     dd      ?
    ahd     dd      ?               ; past the array headers
    aid     dw      ?
    aend    dd      ?
    sfr     dd      ?
    progend dd      ?
    enterat dd      ?               ; the stored line being entered
    bodyptr dd      ?               ; prg_put's body
    bodylen dd      ?
    preptr  dd      ?               ; its text before the body; used up
    prelen  dd      ?
    addfl   db      ?               ; the lines stored: their fl
    addat   dd      ?               ; prg_add's place, 0 the end
    peekseg dw      ?
TMEM     ends

INMAX   = 32767                 ; an input line's characters at most, as QBasic's
SBMAX   = 0FFF0h                ; sbuf: a line's LIST spelled, the rest dropped

TIO      struc                   ; input and output
    fline   dw      ?               ; loading a file: its line, for a warning
    ocap    db      ?
    pcol    db      ?
    sbx     dd      ?               ; sbuf's bytes, to SBMAX
    inlen   dd      ?
    inpos   dd      ?
    infl    db      ?
    lsv     dw      ?
    cin     db      ?
    isdev   db      ?
    fb      db      ?
    inh     dw      ?
    outh    dw      ?
    oblen   dw      ?
    toff    dd      ?
    fln     db      ?
    fdv     db      ?
    fsa     db      ?
    iost    db      ?
    lmode   db      ?               ; loading a file: every line is stored
    batch   db      ?               ; a file run: its end exits
    fnam    db      17 dup(?)
    lq      db      ?
TIO      ends

TCHAN    struc                   ; a channel (io/chan.asm)
    cnum    db      ?               ; its file number, 0 free
    ckind   db      ?               ; CK_*
    cmode   db      ?               ; FM_*
    ccol    db      ?               ; its output column while another is used
    chnd    dw      ?               ; the DOS handle
    creclen dw      ?               ; RANDOM's record length
    cbn     dw      ?               ; bytes in cbuf
    cbp     dw      ?               ; read ahead: the next one
    cdirty  db      ?               ; cbuf holds bytes to write
    ceof    db      ?               ; GET read past the end
    cpos    dd      ?               ; the file's offset of cbuf
    clast   dd      ?               ; LOC: the last record, or byte
    cfld    dw      ?               ; FIELD's next offset
    cbuf    db      CBSZ dup(?)
TCHAN    ends

TFLT     struc                   ; the float routines
    rs      dd      ?
    fpa     dd      ?
    fpb     dd      ?
    fpw     dw      ?
    fpi     dd      ?
TFLT     ends

TPLAT    struc                   ; the platform: kept over the zeroing
    dsv     dw      ?               ; DS, for code that runs without it
    dsbase  dd      ?               ; DS's base, linear
    pspsel  dw      ?               ; the PSP
    old23   df      ?               ; the vectors plt_init replaced
    old1c   df      ?
TPLAT    ends

TDBG     struc                   ; the debugger (vm/dbg.asm)
    mode    db      ?               ; DM_*: the stop the next DBG looks for
    trace   db      ?               ; 1: each statement to hook (TRON)
    bpon    db      ?               ; 1: a line's LF_BP stops
    utl     db      ?               ; 1: until, a one-time stop at line uline
    emit    db      ?               ; 1: a DBG op starts each statement
    pdep    dd      ?               ; DM_PSTEP: stop at this depth or less
    uline   dd      ?
    hook    dd      ?               ; trace's routine, 0 none
    wline   dd      ?               ; dbg_pos: the line
    stmtfrom dd      ?               ;   its statement's body offsets, from
    stmtto  dd      ?               ;   and to (BOFF the line's end)
TDBG     ends

TALLOC   struc                   ; a block (mem/alloc.asm)
    base    dd      ?               ; its first byte
    cap     dd      ?               ; its bytes
    moved   dd      ?               ; called when it moves or grows, 0 none
TALLOC   ends

NBLKH   = 8                     ; blocks at most
HCACHE  = 0                     ; the code cache's block
HPROG   = 1                     ; the program store's
HLMAP   = 2                     ; the editor's line map (ui/lmap.asm)

TMAXA   = 4                     ; a table's arrays at most

TTAB     struc                   ; a table (mem/tab.asm)
    hnd     dd      ?               ; its block
    cap     dd      ?               ; its entries
    max     dd      ?               ;   at most
    base    dd      ?               ; the block's base when placed
    narr    dd      ?               ; its arrays
    sz      dd      TMAXA dup(?)    ; each one's bytes per entry
    at      dd      TMAXA dup(?)    ; each one's first byte
TTAB     ends

TVAR     struc                   ; a variable, at FS:id*8 (mem/sym.asm)
    vt      dd      ?               ; its cell; an array's header
    nam     dw      ?               ; its name
    typ     db      ?               ; ET_*
    info    db      ?               ; bit 7 an array, SK_DBL, bits 1-0 the kind
TVAR     ends

TVX      struc                   ; a variable's rest, at vars.at[8]+id*8
    next    dw      ?               ; its hash chain's next
    nloc    dw      ?               ; the next local of its procedure
    scp     db      ?               ; its scope: 0 the module, a procedure
    dyn     db      ?               ; REDIM made it; ERASE frees it
    kop     db      ?               ; a CONST's literal load, its operand
            db      ?               ;   in kval; 0 none (comp/fold.asm)
TVX      ends

TNAM     struc                   ; a name, at nams.at+id*8
    text    dd      ?               ; its text's offset in ntext
    flag    db      ?               ; NM_*
    typ     db      ?               ; its TYPE, 0 none
    prc     db      ?               ; its procedure, 0 none
            db      ?
TNAM     ends

TSYM     struc                   ; the variables and names (mem/sym.asm)
    vars    TTAB    <>              ; cells, TVAR, TVX, kval
    nams    TTAB    <>              ; TNAM, the hash chains' next
    ntext   TTAB    <>              ; the names' text
    tfree   dd      ?               ; ntext's next byte, an offset
    vsel    dw      ?               ; FS: the TVARs
TSYM     ends

TALLOCS  struc                   ; the allocator
    nb      dd      ?               ; the blocks made
    tab     TALLOC  NBLKH dup(<>)
TALLOCS  ends

TSTATE   struc
    alloc   TALLOCS <>
    cm      TCOMP   <>
    lnk     TLINK   <>
    lx      TLEX    <>
    rn      TRUN    <>
    sg      TSTR    <>
    mm      TMEM    <>
    io      TIO     <>
    ft      TFLT    <>
    dbg     TDBG    <>
    sym     TSYM    <>
TSTATE   ends

vxat    macro   reg                     ; REG the TVXs (mem/sym.asm)
        mov     reg,[g_state.sym.vars.at+8]
endm

cellof  macro   reg,id                  ; REG variable ID's own cell
        mov     reg,[g_state.sym.vars.at]
        lea     reg,[reg+id*8]
endm

nmat    macro   reg                     ; REG the TNAMs
        mov     reg,[g_state.sym.nams.at]
endm

nmtest  macro   id,bits                 ; ZF: name ID's flag has none of BITS
        push    edi
        nmat    edi
        test    [edi+id*8].TNAM.flag,bits
        pop     edi
endm
