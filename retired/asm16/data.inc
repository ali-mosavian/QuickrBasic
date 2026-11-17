; ==========================================================================
; data.inc - the variables, zeroed at start
; ==========================================================================

DBSS    segment
bss_s   label   byte
cty     db      ?                ; the compiled expression's type: T_INT ..
hasfpu  db      ?
cll     dw      ?                ; the last INT load the compiler emitted
clle    dw      ?                ; its end
ccmp    dw      ?                ; the last compare
ccmpe   dw      ?                ; its end
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
ahd     dw      ?                ; past the array headers
peekseg dw      ?
ocap    db      ?
pcol    db      ?
lc      dw      ?
xo      db      ?
linked  db      ?
ctxlin  dd      ?                ; linear address of body offset 0, for literals
cend    dw      ?                ; the cache's next byte, past the program
ntsz    db      ?                ; cm_native's op size
inend   dw      ?                ; where the INPUT in hand ends
elim    dw      ?                ; cm_xlate's limit
eeol    dw      ?                ; the EOL element
efn     dw      ?                ; a DEF's length, to fill in
einb    dw      ?                ; an INPUT's end, to fill in
dcode   db      1536 dup(?)      ; the direct line's stream
pbs     dw      ?                ; prg_put's body
pbl     dw      ?
if IDE
include ui/uidata.inc
endif
cfail   db      ?
cxb     dw      ?                ; where the compiler writes
cxlim   dw      ?
ctks    dw      ?                ; its keyword
cfv     db      ?
iredo   db      ?
lq      db      ?
onk     db      ?
contsi  dw      ?
tsp     dw      ?                ; the temporaries in use, DSZ each
stpflg  db      ?
fid     db      ?
fnid    db      ?
fnpid   db      ?
csp     dw      ?
ln      dw      ?
vpt     dw      ?
lyo2    dw      ?
pend    dw      ?
vend    dw      ?
aend    dd      ?
sfr     dd      ?
dlp     dw      ?
dcp     dw      ?
dyo     db      ?                ; 1: dlp is before a DATA's first item
cth     db      ?                ; the statement follows a THEN
rdsv    dw      ?
contip  dw      ?
rsp     dw      ?
cstk    dw      ?                ; the control stack's top frame
cop     dw      ?
fop     dw      ?
stv     dd      ?
aret    dw      ?
cnarg   db      ?
flim    dd      ?
fstep   dd      ?
runl    dw      ?
stklim  dw      ?
spt     dd      ?
bpo     dd      ?
dpv     dd      ?
sac     dd      ?
sab     dd      ?
aid     dw      ?
lnum    dw      ?
fyv     dw      ?
fvp     dw      ?
fnbody  dw      ?
sysv    dd      ?
lfrom   dw      ?
lto     dw      ?
lsv     dw      ?
inlen   db      ?
inpos   db      ?
iny0    dw      ?
infl    db      ?
isw_si  dw      ?
wv      dw      ?
spc     dw      ?
onp     dw      ?
cst     dw      ?
ctok    db      ?
cfid    db      ?
lxd     db      ?
lxe     db      ?
lxf     db      ?
nkind   db      ?
nsyms   db      ?
lxn     dw      ?
lxs     dw      ?
nkey    dw      ?
nlenp   dw      ?
fx      db      ?
ff      db      ?
fq      db      ?
fd      db      ?
fsg     db      ?
fout    dd      ?
fw      dd      ?
fdw     dw      ?
fbuf    db      7 dup(?)
ft1     dd      ?
ft2     dd      ?
ft3     dd      ?
ft4     dd      ?
fipt    dw      ?
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
vt      dw      256 dup(?)
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
DBSS    ends
