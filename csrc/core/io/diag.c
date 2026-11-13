/* A compile error in words (io/diag.asm, from llrm's front/explain.rs). */
#define DIAG_DATA
#include "io/diag.h"
#include "comp/comp.h"
#include "comp/lexr.h"
#include "io/cons.h"
#include "io/emsg.h"
#include "io/repl.h"
#include "vm/exec.h"

Diag diag;

enum { STM_MAX = sizeof diag.stm - 1 };

static const char *const leads[] = {
    "SUB", "FUNCTION", "TYPE", "DECLARE", "CALL", "STATIC", "SHARED", "DIM", "REDIM", "COMMON",
    "CONST", "DEF", "END", "EXIT", "ERASE", "SELECT", "CASE", "GOTO", "GOSUB", "RETURN",
};

/* A name's or a number's byte: a letter, a digit or . _ $ % & ! # @. */
static bool is_word(const char c)
{
    if (is_digit((uint8_t)c) || is_letter((uint8_t)c))
        return true;
    return c && strchr("._$%&!#@", c);
}

/* The letters at s, at most max. */
static uint32_t letters(const char *const s, const uint32_t max)
{
    uint32_t n = 0;
    while (n < max && is_letter((uint8_t)s[n]))
        n++;
    return n;
}

/* n bytes of s to d in capitals, 0 after. */
static void upper(char *const d, const char *const s, const uint32_t n)
{
    for (uint32_t i = 0; i < n; i++)
        d[i] = (char)upcase((uint8_t)s[i]);
    d[n] = 0;
}

/* The tables (diag.h) are runs of 0-ended strings: a form's entry two (word, form), a message's
   three (name, what, todo); an empty string ends a table. */
static const char *skip(const char *const s) { return s + strlen(s) + 1; }
static const char *next_form(const char *const e) { return skip(skip(e)); }
static const char *next_msg(const char *const e) { return skip(skip(skip(e))); }

/* The form of statement word w, nullptr none. */
static const char *form_of(const char *const w)
{
    for (const char *e = diag_forms; *e; e = next_form(e))
        if (!strcmp(e, w))
            return skip(e);
    return nullptr;
}

/* The entry named key, nullptr none. */
static const char *entry(const char *const key)
{
    for (const char *e = diag_msgs; *e; e = next_msg(e))
        if (!strcmp(e, key))
            return e;
    return nullptr;
}

/* The item before column col: col and len at it; none: len 0. */
static void back(const uint32_t col)
{
    const char *s = diag.line;
    uint32_t b = col;
    while (b && s[b - 1] == ' ')
        b--;
    if (!b) {
        diag.len = 0;
        return;
    }
    const uint32_t end = b--;
    if (s[b] == '"') {
        while (b && s[--b] != '"') {
        }
    } else if (is_word(s[b])) {
        while (b && is_word(s[b - 1]))
            b--;
    }
    diag.col = b;
    diag.len = end - b;
}

/* stm: the statement's first word, if a keyword; first when the error is at that word. */
static void statement(void)
{
    const char *const s = diag.line;
    const uint32_t c = diag.col;
    uint32_t start = 0;
    bool quoted = false;
    for (uint32_t i = 0; i < c; i++) { /* past the last ':' out of quotes */
        if (s[i] == '"')
            quoted = !quoted;
        if (s[i] == ':' && !quoted)
            start = i + 1;
    }
    while (start < c && (s[start] == ' ' || is_digit((uint8_t)s[start])))
        start++; /* past blanks and a line number */
    const char *w = s + start;
    uint32_t n = letters(w, c - start);
    diag.first = !n;
    if (n < 2) {
        if (!diag.first) { /* at the start: the word at the error */
            diag.stm[0] = 0;
            return;
        }
        w = s + diag.col;
        n = letters(w, 255);
        if (n < 2) {
            diag.stm[0] = 0;
            return;
        }
    }
    upper(diag.stm, w, n > STM_MAX ? STM_MAX : n);
    if (form_of(diag.stm))
        return;
    const uint8_t *p = (const uint8_t *)diag.stm; /* a keyword, if no form for it */
    if (!lexr_keyword(&p) || *p)
        diag.stm[0] = 0;
}

