# UNLOCK Statement
keys: UNLOCK

Syntax
    UNLOCK [#]filenumber [, record | [start] TO end]

Releases a lock set with LOCK. The range must match the LOCK's. With
no range the whole file is unlocked.

## Example
    LOCK #1, 5
    UNLOCK #1, 5

See also [LOCK](LOCK), [OPEN](OPEN), [files](files)
