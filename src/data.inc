; ==========================================================================
; data.inc - the variables, zeroed at start
; ==========================================================================

        .data?
bss_s   label   byte
cty     db      ?                ; the compiled expression's type: T_INT ..
hasfpu  db      ?
cll     dd      ?                ; the last INT load the compiler emitted
clle    dd      ?                ; its end
ccmp    dd      ?                ; the last compare
ccmpe   dd      ?                ; its end
ccnt    db      ?
ctv     db      ?                ; the target: id, then sinfo
ctk     db      ?
ctn     db      ?
ctgt    db      ?
cinf    db      ?                ; cm_infer's pass is on
cinfc   db      ?                ; a pass demoted a variable
cinfv   db      ?                ; the FOR variable being compiled, in a pass
cfk     db      ?
bop     db      ?
lk      db      ?
lpv     dw      ?
arrop   db      ?
vid     db      ?
narr    db      ?
s0v     dw      ?
s1v     dw      ?
sln     dw      ?
sn      dw      ?
sbx     db      ?
dslin   dd      ?                ; DS:0, linear
tmplin  dd      ?                ; tmpd, linear
proglin dd      ?                ; PROGBASE, linear
pglin   dd      ?                ; the program's segment, linear
hbot    dd      ?                ; the heap
htop    dd      ?
gq      dd      ?
gd      dd      ?
asz     dd      ?
ahd     dd      ?                ; past the array headers
peekseg dw      ?
ocap    db      ?
pcol    db      ?
lc      dw      ?
xo      db      ?
linked  db      ?
ctxlin  dd      ?                ; linear address of body offset 0, for literals
cend    dd      ?                ; the cache's next byte, past the program
ntsz    db      ?                ; cm_native's op size
inend   dd      ?                ; where the INPUT in hand ends
elim    dd      ?                ; cm_xlate's limit
eeol    dd      ?                ; the EOL element
efn     dd      ?                ; a DEF's length, to fill in
einb    dd      ?                ; an INPUT's end, to fill in
pbs     dd      ?                ; prg_put's body
pbl     dw      ?
if IDE
include ui/uidata.inc
endif
cfail   db      ?
cxb     dd      ?                ; where the compiler writes
cxlim   dd      ?
ctks    dd      ?                ; its keyword
cfv     db      ?
iredo   db      ?
lq      db      ?
onk     db      ?
contsi  dw      ?
tsp     dw      ?                ; the temporaries in use, DSZ each
stpflg  db      ?
opip    dd      ?                ; the op dispatch ran last
fid     db      ?
fnid    db      ?
fnpid   db      ?
csp     dd      ?
ln      dw      ?
vpt     dw      ?
lyo2    dw      ?
pend    dw      ?
vend    dd      ?
aend    dd      ?
sfr     dd      ?
dlp     dd      ?
dcp     dw      ?
dyo     db      ?                ; 1: dlp is before a DATA's first item
cth     db      ?                ; the statement follows a THEN
rdsv    dw      ?
contip  dw      ?
rsp     dd      ?
cstk    dd      ?                ; the control stack's top frame
cop     dw      ?
fop     dw      ?
stv     dd      ?
aret    dd      ?
cnarg   db      ?
flim    dd      ?
fstep   dd      ?
runl    dw      ?
stklim  dd      ?
cplim   dd      ?                ; cm_p's stklim
spt     dd      ?
bpo     dd      ?
dpv     dd      ?
sac     dd      ?
sab     dd      ?
aid     dw      ?
lnum    dw      ?
fyv     dw      ?
fvp     dd      ?
fnbody  dw      ?
sysv    dd      ?
lfrom   dw      ?
lto     dw      ?
lsv     dw      ?
inlen   db      ?
inpos   db      ?
iny0    dd      ?
infl    db      ?
isw_si  dd      ?
wv      dw      ?
onp     dd      ?
cst     dd      ?
ctok    db      ?
cfid    db      ?
lxst    db      ?                ; the tokenizer's state
lxf     db      ?                ; the number is text
nkind   db      ?
nsyms   db      ?
lxn     dd      ?
lxs     dd      ?
nkey    dw      ?
nlenp   dd      ?
lxv     dd      ?                ; a whole number's value
lxm     dd      ?                ; a name's end
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
fsv     dd      ?
fln     db      ?
fdv     db      ?
fsa     db      ?
iost    db      ?
cin     db      ?
isdev   db      ?
fb      db      ?
inh     dw      ?
outh    dw      ?
oblen   dw      ?
toff    dd      ?
fdev    db      NFILE dup(?)
flfn    db      NFILE dup(?)
ffd     db      NFILE dup(?)
fmd     db      NFILE dup(?)
fstt    db      NFILE dup(?)
flk     db      NFILE dup(?)
fnam    db      17 dup(?)
tmpd    db      DSZ*NTMP dup(?)
nbuf    db      257 dup(?)
dlb     db      256 dup(?)
rdbuf   db      256 dup(?)
dimbuf  dw      64 dup(?)
cntbuf  dw      64 dup(?)
vt      dd      256 dup(?)
skey    dw      256 dup(?)
sinfo   db      256 dup(?)
sbuf    db      256 dup(?)
obuf    db      OBSZ dup(?)
inbuf   db      256 dup(?)
cbufh   db      ?                ; cbuf-1: lx_line's line before the body
cbuf    db      256 dup(?)
xbuf    db      264 dup(?)
lbuf    db      264 dup(?)
bss_e   label   byte
        .code
