' What each kind of thing looks like and what is asked of
' it: a row a kind, read from CHARLIE.DAT, where
' charlieart.py writes them.
defint A-Z
option explicit

from engine.datafile import DataFile, OpenData, Word

' How a row picks its frame: none; by the tick, rate ticks
' a frame; the same, each thing a frame on from the last; by
' its state; pressed, its state above 0.
const FNONE = 0
const FTICK = 1
const FSWAY = 2
const FSTATE = 3
const FPRESS = 4

' How a thing is moved: not at all; by its kind's own rule;
' prowling, turned where its beat ends; resting, its state
' counted down to 0.
const MNONE = 0
const MOWN = 1
const MPROWL = 2
const MREST = 3

' A kind's picture, its box from the thing's place, and its
' frames: none, and the kind draws itself. The state it
' starts a stage in; whether it keeps its own, rides a
' track, hangs where a bark looses it, is crushed by a fall,
' grows where the ground is laid. How it is moved.
type Kind
    pic as integer
    dx as integer
    dy as integer
    w as integer
    h as integer
    frames as integer
    by as integer
    rate as integer
    start as integer
    keeps as integer
    rides as integer
    hangs as integer
    crushed as integer
    grows as integer
    mind as integer
end type

' The kinds, by their numbers.
type Kinds
    k() as Kind
end type

' The rows of df, at the byte its third LONG names.
sub LoadKinds (kd as Kinds, df as DataFile)
    dim i as integer, n as integer

    OpenData df
    seek #df.num, 9
    seek #df.num, Word&(df)
    n = Word&(df)
    redim kd.k(n - 1)

    for i = 0 to n - 1
        kd.k(i).pic = Word&(df)
        kd.k(i).dx = Word&(df)
        kd.k(i).dy = Word&(df)
        kd.k(i).w = Word&(df)
        kd.k(i).h = Word&(df)
        kd.k(i).frames = Word&(df)
        kd.k(i).by = Word&(df)
        kd.k(i).rate = Word&(df)
        kd.k(i).start = Word&(df)
        kd.k(i).keeps = Word&(df)
        kd.k(i).rides = Word&(df)
        kd.k(i).hangs = Word&(df)
        kd.k(i).crushed = Word&(df)
        kd.k(i).grows = Word&(df)
        kd.k(i).mind = Word&(df)
    next

    close #df.num
end sub
