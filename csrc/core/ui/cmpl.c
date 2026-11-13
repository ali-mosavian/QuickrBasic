/* Tab completion (src/ui/compl.asm). An entry of the list: the length, the text and 0 (AC_LEN at
   most); the kind (AK_*); the rank, 0 first; a token or a line. */
#include "ui/cmpl.h"
#if AC
#include "comp/comp.h"
#include "comp/decl.h"
#include "comp/dfa.h"
#include "comp/lexr.h"
#include "io/gen.h"
#include "ui/draw.h"
#include "ui/edit.h"
#include "ui/mous.h"
#include "ui/scrn.h"
#include "ui/text.h"
#include "ui/uimn.h"

enum {
    AC_MAX = 48,  /* completions listed at most */
    AC_SZ = 18,   /* one: its length, its text */
    AC_EVN = 8,   /* a line's FORs and NEXTs */
    AC_ROWS = 8,  /* the list's rows */
    AC_LEN = 10,  /* a name's characters at most */
    AC_PREV = 40, /* a label's box: columns */
};
/* What fits (ac_m). */
enum : uint8_t { AM_ST = 1, AM_FN = 2, AM_OP = 4, AM_VR = 8, AM_LB = 0x10 };
/* What came last (ac_pv), when not a token. */
enum : uint8_t { AP_ST = 0, AP_ID = 1, AP_SEP = 2, AP_COM = 3 };
/* An entry's kind: a keyword, a function ('(' put after), a variable, a line label; AK_NONE not
   listed. */
enum : uint8_t { AK_KW = 0, AK_FN = 1, AK_VR = 2, AK_LB = 3, AK_NONE = 0xFF };
/* The type the expression wants (ac_ty). */
enum : uint8_t { AT_ANY = 0, AT_STR = 1, AT_NUM = 2 };
/* ac.fn: an array's subscripts, not a function's. */
enum : uint8_t { AF_ARRAY = 0xFF };
/* A body's M_VAR: len at VAR_LEN, the text at VAR_TEXT, VAR_FIXED bytes besides; an M_INT: n at
   INT_VALUE, INT_SZ bytes. */
enum { VAR_LEN = 1, VAR_TEXT = 2, VAR_FIXED = 4, INT_VALUE = 1, INT_SZ = 5 };
/* cons_kwlist: a keyword's last character has KW_LAST. */
enum : uint8_t { KW_LAST = 0x80, KW_CHAR = 0x7F, CASE_BIT = 0x20 };
/* The list's box: its frame's rows; a name's column, the columns past a name; a label's line at
   PV_COL past the name, PV_LEN characters at most. */
enum { BOX_FRAME = 2, NAME_COL = 2, NAME_PAD = 4, PV_COL = 7, PV_LEN = AC_PREV - 11 };

/* TACE */
typedef struct PACKED {
    uint8_t len;
    uint8_t text[AC_LEN + 1];
    uint8_t kind;
    uint8_t rank;
    uint32_t xt; /* its token or line */
} Entry;
static_assert(sizeof(Entry) == AC_SZ);

/* TCOMPL, ac_list, ac_ev. */
static struct {
    uint16_t n;      /* the list's names */
    uint16_t sel;    /* the one lit */
    uint16_t top;    /* the first shown */
    uint16_t pn;     /* the word's length */
    uint16_t ps;     /* its column */
    uint8_t at_row, at_col; /* the list's box */
    uint8_t m;       /* what fits (AM_*) */
    uint8_t st;      /* the statement's keyword */
    uint8_t pv;      /* what came last (AP_*, a token) */
    bool lc;         /* the word in small letters */
    uint8_t bw;      /* the box's columns */
    uint8_t bn;      /* its rows of names */
    uint8_t pfx[AC_SZ]; /* the word in capitals */
    uint8_t ty;      /* the type the expression wants (AT_*) */
    uint8_t lt;      /* the last operand's type */
    uint8_t fn;      /* the function whose ( is open, AF_ARRAY an array */
    uint8_t flt;     /* its value's type */
    uint8_t plk;     /* the line before's last statement */
    bool cur;        /* ac_evs on the word's own line */
    uint16_t dp;     /* NEXTs not yet matched */
    uint8_t nm[AC_SZ]; /* a name: its length, it, 0 */
    uint8_t on[AC_SZ]; /* the open FOR's variable */
    char sig[32];    /* a function's form */
    uint8_t ev[AC_EVN + 1][AC_SZ]; /* a line's FORs (the name) and NEXTs (0, a count); the
                                      asm's past-the-end slot made one */
    Entry list[AC_MAX];
} ac;

