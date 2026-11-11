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
    ctv     db      ?               ; the target: id, then sinfo
    ctk     db      ?
    ctn     db      ?
    cinf    db      ?               ; cm_infer's pass is on
    cinfc   db      ?               ; a pass demoted a variable
    cinfv   db      ?               ; the FOR variable being compiled, in a pass
    cfk     db      ?
    xo      db      ?
    ctxlin  dd      ?               ; linear address of body offset 0, for literals
    cfail   db      ?
    cxb     dd      ?               ; where the compiler writes
    cxlim   dd      ?
    ctks    dd      ?               ; its keyword
    cth     db      ?               ; the statement follows a THEN
    wv      dw      ?
    onp     dd      ?
    cst     dd      ?
    ctok    db      ?
    cfid    db      ?
    csp     dd      ?
TCOMP    ends

TLINK    struc                   ; the linker and the cache
    cend    dd      ?               ; the cache's next byte, past the program
    elim    dd      ?               ; cm_xlate's limit
    eeol    dd      ?               ; the EOL element
    efn     dd      ?               ; a DEF's length, to fill in
    einb    dd      ?               ; an INPUT's end, to fill in
    linked  db      ?
    lc      dw      ?
TLINK    ends

TLEX     struc                   ; the tokenizer
    lxst    db      ?               ; the tokenizer's state
    lxf     db      ?               ; the number is text
    nkind   db      ?
    nsyms   db      ?
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
    cplim   dd      ?               ; cm_p's stklim
    rsp     dd      ?
    cstk    dd      ?               ; the control stack's top frame
    contip  dw      ?
    contsi  dw      ?
    ln      dw      ?
    lnum    dw      ?
    vpt     dw      ?
    fid     db      ?
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
    hasfpu  db      ?
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
