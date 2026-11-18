# ON and OFF
keys: ON, OFF

ON starts three different statements. Pick by what follows it:

  [ON ERROR](ON ERROR)                  jump to a handler on a run-time error
  [ON expr GOTO, ON expr GOSUB](ON GOTO)  jump by the value of a number
  [ON event GOSUB](ON event)             call a subroutine when an event happens

OFF is a word, not a statement. It switches an event off, or hides
the soft-key line:

    KEY(n) OFF       TIMER OFF       STRIG(n) OFF       PEN OFF
    PLAY OFF         KEY OFF (hides the soft-key line)

An event has three states: ON (watched, handled), OFF (ignored, not
remembered) and STOP (remembered, handled once switched back ON).

See also [ON ERROR](ON ERROR), [ON GOTO](ON GOTO), [ON event](ON event)
