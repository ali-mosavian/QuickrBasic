' What each kind of thing looks like: a row a kind, read
' from CHARLIE.DAT, where charlieart.py writes them.
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

' A kind's picture, its box from the thing's place, and its
' frames: none, and the kind draws itself.
type Kind
    pic as integer
    dx as integer
    dy as integer
    w as integer
    h as integer
    frames as integer
    by as integer
    rate as integer
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
    next

    close #df.num
end sub