static const uint8_t var_stmts[] = {TK_FOR, TK_NEXT, TK_DIM, TK_READ, TK_INPUT, TK_INPUTN, TK_GET};
/* the words that go on after an operand, by statement */
static const uint8_t ops[][3] = {
    {TK_IF, TK_THEN, TK_GOTO}, {TK_FOR, TK_TO, TK_STEP}, {TK_ON, TK_GOTO, TK_GOSUB}};
/* the arguments of the functions not of one number */
static const struct {
    uint8_t tok;
    const char *args;
} func_args[] = {
    {TK_PEEK, "address"}, {TK_LEN, "s$"}, {TK_VAL, "s$"}, {TK_ASC, "s$"},
    {TK_CHRS, "code"}, {TK_LEFTS, "s$, n"}, {TK_RIGHTS, "s$, n"},
    {TK_MID, "s$, start[, n]"}, {TK_TAB, "column"}, {TK_SPC, "n"},
};

/* What a key in the list does. */
typedef enum : uint8_t { STEP_STAY, STEP_TAKE, STEP_LEAVE } Step;

static bool is_alnum(const uint8_t c) { return is_digit(c) || is_letter(c); }
static bool starts_number(const uint8_t c) { return c == '.' || is_digit(c); }
static bool is_token(const uint8_t c) { return c >= TK_END; }
static bool is_function(const uint8_t tok) { return tok >= TK_SGN && tok <= TK_MID; }
static bool is_operator(const uint8_t tok) { return tok >= TK_PLUS && tok <= TK_LT; } /* + .. < */
static bool is_comparison(const uint8_t tok) { return tok >= TK_GT && tok <= TK_LT; }

/* The column the word ending at col starts at. */
static uint16_t word_start(const LineBuf *const b, const uint16_t col)
{
    for (uint16_t s = col; s; s--)
        if (!is_alnum(b->text[s - 1]))
            return s;
    return 0;
}

/* ac_word: the word before the cursor; false: the cursor is inside a word or past the line. */
static bool word(void)
{
    const LineBuf *const b = &uimn.edline;
    const uint16_t col = b->col;
    if (col > b->len || (col < b->len && is_alnum(b->text[col])))
        return false;
    const uint16_t s = word_start(b, col);
    ac.ps = s;
    ac.pn = col - s;
    if (ac.pn >= AC_SZ - 2)
        return false;
    ac.lc = false;
    for (uint16_t i = 0; i < ac.pn; i++) {
        const uint8_t c = b->text[s + i];
        ac.lc = c >= 'a';
        ac.pfx[i] = upcase(c);
    }
    ac.pfx[ac.pn] = 0;
    return true;
}

/* ac_lex: the 0-ended uimn.build tokenized into comp_cbuf; the body. */
static uint8_t *lex(void)
{
    lexr_line(uimn.build, comp.cbuf);
    return comp.cbuf;
}

/* ac_line: text line line tokenized (the cursor's from the editor): its text in uimn.build, its
   body in comp_cbuf. */
static uint8_t *line_body(const uint32_t line)
{
    uint16_t len;
    if (line == uimn.ed.line) {
        len = uimn.edline.len;
        memcpy(uimn.build, uimn.edline.text, len);
    } else
        len = (uint16_t)text_get(line, uimn.build);
    uimn.build[len] = 0;
    return lex();
}

