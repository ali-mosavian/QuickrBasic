# Device I/O error / Disk full / Disk not ready / Disk-media error
keys: ~Device I/O error, Disk full, Disk not ready, Disk-media error, error 57, error 61, error 71, error 72

Errors 57, 61, 71 and 72

## Cause

The disk could not be read or written: it is full, the drive door is
open or empty, or the disk is damaged.

## Remedy

Free space, insert a good disk and close the drive, then try again.
Trap the error with [ON ERROR](ON ERROR) to let the user retry.

See also: [OPEN](OPEN), [ON ERROR](ON ERROR)