/* name: the first word from the error on that is no leading keyword (p in SUB p (a%)). */
static void name(void)
{
    const char *b = diag.line + diag.col;
    diag.namelen = 0;
    for (;;) {
        while (*b == ' ')
            b++;
        uint32_t n = 0;
        while (is_word(b[n]))
            n++;
        if (!n)
            return;
        upper(diag.key, b, n < sizeof diag.key ? n : sizeof diag.key - 1);
        bool lead = false;
        for (size_t i = 0; i < countof(leads) && !lead; i++)
            lead = !strcmp(diag.key, leads[i]);
        if (!lead) {
            diag.name = b;
            diag.namelen = n;
            return;
        }
        b += n;
    }
}

static const char *const s_syntax = "Syntax error";

/* A syntax error in words: what was found where, then the statement's form. */
static void syntax(void)
{
    diag.bc = s_syntax;
    bool done = false;
    if (diag.first && diag.len) { /* at the statement's first word, a word: that one is it */
        const uint32_t n = letters(diag.line + diag.col, diag.len);
        if (n == diag.len && n <= STM_MAX) {
            upper(diag.stm, diag.line + diag.col, n);
            diag.what = "`{text}` cannot be used here: it does not start a valid statement at this "
                      "point";
            done = true;
        }
    }
    if (!done) {
        const char *names[256];
        if (diag.state != DX_NOSTATE && comp_expnames(diag.state, names)) {
            diag.what = "expected {expected} in the {stm}, but found {found}";
        } else {
            diag.what = diag.len ? "`{text}` does not fit the {stm} here"
                             : "the {stm} is incomplete: the line ends where more is needed";
        }
    }
    diag.todo = "check the statement against the manual; the mark shows where it stopped being "
              "understood";
    const char *const f = form_of(diag.stm);
    if (f) { /* STATEMENT is written: */
        diag.form = f;
        const size_t n = strlen(diag.stm);
        memcpy(diag.key, diag.stm, n);
        memcpy(diag.key + n, " is written:", sizeof " is written:");
        diag.todo = diag.key;
    }
}

/* The error's words: what is wrong, what to do, the name for an error with no words. */
static void words(void)
{
    diag.form = nullptr;
    const char *e = nullptr;
    if (diag.dg) {
        e = diag_msgs; /* DG_ n: the nth message */
        for (uint8_t i = 1; i < diag.dg; i++)
            e = next_msg(e);
    } else { /* by BC's name: name /TEXT, name /end, name */
        const char *const t = emsg_text(diag.code);
        const size_t n = strlen(t);
        memcpy(diag.key, t, n);
        diag.keyend = diag.key + n;
        diag.keyend[0] = ' ';
        diag.keyend[1] = '/';
        const uint32_t room = (uint32_t)(sizeof diag.key - 1 - (n + 2));
        upper(diag.keyend + 2, diag.line + diag.col, diag.len < room ? diag.len : room);
        e = entry(diag.key);
        if (!e && !diag.len) {
            memcpy(diag.keyend, " /end", sizeof " /end");
            e = entry(diag.key);
        }
        if (!e) {
            *diag.keyend = 0;
            e = entry(diag.key);
        }
        if (!e) {
            diag.bc = diag.key;
            diag.what = "{bc} (at `{text}`)";
            diag.todo = nullptr;
            if (diag.code == E_SN)
                syntax();
            return;
        }
    }
    diag.bc = e;
    diag.what = skip(e);
    diag.todo = skip(diag.what);
}

/* The item at b's length: a string to its closing quote, a word, or one sign; 0 the end. */
static uint32_t item_len(const char *const b)
{
    if (!*b)
        return 0;
    uint32_t n = 1;
    if (*b == '"') {
        while (b[n] && b[n] != '"')
            n++;
        if (b[n])
            n++; /* the closing quote */
    } else if (is_word(*b)) {
        while (is_word(b[n]))
            n++;
    }
    return n;
}

uint32_t diag_at(const char *const text, uint32_t col, const uint8_t code, const uint8_t dg,
                 const uint16_t state)
{
    diag.line = text;
    diag.code = code;
    diag.dg = dg;
    diag.state = state;
    const uint32_t end = (uint32_t)strlen(text); /* the column, at most the end */
    if (col > end)
        col = end;
    while (text[col] == ' ')
        col++;
    diag.col = col;
    const uint32_t n = item_len(text + col);
    diag.len = n;
    if (!n && code != E_SN) /* nothing here, past what it read: that item, but for a syntax error */
        back(col);
    statement();
    name();
    words();
    return diag.col;
}