/* ac_tkty: the types keyword tok gives: a function's value, what an operator wants. */
static uint8_t tok_type(const uint8_t tok)
{
    if (tok == TK_FN || is_function(tok)) {
        const uint8_t t = tok != TK_FN && (tok == TK_STRS || tok >= TK_CHRS) ? AT_STR : AT_NUM;
        ac.fn = tok;
        ac.flt = t;
        ac.lt = t;
        return t;
    }
    const uint8_t other = ac.lt; /* + = < >: the other operand's type */
    if (tok == TK_PLUS || is_comparison(tok)) {
        ac.ty = other;
        return other;
    }
    if (tok >= TK_GT)
        return other;
    const uint8_t t = tok > TK_PLUS || tok == TK_NOT || tok == TK_TO || tok == TK_STEP ||
                              tok == TK_FOR || tok == TK_NEXT
                          ? AT_NUM /* - * / ^ AND OR NOT TO STEP FOR NEXT */
                          : AT_ANY;
    ac.ty = t;
    return t;
}

/* ac_vnm: the name at p's M_VAR to ac.nm: its length, then it in capitals, AC_LEN at most; a
   '(' after it added. Past it, at the '('. */
static const uint8_t *var_name(const uint8_t *const p)
{
    const uint32_t len = p[VAR_LEN];
    const uint8_t *const next = p + len + VAR_FIXED;
    uint32_t n = len > AC_LEN ? AC_LEN : len;
    for (uint32_t i = 0; i < n; i++)
        ac.nm[1 + i] = upcase(p[VAR_TEXT + i]);
    if (*next == '(' && n < AC_LEN)
        ac.nm[1 + n++] = '(';
    ac.nm[0] = (uint8_t)n;
    ac.nm[1 + n] = 0;
    return next;
}

/* ac_nty: the type of the name in ac.nm: a string when it ends in $. */
static uint8_t name_type(void)
{
    const uint32_t b = ac.nm[ac.nm[0]] == '(' ? ac.nm[0] - 1u : ac.nm[0];
    return ac.nm[b] == '$' ? AT_STR : AT_NUM;
}

/* The statement st names variables after pv. */
static bool names_vars(const uint8_t st, const uint8_t pv)
{
    for (uint32_t i = 0; i < countof(var_stmts); i++)
        if (st == var_stmts[i] && (pv == var_stmts[i] || pv == AP_COM))
            return true;
    return false;
}

/* ac_mask: ac.m from the statement (ac.st) and what came last (ac.pv). */
static void mask(void)
{
    const uint8_t pv = ac.pv, st = ac.st;
    if (pv == AP_ST)
        ac.m = AM_ST | AM_VR;
    else if (pv == TK_THEN)
        ac.m = AM_ST | AM_VR | AM_LB;
    else if (pv == TK_GOTO || pv == TK_GOSUB || pv == TK_RUN || pv == TK_LIST ||
             (st == TK_ON && pv == AP_COM))
        ac.m = AM_LB;
    else if (names_vars(st, pv))
        ac.m = AM_VR;
    else
        ac.m = pv == AP_ID ? AM_OP : AM_FN | AM_VR;
}

/* context's keyword c at p: a statement's start its keyword; an operator a separator. */
static const uint8_t *ctx_keyword(const uint8_t *const p, const uint8_t c)
{
    const uint8_t *const next = decl_item(p);
    if (ac.pv == TK_THEN || ac.pv == AP_ST)
        ac.st = c;
    ac.pv = c;
    tok_type(c);
    if (is_operator(c))
        ac.pv = AP_SEP;
    return next;
}

/* context's name at p: an operand, or an array's subscripts (numbers) opened. */
static const uint8_t *ctx_var(const uint8_t *const p)
{
    const uint8_t *const next = var_name(p);
    const uint8_t t = name_type();
    ac.lt = t;
    if (ac.pv == AP_ST)
        ac.st = AP_ID; /* an assignment */
    if (*next != '(') {
        ac.pv = AP_ID;
        return next;
    }
    ac.flt = t;
    ac.fn = AF_ARRAY;
    ac.ty = AT_NUM;
    ac.pv = AP_SEP;
    return next + 1;
}

