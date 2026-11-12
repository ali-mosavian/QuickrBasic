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
    wa      dw      ?               ; a jump's line, a label's
    onp     dd      ?
    cst     dd      ?
    ctok    db      ?
    cfid    dw      ?
    csp     dd      ?
    cbase   dd      ?               ; the body's start
    cline   dw      ?               ; its line, DLINE direct
    nst     db      ?               ; statements recorded (g_stb, g_stx)
    csif    db      ?               ; in a one-line IF
    cscope  db      ?               ; var_find's scope: 0 the module, a procedure
    cstat   db      ?               ; var_find makes a STATIC, not a local
    cproc   db      ?               ; the procedure the compiled line is in
TCOMP    ends

TLINK    struc                   ; the linker and the cache
    cend    dd      ?               ; the cache's next byte, past the program
    elim    dd      ?               ; cm_xlate's limit
    eeol    dd      ?               ; the EOL element
    efn     dd      ?               ; a DEF's length, to fill in
    einb    dd      ?               ; an INPUT's end, to fill in
    linked  db      ?
    lc      dw      ?
    nblk    dw      ?               ; block entries (comp/block.asm)
    bsp     dw      ?               ; their scan's stack depth
    bln     dw      ?               ; the line it scans
    nfl     db      ?               ; cache flushes, mod 256
    emap    dd      ?               ; cm_xlate's statement map
    ecode   dd      ?               ; the code after it
    ek      db      ?               ; the next statement it maps
    nprc    dw      ?               ; procedures + 1 (g_prc)
    nppar   dw      ?               ; their parameters (g_ppar)
    cprc    dw      ?               ; the procedure the scan is in, 0 none
TLINK    ends

TLEX     struc                   ; the tokenizer
    lxst    db      ?               ; the tokenizer's state
    lxf     db      ?               ; the number is text
    nsyms   dw      ?               ; the next variable
    nnams   dw      ?               ; the next name
    nshr    dw      ?               ; SHARED, STATIC names (g_shr)
    dproc   dw      ?               ; dc_line: the procedure being entered, its name
    ntyps   dw      ?               ; TYPEs (comp/decl.asm), the last's index
    nflds   dw      ?               ; their fields
    dtype   dw      ?               ; dc_line: the TYPE being entered
    nlen    db      ?               ; nam_find's name: its length, its text
    nptr    dd      ?
    nsfree  dd      ?               ; the names' next byte
    lxn     dd      ?
    lxs     dd      ?
    nkey    dw      ?
    nlenp   dd      ?
    lxv     dd      ?               ; a whole number's value
    lxm     dd      ?               ; a name's end
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
    contip  dw      ?
    contsi  dw      ?
    ln      dw      ?
    lnum    dw      ?
    vpt     dw      ?
    fid     dw      ?
    onk     db      ?
    fvp     dd      ?
    fsv     dd      ?
    iredo   db      ?
    stpflg  db      ?
    dlp     dd      ?
    dcp     dw      ?
    dyo     db      ?               ; 1: dlp is before a DATA's first item
    inend   dd      ?               ; where the INPUT in hand ends
    iny0    dd      ?
    isw_si  dd      ?
    lfrom   dw      ?
    lto     dw      ?
    fop     dw      ?
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
    proglin dd      ?               ; PROGBASE, linear
    pglin   dd      ?               ; the program's segment, linear
    hbot    dd      ?               ; the heap
    htop    dd      ?
    asz     dd      ?
    ahd     dd      ?               ; past the array headers
    aid     dw      ?
    aend    dd      ?
    sfr     dd      ?
    vend    dd      ?
    pend    dw      ?
    lyo2    dw      ?
    pbs     dd      ?               ; prg_put's body
    pbl     dw      ?
    peekseg dw      ?
TMEM     ends

TIO      struc                   ; input and output
    fline   dw      ?               ; loading a file: its line, for a warning
    ccr     db      ?               ; plt_getline: a file's line ended at a CR
    ocap    db      ?
    pcol    db      ?
    sbx     db      ?
    inlen   db      ?
    inpos   db      ?
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

TFILES   struc                   ; the open files, by number
    fdev    db      NFILE dup(?)
    flfn    db      NFILE dup(?)
    ffd     db      NFILE dup(?)
    fmd     db      NFILE dup(?)
    fstt    db      NFILE dup(?)
    flk     db      NFILE dup(?)
TFILES   ends

TFLT     struc                   ; the float routines
    fx      db      ?
    ff      db      ?
    fq      db      ?
    fd      db      ?
    fsg     db      ?
    fout    dd      ?
    fw      dd      ?
    fdw     dd      ?
    fbuf    db      7 dup(?)
    ft1     dd      ?
    ft2     dd      ?
    ft3     dd      ?
    ft4     dd      ?
    fipt    dd      ?
    fcnt    dw      ?
    flog    dw      ?
    rs      dd      ?
    sqa     dd      ?
    sqg     dd      ?
    sqn     db      ?
    fpa     dd      ?
    fpb     dd      ?
    fpw     dw      ?
    fpi     dd      ?
TFLT     ends

TPLAT    struc                   ; the platform: kept over the zeroing
    dsv     dw      ?               ; DS, for code that runs without it
    pspsel  dw      ?               ; the PSP
    old23   df      ?               ; the vectors plt_init replaced
    old1c   df      ?
TPLAT    ends

TSTATE   struc
    cm      TCOMP   <>
    lnk     TLINK   <>
    lx      TLEX    <>
    rn      TRUN    <>
    sg      TSTR    <>
    mm      TMEM    <>
    io      TIO     <>
    fl      TFILES  <>
    ft      TFLT    <>
TSTATE   ends
