/* What clang's i386-pc-win32 code expects of a C runtime: a function using floating point marks
 * itself by referring to _fltused. Its 64-bit division's helpers are lib/lldiv.c's.
 */
int _fltused;