/* context's punctuation c: a function's argument opened, a call closed, a list's separator. */
static void ctx_punct(const uint8_t c)
{
    uint8_t pv = AP_SEP;
    const uint8_t fn = ac.fn;
    if (c == '(') { /* a function's argument: its type */
        if (fn) {
            ac.ty = AT_STR; /* LEN, VAL, ASC, LEFT$, RIGHT$, MID$ */
            if (fn != TK_LEN && fn != TK_VAL && fn != TK_ASC && fn < TK_LEFTS)
                ac.ty = AT_NUM;
        }
    } else if (c == ')') {
        pv = AP_ID;
        if (fn) { /* the call's value */
            ac.lt = ac.flt;
            ac.fn = 0;
        }
    } else if (c == ',' || c == ';') {
        pv = AP_COM;
        ac.ty = fn >= TK_LEFTS ? AT_NUM : AT_ANY; /* LEFT$( s$, n */
    }
    ac.pv = pv;
}

/* ac_ctx: what fits at the word: the line before it tokenized, its last statement read item by
   item; the type each operand gives (ac.lt) and the one the expression wants (ac.ty). false:
   nothing (a string, REM, DATA). */
static bool context(void)
{
    memcpy(uimn.build, uimn.edline.text, ac.ps);
    uimn.build[ac.ps] = 0;
    const uint8_t *p = lex();
    if (decl_tail(&p))
        return false;
    ac.st = 0;
    ac.pv = AP_ST;
    ac.ty = AT_ANY;
    ac.fn = 0;
    for (uint8_t c; (c = *p);) {
        if (is_token(c))
            p = ctx_keyword(p, c);
        else if (c == M_VAR)
            p = ctx_var(p);
        else if (c == '"') {
            p = decl_item(p);
            ac.lt = AT_STR;
            ac.pv = AP_ID;
        } else if (c == M_INT || starts_number(c)) {
            p = decl_step(p);
            ac.lt = AT_NUM;
            ac.pv = AP_ID;
        } else {
            p++;
            ctx_punct(c);
        }
    }
    mask();
    if (starts_number(ac.pfx[0])) /* a number is a label */
        ac.m &= AM_LB;
    return ac.m != 0;
}

/* ac_want: token tok is wanted by ac.m in statement ac.st. */
static bool wanted(const uint8_t tok)
{
    if (ac.m & AM_ST && tok <= TK_NEW)
        return true;
    if (ac.m & AM_FN &&
        (tok == TK_TAB || tok == TK_FN || tok == TK_SPC || tok == TK_NOT || is_function(tok)))
        return true;
    if (!(ac.m & AM_OP))
        return false;
    if (tok == TK_AND || tok == TK_OR)
        return true;
    for (uint32_t i = 0; i < countof(ops); i++)
        if (ops[i][0] == ac.st && (tok == ops[i][1] || tok == ops[i][2]))
            return true;
    return false;
}

enum : uint16_t { PLACE_THERE = UINT16_MAX };
/* The place in the list of up, of rank: before the first above it; PLACE_THERE there already. */
static uint16_t place_of(const uint8_t *const up, const uint8_t rank)
{
    for (uint16_t i = 0; i < ac.n; i++) {
        const Entry *const e = &ac.list[i];
        if (rank < e->rank)
            return i;
        if (rank > e->rank)
            continue;
        const int d = strcmp((const char *)up, (const char *)e->text);
        if (!d)
            return PLACE_THERE;
        if (d < 0)
            return i;
    }
    return ac.n;
}

/* ac_add: n characters at s to the list, in capitals, once, in order of rank then text; when
   they start with the word and are longer. A variable ranks first when it is the open FOR's. */
