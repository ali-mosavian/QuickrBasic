/* Constant folding (comp/fold): a direct line's code. */
#include "session.h"
#include "unit.h"

/* The direct line text compiled; its code's bytes in code, how many. */
static uint32_t compile(Session *const S, const char *const text, const uint8_t **const code)
{
    const uint32_t n = (uint32_t)strlen(text);
    memcpy(S->cons.inbuf, text, n);
    CHECK(repl_enter(S, n) == ENTER_DIRECT);
    comp_line(S);
    *code = S->comp.xbuf;
    return (uint32_t)(S->comp.out - S->comp.xbuf);
}

static bool has(const uint8_t *const c, const uint32_t n, const uint8_t *const seq, const uint32_t len)
{
    for (uint32_t i = 0; i + len <= n; i++)
        if (!memcmp(c + i, seq, len))
            return true;
    return false;
}

TEST(constant_expression_compiles_to_its_value)
{
    const uint8_t *c;
    const uint32_t n = compile(S, "x% = 2 * 3 + 4 * 5", &c);
    const uint8_t load[] = {O_I_LD_U8, 26};
    CHECK(has(c, n, load, sizeof load));
    const uint8_t mul[] = {O_I_MUL_U8};
    CHECK(!has(c, n, mul, sizeof mul));
}

TEST(constant_division_by_zero_is_left_to_raise)
{
    const uint8_t *c;
    const uint32_t n = compile(S, "x% = 1 \\ 0", &c);
    const uint8_t zero[] = {O_I_LD_U8, 0};
    CHECK(has(c, n, zero, sizeof zero));
}

TEST(constants_fold_inside_parentheses_and_arguments)
{
    const uint8_t *c;
    const uint32_t n = compile(S, "x! = SIN(2 * 3) + (1 + 2) * y!", &c);
    const uint8_t six[] = {O_F_LD_U8, 6};
    CHECK(has(c, n, six, sizeof six));
    const uint8_t three[] = {O_F_LD_U8, 3};
    CHECK(has(c, n, three, sizeof three));
    const uint8_t add[] = {O_I_ADD_U8};
    CHECK(!has(c, n, add, sizeof add));
}
