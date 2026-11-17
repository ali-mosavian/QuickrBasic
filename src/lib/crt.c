/* What clang's i386-pc-win32 code expects of a C runtime.
 *
 * # How it works
 * A function using floating point marks itself by referring to _fltused.
 */
int _fltused;