static void add(const uint8_t *const s, const uint32_t n, const uint8_t kind, uint8_t rank,
                const uint32_t xt)
{
    if (n <= ac.pn || n > AC_LEN)
        return;
    uint8_t up[AC_LEN + 1];
    for (uint32_t i = 0; i < n; i++)
        up[i] = upcase(s[i]);
    up[n] = 0;
    if (memcmp(up, ac.pfx, ac.pn))
        return;
    if (kind == AK_VR && ac.on[0]) /* the open FOR's variable first */
        rank = memcmp(up, ac.on + 1, n + 1) ? 1 : 0;
    const uint16_t i = place_of(up, rank);
    if (i == PLACE_THERE || ac.n >= AC_MAX)
        return;
    memmove(&ac.list[i + 1], &ac.list[i], (ac.n - i) * sizeof(Entry));
    Entry *const e = &ac.list[i];
    e->len = (uint8_t)n;
    memcpy(e->text, up, n + 1);
    e->kind = kind;
    e->rank = rank;
    e->xt = xt;
    ac.n++;
}

/* The keyword at p in cons_kwlist to w, 0-ended, *n its length; past it. */
static const uint8_t *kw_word(const uint8_t *const p, uint8_t *const w, uint32_t *const n)
{
    uint32_t i = 0;
    for (; !(p[i] & KW_LAST); i++)
        w[i] = p[i] & KW_CHAR;
    w[i] = p[i] & KW_CHAR;
    w[i + 1] = 0;
    *n = i + 1;
    return p + i + 1;
}

/* keyword tok, starting with c0: its kind in the list; AK_NONE not wanted. */
static uint8_t kw_kind(const uint8_t tok, const uint8_t c0)
{
    if (!wanted(tok) || !is_letter(c0))
        return AK_NONE;
    if (ac.ty == AT_STR && (tok == TK_FN || tok == TK_NOT)) /* FN and NOT give numbers */
        return AK_NONE;
    if (tok == TK_TAB || tok == TK_SPC) /* TAB( SPC( in PRINT only */
        return ac.st == TK_PRINT || ac.st == TK_PRINTN ? AK_FN : AK_NONE;
    if (tok < TK_SGN)
        return AK_KW;
    const uint8_t t = tok_type(tok); /* a function: its value's type */
    return ac.ty && ac.ty != t ? AK_NONE : AK_FN;
}

/* ac_keys: the wanted keywords that start with the word, to the list: a function only of the
   type wanted. */
static void keys(void)
{
    const uint8_t *p = cons_kwlist;
    uint8_t w[64];
    for (uint8_t tok = TK_END; *p; tok++) {
        uint32_t n;
        p = kw_word(p, w, &n);
        const uint8_t kind = kw_kind(tok, w[0]);
        if (kind != AK_NONE)
            add(w, n, kind, tok <= TK_NEW ? 0 : 1, tok); /* a function after the variables */
    }
}

/* ac_label: the label that starts the body p of line line, to the list. */
static void label(const uint8_t *const p, const uint32_t line)
{
    const uint32_t num = load32(p + INT_VALUE);
    if (p[0] != M_INT || num > UINT16_MAX)
        return;
    const char *const e = uimn_dec((char *)ac.nm, (uint16_t)num);
    const bool start = ac.plk == TK_RET || ac.plk == TK_END || ac.plk == TK_STOP ||
                       ac.plk == TK_GOTO;
    const uint8_t rank = ac.pv == TK_GOSUB && start ? 0 : 1; /* GOSUB: a start first */
    add(ac.nm, (uint32_t)(e - (char *)ac.nm), AK_LB, rank, line);
}

/* ac_ids: the names in the body p, to the list: not after REM or DATA; of the type wanted; no
   array after FOR or NEXT. */
static void ids(const uint8_t *p, const uint32_t line)
{
    for (uint8_t c; (c = *p) && c != TK_REM && c != TK_DATA;) {
        if (c != M_VAR) {
            p = decl_step(p);
            continue;
        }
        p = var_name(p);
        if (ac.ty && name_type() != ac.ty)
            continue;
        if (ac.nm[ac.nm[0]] == '(' && ac.m == AM_VR && (ac.st == TK_FOR || ac.st == TK_NEXT))
            continue;
        add(ac.nm + 1, ac.nm[0], AK_VR, 0, line);
    }
}

