# os Module
keys: ~quickr os, ~quickr os.env, ~quickr os.exit, ~quickr os.args

The program's command line and environment.

    args() as string[]             the words after the program's name
    env(name) as string            an environment variable, "" none
    exit()                         the program ended, files closed

See also [Quickr](quickr index), [fs](quickr fs)

## Details
exit() runs no drop. END has no exit code; neither has exit().

## Example
    ' OPTION DIALECT QUICKR
    import os
    for a in os.args()
        print(a)
    end for
    print(os.env("PATH"))
