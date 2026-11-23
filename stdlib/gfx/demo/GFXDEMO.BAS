' gfx's first slice: boxes bouncing at 8 bits, a frame each
' refresh. A key stops it.
import gfx.display, gfx.draw

const W = 320
const H = 200
const SIDE = 16
const BOXES = 12

type Box
    x as single
    y as single
    dx as single
    dy as single
    colour as integer
end type

declare sub start (b as Box, byval n as integer)
declare sub move (b as Box, byval dt as single)
declare sub show (b as Box)

dim fleet(1 to BOXES) as Box
randomize timer

for i = 1 to BOXES
    start fleet(i), i
next

gfx.display.open W, H, 8, gfx.display.VSYNC
was# = gfx.display.clock

do
    now# = gfx.display.clock
    dt! = now# - was#
    was# = now#
    gfx.draw.clear 1

    for i = 1 to BOXES
        move fleet(i), dt!
        show fleet(i)
    next

    fps% = 0
    if dt! > 0 then fps% = 1 / dt!
    gfx.draw.text 4, 4, "gfx" + str$(fps%) + " fps", 15
    gfx.display.flip
loop while inkey$ = ""

gfx.display.close

' Box n somewhere on the screen, heading anywhere.
sub start (b as Box, byval n as integer)
    b.x = rnd * (W - SIDE)
    b.y = rnd * (H - SIDE)
    b.dx = 40 + rnd * 80
    b.dy = 40 + rnd * 80
    b.colour = 32 + n * 8
end sub

' dt seconds on, turned back off the screen's edges.
sub move (b as Box, byval dt as single)
    b.x = b.x + b.dx * dt
    b.y = b.y + b.dy * dt

    if b.x < 0 then b.dx = abs(b.dx)
    if b.x > W - SIDE then b.dx = -abs(b.dx)
    if b.y < 0 then b.dy = abs(b.dy)
    if b.y > H - SIDE then b.dy = -abs(b.dy)
end sub

sub show (b as Box)
    x2! = b.x + SIDE
    y2! = b.y + SIDE
    gfx.draw.rect b.x, b.y, x2!, y2!, b.colour, gfx.draw.FILL
    gfx.draw.rect b.x, b.y, x2!, y2!, 15, 0
    gfx.draw.line b.x, b.y, x2!, y2!, 15
end sub
