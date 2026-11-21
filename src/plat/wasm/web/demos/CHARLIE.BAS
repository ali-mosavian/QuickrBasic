' CHARLIE BARKLEY: Charlie the Maltese tears through the
' Amber Downs. Arrows run. Space, Z or Up jumps, curled.
' Down while running rolls; Down while still crouches: jump
' to wind up, let Down go to launch. M mutes, N turns the
' tune off; Esc leaves. SCREEN 13 on two pages: one drawn,
' one shown. Held keys come from the keyboard's port (60h);
' where it reads FFh, from INKEY$'s repeats. Physics in
' pixels and seconds, 60 steps a second; the pictures are
' DATA, drawn by tools/charlieart.py. First the intro, how
' the chase began: any key skips it. The game is the barkley
' package beside this file; barkley/main.bas runs it.
defint A-Z

from barkley.main import Allot, Main
from barkley.model.world import World

dim wd as World
Allot wd

Main wd
end
