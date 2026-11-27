# os Module
keys: ~quickr os, ~quickr os.env, ~quickr os.exit, ~quickr os.run, ~quickr os.args

The program's command line and environment.

    args() as string[]             the words after the program's name
    env(name) as string            an environment variable, "" none
    exit()                         the program ended, files closed
    run(cmd)                       cmd run by the command interpreter

See also [Quickr](quickr index), [fs](quickr fs)

## Details
exit() runs no drop. END has no exit code; neither has exit().
run(cmd) is SHELL cmd: /bin/sh -c on the host, COMSPEC /C on DOS. It
gives no exit code; a command that could not start is Illegal function
call.

## Example
    option dialect quickr
    import os
    for a in os.args()
        print a
    next
    print os.env("PATH")
