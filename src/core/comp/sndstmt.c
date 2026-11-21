/* The speaker's statements compiled: SOUND hertz, ticks; PLAY commands; PLAY a$, b$, ..., a voice each; and the
 * songs' (docs/sound.md): instrument n, commands$; music events()[, from]; music stop[, ticks]; note sound, pitch,
 * length, loud; beat(). Each pushes its operands, then an O_SYSCALL of its service (vm/syscalls.def).
 *
 * The songs' words are names, not keywords: each is ours only where a program of QBasic's could not have written
 * it, a statement starting with the name that is neither a SUB's call nor an assignment, or beat() with nothing
 * between its parentheses and no FUNCTION of that name. A program that names a variable, an array or a procedure
 * so is as it was.
 *
 * # asm structure
 * | asm | here | |
 * |---|---|---|
 * | (none) | snds_stmt | NEW |
 */
#include <string.h>

#include "comp/sndstmt.h"
#include "comp/declare.h"
#include "comp/dfa.h"
#include "comp/expr.h"
#include "comp/refs.h"
#include "vm/exec.h"
#include "vm/syscall.h"

/* PLAY's strings at most: a voice each */
enum { PLAY_VOICES = 32 };

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

/* The name token at p is `word`, its capitals: written in any case, with no suffix. */
static bool named(const uint8_t *const p, const char *const word)
{
    const size_t n = strlen(word);

    if (*p != M_VAR || p[1] != n)
        return false;
    for (size_t i = 0; i < n; i++)
        if ((p[2 + i] & ~0x20) != (uint8_t)word[i])
            return false;
    return true;
}

/* The name at p is assigned to: `=` after it, past its subscripts, or a field after it. */
static bool assigned(const uint8_t *p)
{
    p = comp_skipname(p);
    while (*p == ' ')
        p++;
    if (*p == '(') {
        p = decl_paren(p);
        while (*p == ' ')
            p++;
    }
    return *p == TK_EQ || *p == '.';
}

/* An integer argument. */
static void snds_int(Comp *const C)
{
    expr_compile(C);
    comp_toint(C);
}

/* An array, name or name(): its cell, then 0 0, its first element on. */
static void snds_array(Comp *const C)
{
    if (comp_cur(C) != M_VAR)
        fault_error(C->fault, E_SN);
    const uint32_t id = ref_arrayof(C, comp_var(C, 2, 0));

    if (comp_cur(C) == '(') {
        comp_curn(C);
        comp_close(C);
    }
    comp_step(C, ref_cell(C, id));
    comp_int(C, 0);
    comp_int(C, 0);
}

bool snds_named(Comp *const C)
{
    const uint8_t *const name = C->p;
    uint8_t service;

    if (assigned(name))
        return false;
    if (named(name, "INSTRUMENT")) {
        C->p = comp_skipname(name);
        snds_int(C);
        comp_comma(C);
        expr_compile(C);
        comp_tostring(C);
        service = SC_INSTRUMENT;
    } else if (named(name, "NOTE")) {
        C->p = comp_skipname(name);
        for (int i = 0; i < 4; i++) {
            if (i)
                comp_comma(C);
            snds_int(C);
        }
        service = SC_NOTE;
    } else if (named(name, "MUSIC")) {
        C->p = comp_skipname(name);
        if (comp_cur(C) == TK_STOP) {
            comp_curn(C);
            if (comp_optcomma(C))
                snds_int(C);
            else
                comp_int(C, 0);
            service = SC_MUSICSTOP;
        } else {
            snds_array(C);
            if (comp_optcomma(C))
                snds_int(C);
            else
                comp_int(C, (uint32_t)-1);
            service = SC_MUSIC;
        }
    } else
        return false;
    comp_byte(comp_op(C, O_SYSCALL), service);
    return true;
}

bool snds_beat(Comp *const C)
{
    const uint8_t *p = comp_skipname(C->p);

    if (!named(C->p, "BEAT") || comp_peek(&p) != '(' || comp_next(&p) != ')')
        return false;
    C->p = p + 1;
    return true;
}
