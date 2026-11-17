/* The speaker's statements compiled: SOUND hertz, ticks; PLAY commands; PLAY a$, b$, ..., a voice each. Each pushes its operands,
 * then an O_SYSCALL of its service (vm/syscalls.def).
 *
 * # asm structure
 * | asm | here | |
 * |---|---|---|
 * | (none) | snds_stmt | NEW |
 */
#include "comp/sndstmt.h"
#include "comp/dfa.h"
#include "comp/expr.h"
#include "vm/exec.h"
#include "vm/syscall.h"

/* PLAY's strings at most: a voice each */
enum { PLAY_VOICES = 6 };

/* After PLAY (comp.p): its event's ON, OFF or STOP. */
static bool snds_event(Comp *const C)
{
    const uint8_t c = comp_cur(C);
    return c == TK_ON || c == TK_STOP || (c == TK_X && C->p[1] == TK1_OFF);
}

bool snds_stmt(Comp *const C)
{
    if (C->p[0] != TK_X)
        return false;

    switch (C->p[1]) {
    case TK1_SOUND:
        C->p += 2;
        expr_compile(C);
        comp_toint(C);
        comp_comma(C);
        expr_compile(C);
        comp_todouble(C);
        comp_byte(comp_op(C, O_SYSCALL), SC_SOUND);
        return true;

    case TK1_PLAY:
        C->p += 2;

        if (snds_event(C)) {
            C->p -= 2;
            return false;
        }

        /* a string a voice; one left out (PLAY , , a$) leaves its voice playing what it was */
        uint32_t given = 0;
        for (int32_t voice = 0;; voice++) {
            if (comp_cur(C) != ',') {
                expr_compile(C);
                comp_tostring(C);
                given |= 1u << voice;
            }
            if (comp_cur(C) != ',' || voice + 1 == PLAY_VOICES)
                break;
            comp_curn(C);
        }
        if (given == 1) {
            comp_byte(comp_op(C, O_SYSCALL), SC_PLAY);
            return true;
        }
        comp_int(C, given);
        comp_byte(comp_op(C, O_SYSCALL), SC_PLAYV);
        return true;
    }

    return false;
}