/* ac_vars: the program's variables or labels that start with the word. GOSUB ranks first a
   label after a RETURN, END, STOP or GOTO: a start. */
static void vars(void)
{
    if (!(ac.m & (AM_VR | AM_LB)))
        return;
    ac.plk = 0;
    for (uint32_t line = 0; line < uimn.text.n; line++) {
        const uint8_t *p = line_body(line);
        if (ac.m & AM_LB && line != uimn.ed.line) /* not the word's own line */
            label(p, line);
        if (ac.m & AM_VR)
            ids(p, line);
        p = comp.cbuf;
        decl_tail(&p); /* its last statement's keyword */
        ac.plk = is_token(*p) ? *p : 0;
    }
}

/* The names of the NEXT whose list is at p counted to *n; past them. */
static const uint8_t *next_names(const uint8_t *p, uint8_t *const n)
{
    *n = 0;
    for (;;) {
        if (*p == ',')
            p++;
        else if (*p == M_VAR) {
            p = decl_item(p);
            (*n)++;
        } else
            return p;
    }
}

/* The body p's FORs and NEXTs to ac.ev, AC_EVN at most kept apart; their count. */
static uint32_t collect_events(const uint8_t *p)
{
    uint32_t k = 0;
    for (uint8_t c; (c = *p) && c != TK_REM && c != TK_DATA;) {
        p = decl_step(p);
        if (c == TK_FOR && *p == M_VAR) {
            p = var_name(p);
            memcpy(ac.ev[k], ac.nm, AC_SZ);
        } else if (c == TK_NEXT) {
            uint8_t n;
            p = next_names(p, &n);
            ac.ev[k][0] = 0;
            ac.ev[k][1] = n;
        } else
            continue;
        if (k < AC_EVN)
            k++;
    }
    return k;
}

/* ac.ev's k events taken last first against the NEXTs after them (ac.dp); true: the open FOR
   found, in ac.on. */
static bool match_events(const uint32_t k)
{
    for (uint32_t i = k; i > 0; i--) {
        const uint8_t *const e = ac.ev[i - 1];
        if (!e[0]) { /* NEXT: a bare one closes one, but not the one being written */
            ac.dp += !e[1] && !ac.cur ? 1 : e[1];
            ac.cur = false;
        } else if (ac.dp)
            ac.dp--;
        else {
            memcpy(ac.on, e, AC_SZ);
            return true;
        }
    }
    return false;
}

/* ac_evs: the body p's FORs and NEXTs, taken last first against the NEXTs after them (ac.dp);
   true: the open FOR found, in ac.on. */
static bool events(const uint8_t *const p) { return match_events(collect_events(p)); }

/* ac_open: the variable of the innermost FOR that no NEXT closes before the word, to ac.on. */
static void open_for(void)
{
    ac.dp = 0;
    ac.cur = true;
    const uint8_t *p = comp.cbuf; /* the line to the word, as context left it */
    for (int32_t line = (int32_t)uimn.ed.line; !events(p);) {
        ac.cur = false;
        if (--line < 0)
            return;
        p = line_body((uint32_t)line);
    }
}

/* ac_put: characters ac.pn to upto of the name s typed after the word, in small letters when the
   word is; the word that long. */
static void put(const uint8_t *const s, const uint16_t upto)
{
    if (upto <= ac.pn)
        return;
    uint8_t t[AC_SZ];
    const uint16_t n = upto - ac.pn;
    for (uint16_t i = 0; i < n; i++) {
        const uint8_t c = s[ac.pn + i];
        t[i] = ac.lc && is_letter(c) ? c | CASE_BIT : c;
    }
    ac.pn = upto;
    edit_lb_insert(&uimn.edline, t, n);
    uimn.text.changed = true;
}

/* Every entry of the list has character b, the first's. */
static bool agree(const uint16_t b)
{
    const Entry *const first = &ac.list[0];
    if ((uint8_t)b >= first->len)
        return false;
    for (uint16_t i = 0; i < ac.n; i++) {
        const Entry *const e = &ac.list[i];
        if ((uint8_t)b >= e->len || e->text[b] != first->text[b])
            return false;
    }
    return true;
}