void diag_raise(const uint8_t code, const uint8_t dg, const uint32_t at, const uint32_t line,
                const char *const other, uint32_t len)
{
    static ErrOp op;
    op = (ErrOp){.op = O_ERR, .code = code, .dg = dg, .at = at, .state = DX_NOSTATE};
    vm.line = line;
    vm.opip = (uint8_t *)&op;
    vm.err.active = true;
    if (len > sizeof diag.other - 1)
        len = sizeof diag.other - 1;
    memcpy(diag.other, other, len);
    diag.other[len] = 0;
    repl_error(code);
}

static void put_n(const char *const s, const uint32_t n)
{
    for (uint32_t i = 0; i < n; i++)
        cons_putc((uint8_t)s[i]);
}

static void put_text(void) { put_n(diag.line + diag.col, diag.len); }

/* A message's {holder}s. */
typedef enum { H_TEXT, H_NAME, H_STATEMENT, H_STM, H_OTHER, H_BC, H_EXPECTED, H_FOUND, NHOLDERS } Holder;
static const char *const holders[NHOLDERS] = {
    [H_TEXT] = "text}",   [H_NAME] = "name}", [H_STATEMENT] = "statement}", [H_STM] = "stm}",
    [H_OTHER] = "other}", [H_BC] = "bc}",     [H_EXPECTED] = "expected}",   [H_FOUND] = "found}",
};

/* The holder after a '{' at t; NHOLDERS none. */
static Holder holder_at(const char *const t)
{
    for (uint32_t h = 0; h < NHOLDERS; h++)
        if (!strncmp(t, holders[h], strlen(holders[h])))
            return (Holder)h;
    return NHOLDERS;
}

static void put_holder(const Holder h)
{
    switch (h) {
    case H_TEXT:
        put_text();
        break;
    case H_NAME:
        if (diag.namelen)
            put_n(diag.name, diag.namelen);
        else
            put_text();
        break;
    case H_STATEMENT:
        cons_puts(diag.stm[0] ? diag.stm : "this");
        break;
    case H_STM:
        if (diag.stm[0]) {
            cons_puts(diag.stm);
            cons_putc(' ');
        }
        cons_puts("statement");
        break;
    case H_OTHER:
        cons_puts(diag.other);
        break;
    case H_BC:
        cons_puts(diag.bc);
        break;
    case H_EXPECTED:
        comp_expected(diag.state);
        break;
    default:
        if (!diag.len) {
            cons_puts("the end of the statement");
        } else {
            cons_putc('`');
            put_text();
            cons_putc('`');
        }
    }
}

/* Message t, its holders filled; dx.cap: its first letter a capital. */
static void fill(const char *t)
{
    for (char c; (c = *t++);) {
        const Holder h = c == '{' ? holder_at(t) : NHOLDERS;
        if (c == '{') {
            diag.cap = false;
            diag.last = 'x';
        }
        if (h == NHOLDERS) {
            if (diag.cap) {
                diag.cap = false;
                c = (char)upcase((uint8_t)c);
            }
            diag.last = c;
            cons_putc((uint8_t)c);
            continue;
        }
        t += strlen(holders[h]);
        put_holder(h);
    }
}

/* fill as a sentence: a capital first, a period last unless it ends in one or a ':'. */
static void sentence(const char *const t)
{
    diag.cap = true;
    fill(t);
    diag.cap = false;
    if (diag.last != '.' && diag.last != ':')
        cons_putc('.');
}

void diag_what(void) { sentence(diag.what); }

bool diag_todo(void)
{
    if (!diag.todo || !*diag.todo)
        return false;
    sentence(diag.todo);
    return true;
}

bool diag_shape(void)
{
    if (!diag.form)
        return false;
    cons_puts(diag.form);
    return true;
}

void diag_say(void)
{
    diag_what();
    cons_nl();
    if (diag_todo())
        cons_nl();
    if (diag.form) {
        cons_spaces(2);
        diag_shape();
        cons_nl();
    }
}
