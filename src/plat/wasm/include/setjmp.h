/* setjmp and longjmp on wasm32: clang lowers them to WebAssembly exceptions
 * (-mllvm -wasm-enable-sjlj); libc.c has the runtime they call.
 */
#ifndef SETJMP_H
#define SETJMP_H

/* __wasm_setjmp's record: the function's invocation, its label, longjmp's argument. */
typedef struct {
    void *invocation;
    unsigned label;
    void *env;
    int value;
} jmp_buf[1];

int setjmp(jmp_buf env);
[[noreturn]]
void longjmp(jmp_buf env, int value);
#endif