/* ac_common: the word as long as all of the list agree: those letters put in. */
static void common(void)
{
    uint16_t b = ac.pn;
    while (agree(b))
        b++;
    if (b != ac.pn)
        put(ac.list[0].text, b);
}

/* The arguments of function tok, else x. */
static const char *args_of(const uint8_t tok)
{
    for (uint32_t i = 0; i < countof(func_args); i++)
        if (func_args[i].tok == tok)
            return func_args[i].args;
    return "x";
}

/* ac_sig: the form of function entry e in ac.sig: LEFT$(s$, n). */
static void signature(const Entry *const e)
{
    uint32_t n = e->len;
    memcpy(ac.sig, e->text, n);
    if (ac.sig[n - 1] != '(')
        ac.sig[n++] = '(';
    const char *const a = args_of((uint8_t)e->xt);
    const uint32_t an = (uint32_t)strlen(a);
    memcpy(ac.sig + n, a, an);
    n += an;
    ac.sig[n++] = ')';
    ac.sig[n] = 0;
}

/* Function entry e's form on the status bar. */
static void show_form(const Entry *const e)
{
    signature(e);
    uimn.help = ac.sig;
}

/* ac_ins: entry i completes the word; a function gets its '(' and its form on the status bar. */
static void insert(const uint16_t i)
{
    const Entry *const e = &ac.list[i];
    put(e->text, e->len);
    if (e->kind != AK_FN)
        return;
    if (e->text[e->len - 1] != '(')
        edit_lb_insert(&uimn.edline, (const uint8_t *)"(", 1);
    show_form(e);
}

/* ac_prev: label entry e's line after the label, at row, col + PV_COL in the box. */
static void preview(const Entry *const e, const uint8_t row, const uint8_t col)
{
    const uint8_t *const p = line_body(e->xt);
    uint32_t off = 0; /* its text from the item after the label */
    if (p[0] == M_INT) {
        if (!p[INT_SZ])
            return;
        off = lexr.col[INT_SZ];
    }
    const uint8_t *const s = uimn.build + off;
    const uint16_t len = (uint16_t)strlen((const char *)s); /* its length, to the box's edge */
    scrn_strn(row, (uint8_t)(col + PV_COL), s, len > PV_LEN ? PV_LEN : len, SL_BAR);
}

/* ac_draw: the list's box, ac.sel lit. */
static void draw(void)
{
    scrn_box(ac.at_row, ac.at_col, ac.bn + BOX_FRAME, ac.bw, SL_BAR, nullptr);
    scrn_shadow(ac.at_row, ac.at_col, ac.bn + BOX_FRAME, ac.bw);
    for (uint8_t b = 0; b < ac.bn; b++) {
        const uint8_t row = ac.at_row + b + 1, col = ac.at_col + NAME_COL;
        const Entry *const e = &ac.list[ac.top + b];
        scrn_strn(row, col, e->text, e->len, SL_BAR);
        if (e->kind == AK_LB)
            preview(e, row, col);
        if (ac.top + b == ac.sel)
            scrn_attr(row, col - 1, ac.bw - 2, SL_DSEL);
    }
}

/* The list's box under the word, else over it; the first lit. */
static void place(void)
{
    ac.sel = 0;
    ac.top = 0;
    uint8_t bw = NAME_PAD; /* the box: the longest name and NAME_PAD */
    for (uint16_t i = 0; i < ac.n; i++) {
        const Entry *const e = &ac.list[i];
        const uint8_t w = e->kind == AK_LB ? AC_PREV : e->len + NAME_PAD; /* a label: its line */
        if (w > bw)
            bw = w;
    }
    ac.bw = bw;
    ac.bn = (uint8_t)(ac.n > AC_ROWS ? AC_ROWS : ac.n);
    const uint8_t h = ac.bn + BOX_FRAME;
    uint8_t row = (uint8_t)(uimn.ed.line - uimn.ed.top + uimn.ed.row0 + 1); /* under, else over */
    if ((uint8_t)(row + h) > STATUS_ROW)
        row = row - h - 1;
    const uint16_t c = ac.ps > uimn.ed.left ? ac.ps - uimn.ed.left : 0;
    uint8_t col = (uint8_t)c;
    if ((uint8_t)(col + bw) > PSCR_COLS)
        col = PSCR_COLS - bw;
    ac.at_row = row;
    ac.at_col = col;
}

