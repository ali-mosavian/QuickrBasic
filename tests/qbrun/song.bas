' A song built by hand and played: two sounds on one step,
' a chord, a loop. beat() counts on through the loop;
' music stop fades it out.
type event
    at as integer
    length as integer
    kind as integer
    pitch as integer
    loud as integer
end type

const last = 7
const wait_most = 5

instrument 1, "W4 Z1,0 H0,4,0,4 V12"
instrument 2, "W3 Y2,3000,2 H0,2,0,2 V10"
instrument 3, "W1 H2,4,10,6 V9"

dim ev(last) as event
for i = 0 to last
    read ev(i).at, ev(i).length, ev(i).kind
    read ev(i).pitch, ev(i).loud
next i

' at, length, kind (0 a tempo), pitch (or tempo), loud
data 0, 0, 0, 240, 0
' a kick and a hat on one step
data 0, 48, 1, 25, 100
data 0, 24, 2, 61, 80
data 48, 24, 2, 61, 60
' a chord: C, E, G
data 96, 96, 3, 37, 70
data 96, 96, 3, 41, 70
data 96, 96, 3, 44, 70
data 144, 24, 2, 61, 60

' two quarters, then the second again
music ev(), 96
began = timer
do while beat() < 5
    if timer - began > wait_most then exit do
loop
print "looped"; beat() >= 5

note 1, 25, 24, 100
music stop, 48
began = timer
do while beat() > 0
    if timer - began > wait_most then exit do
loop
print "stopped"; beat() = 0
