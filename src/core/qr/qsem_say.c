/* Quickr's semantic pass: its errors' text.
 *
 * # How it works
 * An error is said once, at its token: its parts joined, a type spelled as written (qsem_tyname),
 * and in an instance its name after them.
 */
#include "qr/qsem_int.h"
#include <stdarg.h>

static const char *qsem_tyname(QSem *S, QType t);
static const char *qsem_instname(QSem *S, const QFn *f);

enum { QSEM_SAY = 160 }; /* an error's text at most */

/* ---- errors */

/* An error at t: the parts, to a null, joined; in an instance, its name after them. */
void qsem_say(QSem *const S, const QTok *const t, ...)
{
    char text[QSEM_SAY];
    uint32_t n = 0;
    va_list ap;
    va_start(ap, t);

    for (const char *p; (p = va_arg(ap, const char *));) {
        const uint32_t k = (uint32_t)strlen(p);
        const uint32_t room = k < QSEM_SAY - 1 - n ? k : QSEM_SAY - 1 - n;
        memcpy(text + n, p, room);
        n += room;
    }

    va_end(ap);
    const QFn *const f = S->body ? S->body : S->fn;
    const QType ty = f && f->in && f->in->kind == QS_TYPE ? f->in->ty : S->inst;
    const bool inst = ty && qast_ty(S->prog, ty)->of;
    const char *const in = f && f->of ? qsem_instname(S, f)
                           : inst     ? qsem_join(S, " in ", qsem_tyname(S, ty), "")
                                      : "";
    const uint32_t k = (uint32_t)strlen(in);
    const uint32_t room = k < QSEM_SAY - 1 - n ? k : QSEM_SAY - 1 - n;
    memcpy(text + n, in, room);
    qast_error(&S->prog->errs, t, "{}", text, n + room);
}

/* t's text as a C string, for qsem_say. */
const char *qsem_text(QSem *const S, const QTok *const t)
{
    char *const s = qast_alloc(S->mem, t->len + 1);
    memcpy(s, t->text, t->len);
    return s;
}

const char *qsem_tname(const QType t) { return qast_typename(t); }

/* a, b and c joined, in the arena. */
const char *qsem_join(QSem *const S, const char *const a, const char *const b, const char *const c)
{
    const size_t na = strlen(a), nb = strlen(b), nc = strlen(c);
    char *const s = qast_alloc(S->mem, na + nb + nc + 1);
    memcpy(s, a, na);
    memcpy(s + na, b, nb);
    memcpy(s + na + nb, c, nc);
    return s;
}

/* n in decimal, in the arena. */
const char *qsem_uint(QSem *const S, uint32_t n)
{
    char rev[12];
    uint32_t k = 0;

    do {
        rev[k++] = (char)('0' + n % 10);
        n /= 10;
    } while (n);

    char *const s = qast_alloc(S->mem, k + 1);

    for (uint32_t i = 0; i < k; i++)
        s[i] = rev[k - 1 - i];

    return s;
}

/* Type t's name as written: int, Vec, int[], Vec[4], (int, string). */
static const char *qsem_tyname(QSem *const S, const QType t)
{
    if (!qast_compound(t))
        return qast_typename(t);

    const QTy *const y = qast_ty(S->prog, t);

    switch (y->kind) {
    case QT_REC:
    case QT_ENUM: {
        const char *n = qsem_text(S, y->name);

        for (uint32_t j = 0; y->of && j < y->ntp; j++) /* an instance's: Stack[int] */
            n = qsem_join(S, n, j ? ", " : "[", qsem_tyname(S, y->targs[j]));

        return y->of ? qsem_join(S, n, "]", "") : n;
    }

    case QT_DYN:
        return y->grows ? qsem_join(S, "list[", qsem_tyname(S, y->elem), "]")
                        : qsem_join(S, qsem_tyname(S, y->elem), "[]", "");

    case QT_ARRAY:
        return qsem_join(
            S,
            qsem_join(S, qsem_tyname(S, y->elem), "[", qsem_uint(S, y->count)),
            "]",
            ""
        );

    case QT_MAP:
        return qsem_join(
            S,
            qsem_join(S, "dict[", qsem_tyname(S, y->key), ", "),
            qsem_tyname(S, y->elem),
            "]"
        );

    case QT_OPT:
    case QT_RES:
        return qsem_join(
            S,
            qsem_tyname(S, y->fields[QF_VAL].type),
            y->kind == QT_OPT ? " or none" : " or Error",
            ""
        );

    case QT_FN: {
        const char *s = "function(";

        for (uint32_t k = 0; k < y->nfields; k++)
            s = qsem_join(S, s, k ? ", " : "", qsem_tyname(S, y->fields[k].type));

        s = qsem_join(S, s, ")", "");
        return y->elem == QY_VOID ? s : qsem_join(S, s, " as ", qsem_tyname(S, y->elem));
    }

    default: {
        const char *s = "(";

        for (uint32_t k = 0; k < y->nfields; k++)
            s = qsem_join(S, s, k ? ", " : "", qsem_tyname(S, y->fields[k].type));

        return qsem_join(S, s, ")", "");
    }
    }
}

/* A type's name with its article: an int, a single, a Vec. */
const char *qsem_aname(QSem *const S, const QType t)
{
    static const char *const names[] = {
        [QY_VOID] = "nothing",
        [QY_INT] = "an int",
        [QY_SINGLE] = "a single",
        [QY_DOUBLE] = "a double",
        [QY_STRING] = "a string",
        [QY_BOOL] = "a bool",
        [QY_ERROR] = "an error",
        [QY_BYTE] = "a byte",
        [QY_SHORT] = "a short",
        [QY_LONG] = "a long",
        [QY_UBYTE] = "a ubyte",
        [QY_USHORT] = "a ushort",
        [QY_UINT] = "a uint",
        [QY_ULONG] = "a ulong",
    };

    if (t < QY_FIRST)
        return names[t];

    const char *const n = qsem_tyname(S, t);
    return qsem_join(S, strchr("AEIOUaeiou", n[0]) ? "an " : "a ", n, "");
}

/* Instance f's name as an error ends with it: " in f[int, Vec]"; none for a fn's instance for
   its lambdas, whose errors are its own. */
static const char *qsem_instname(QSem *const S, const QFn *const f)
{
    if (!f->ntp)
        return "";

    const char *s = qsem_join(S, " in ", qsem_text(S, f->name), "[");

    for (uint32_t j = 0; j < f->ntp; j++)
        s = qsem_join(S, s, j ? ", " : "", qsem_tyname(S, f->targs[j]));

    return qsem_join(S, s, "]", "");
}