/* A mouse event in the list: a click on a name takes it, one outside leaves. */
static Step on_mouse(void)
{
    if (uimn.mouse.event != MS_PRESS)
        return STEP_STAY;
    const uint8_t r = uimn.mouse.row - ac.at_row;
    const uint8_t x = uimn.mouse.col - ac.at_col;
    if (uimn.mouse.row <= ac.at_row || r > ac.bn || uimn.mouse.col <= ac.at_col || x >= ac.bw)
        return STEP_LEAVE;
    ac.sel = (uint16_t)(r - 1 + ac.top);
    return STEP_TAKE;
}

static bool is_move(const uint8_t scan)
{
    return scan == SCAN_UP || scan == SCAN_DOWN || scan == SCAN_PGUP || scan == SCAN_PGDN;
}

/* An arrow or page key: ac.sel moved, the list scrolled to it. */
static void move(const uint8_t scan)
{
    int32_t s = ac.sel;
    if (scan == SCAN_UP)
        s = s ? s - 1 : 0;
    else if (scan == SCAN_PGUP)
        s = s >= AC_ROWS ? s - AC_ROWS : 0;
    else {
        s += scan == SCAN_PGDN ? AC_ROWS : 1;
        if (s >= ac.n)
            s = ac.n - 1;
    }
    ac.sel = (uint16_t)s;
    if (s < ac.top)
        ac.top = (uint16_t)s;
    if (s >= ac.bn + ac.top)
        ac.top = (uint16_t)(s - (AC_ROWS - 1));
}

/* key in the list: Enter or Tab takes, Esc leaves, another key leaves it typed as ever. */
static Step on_key(const uint16_t key)
{
    if (key == KEY_MOUSE)
        return on_mouse();
    const uint8_t ch = (uint8_t)key, scan = (uint8_t)(key >> 8);
    if (ch == CH_ENTER || ch == CH_TAB)
        return STEP_TAKE;
    if (ch == CH_ESC)
        return STEP_LEAVE;
    if (ch || !is_move(scan)) {
        uimn_key(key);
        return STEP_LEAVE;
    }
    move(scan);
    return STEP_STAY;
}

/* ac_pop: the list under the word: arrows choose, Enter or Tab takes, Esc or another key leaves
   it, the key then typed. */
static void popup(void)
{
    place();
    Step step;
    do {
        uimn.help = nullptr; /* a function's form on the status bar */
        if (ac.list[ac.sel].kind == AK_FN)
            show_form(&ac.list[ac.sel]);
        draw_all();
        draw();
        step = on_key(uimn_getkey());
    } while (step == STEP_STAY);
    uimn.help = nullptr;
    if (step == STEP_TAKE)
        insert(ac.sel);
}

bool cmpl_tab(void)
{
    if (!word() || !context())
        return false;
    ac.n = 0;
    ac.on[0] = 0;
    if (ac.m == AM_VR && ac.st == TK_NEXT) { /* NEXT: the open FOR's variable */
        open_for();
        if (!ac.pn) /* no word: that alone */
            add(ac.on + 1, ac.on[0], AK_VR, 0, 0);
        else {
            keys();
            vars();
        }
    } else {
        if (!ac.pn && !(ac.m & AM_LB) && /* no word: labels, or what has the type wanted */
            (ac.ty == AT_ANY || !(ac.m & (AM_FN | AM_VR))))
            return false;
        keys();
        vars();
    }
    if (!ac.n)
        return false;
    common();
    if (ac.n == 1)
        insert(0);
    else
        popup();
    return true;
}
#endif
