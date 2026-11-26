/* Undo and redo of the editor's changes to the text.
 *
 * # How it works
 * Each command, a key or a mouse event, is one step: from undo_begin to undo_end. text.c tells each
 * line it replaces, inserts or deletes (uimn.text.edits); the step keeps each change: its line in the
 * whole text, its file, its old text and its new, and whether each was kept as typed. Undo plays a
 * step's changes back in reverse through text_place and text_remove, Redo forward; then the views
 * are made again and the cursor goes where the step began, or ended.
 *
 * The cursor's line is edited in uimn.edline and stored only when the cursor leaves it. A command
 * that changed it alone is a line step: the line as it was before. Typing goes on in one such step
 * till a word starts, deleting till something else is done. Once the line is stored, its line steps
 * go: the store's change is the step.
 *
 * NSTEP steps, KEEP bytes of their text, are kept; the oldest go first. A change after an undo drops
 * what could be redone.
 */
#include "ui/undo.h"
#include "util/grow.h"
#include "ui/edit.h"
#include "ui/linemap.h"
#include "ui/text.h"
#include "ui/view.h"

enum { NSTEP = 256, KEEP = 0x10000 };

typedef enum : uint8_t { OP_REPL, OP_INS, OP_DEL } OpKind;
/* A change: then was_len bytes of the old text, now_len of the new. */
typedef struct {
    OpKind kind;
    bool was_asis, now_asis;
    uint16_t file;
    uint32_t line;
    uint16_t was_len, now_len;
} Op;

/* The cursor: its line in the whole text, its column. */
typedef struct {
    uint32_t line;
    uint16_t col;
} Spot;

/* A step: its ops, or a line step's line as it was; the cursor before and after. */
typedef struct {
    bool in_line;
    Spot from, to;
    uint32_t len;
    uint8_t *data;
} Step;

/* Its steps, the oldest first: grown as pushed, NSTEP at most. */
typedef struct {
    Step *s;
    uint32_t n, cap;
} Stack;

/* What a line step's command did, for its next to run into it. */
typedef enum : uint8_t { K_NONE, K_TYPE, K_DEL, K_OTHER } Kind;

static struct {
    bool acting;   /* an undo or redo ran in the command: no step of it */
    Stack done;    /* to undo, the last on top */
    Stack undone;  /* to redo */
    uint32_t kept; /* done's bytes */
    Kind kind;     /* the last line step's */
    uint8_t typed; /* its character, K_TYPE's */
    /* The command's: its changes so far, the cursor and its line when it began. */
    uint8_t *ops;
    uint32_t ops_len, ops_cap;
    Spot from;
    uint8_t *line;
    uint32_t line_cap;
    uint16_t line_len;
    LineBuf was, now; /* a line's text before it changes, after */
    TextKind was_kind;
    uint32_t *at; /* replay's: each op's offset */
    uint32_t at_cap;
} un = {.was = {.grows = true}, .now = {.grows = true}};

static Spot spot(void) { return (Spot){uimn.ed.line + uimn.text.first, uimn.edline.col}; }

static void drop_step(Step *const s)
{
    pmem_free(s->data);
    *s = (Step){};
}

/* The stack's step i out. */
static void take_out(Stack *const st, const uint32_t i)
{
    if (st == &un.done)
        un.kept -= st->s[i].len;

    drop_step(&st->s[i]);
    memmove(&st->s[i], &st->s[i + 1], (st->n - i - 1) * sizeof st->s[0]);
    st->n--;
}

static void empty(Stack *const st)
{
    while (st->n)
        take_out(st, st->n - 1);
}

/* s on st, the oldest out past NSTEP, or past KEEP bytes done. No memory: s not kept. */
static void push(Stack *const st, Step s)
{
    if (st->n == NSTEP)
        take_out(st, 0);

    if (!GROW(st->s, st->cap, st->n + 1)) {
        drop_step(&s);
        return;
    }

    st->s[st->n++] = s;

    if (st != &un.done)
        return;

    un.kept += s.len;

    while (un.kept > KEEP && st->n > 1)
        take_out(st, 0);
}

static Step pop(Stack *const st)
{
    const Step s = st->s[--st->n];

    if (st == &un.done)
        un.kept -= s.len;

    st->s[st->n] = (Step){};

    return s;
}

static Step *top(Stack *const st) { return st->n ? &st->s[st->n - 1] : nullptr; }

/* A step of n bytes from p, the cursor from and to; its data null: no memory. */
static Step step_of(
    const bool in_line,
    const Spot from,
    const Spot to,
    const uint8_t *const p,
    const uint32_t n
)
{
    uint8_t *const data = pmem_resize(nullptr, n ? n : 1);

    if (data)
        memcpy(data, p, n);

    return (Step){in_line, from, to, data ? n : 0, data};
}

/* The line steps on st, the top ones, out. */
static void drop_line_steps(Stack *const st)
{
    while (st->n && top(st)->in_line)
        take_out(st, st->n - 1);
}

void undo_clear(void)
{
    empty(&un.done);
    empty(&un.undone);
    un.ops_len = 0;
    un.kind = K_NONE;
}

/* Op o and its texts at the end of the command's ops. Out of memory: none kept. */
static void add_op(const Op o, const uint8_t *const was, const uint8_t *const now)
{
    const uint32_t n = un.ops_len + sizeof o + o.was_len + o.now_len;

    if (!GROW(un.ops, un.ops_cap, n))
        return;

    uint8_t *d = un.ops + un.ops_len;
    memcpy(d, &o, sizeof o);
    memcpy(d += sizeof o, was, o.was_len);
    memcpy(d + o.was_len, now, o.now_len);
    un.ops_len = n;
}

/* Text line line's text to b; its length. */
static uint16_t line_text(Session *const S, const uint32_t line, LineBuf *const b)
{
    return (uint16_t)text_record(S, lmap_off(S, line), b);
}

/* text.c's: text line line's change e kept. */
static void edits(Session *const S, const uint32_t line, const uint8_t e)
{
    LineBuf *const was = &un.was;

    if (e == TE_REPLACING || e == TE_DELETING) {
        was->len = line_text(S, line, was);
        un.was_kind = text_kind(S, line);

        if (e == TE_REPLACING)
            return;

        add_op(
            (Op){OP_DEL, un.was_kind.asis, false, un.was_kind.file, line, was->len, 0},
            was->text,
            nullptr
        );

        return;
    }

    const TextKind k = text_kind(S, line);
    const uint16_t n = line_text(S, line, &un.now);
    const bool repl = e == TE_REPLACED;
    const Op o = {
        repl ? OP_REPL : OP_INS,
        repl && un.was_kind.asis,
        k.asis,
        k.file,
        line,
        repl ? was->len : 0,
        n
    };

    add_op(o, was->text, un.now.text);
}

void undo_init(void) { uimn.text.edits = edits; }

void undo_begin(Session *const S)
{
    un.acting = false;
    un.ops_len = 0;
    un.from = spot();
    const LineBuf *const b = &uimn.edline;

    if (GROW(un.line, un.line_cap, b->len + 1u)) {
        memcpy(un.line, b->text, b->len);
        un.line_len = b->len;
    } else
        un.line_len = UINT16_MAX; /* none kept: no line step */
}

/* The command's change to the line from (before) to b: K_TYPE, its character to *ch; K_DEL. */
static Kind kind_of(const LineBuf *const b, const Spot from, uint8_t *const ch)
{
    const uint16_t len = un.line_len, col = from.col;

    if (b->col == col + 1 && (b->len == len + 1 || b->len == len)) {
        *ch = b->text[col];
        return K_TYPE;
    }

    return b->len + 1 == len && (b->col == col || b->col + 1 == col) ? K_DEL : K_OTHER;
}

/* A line step of kind k, its character ch, goes on in the one on top. */
static bool runs_on(const Kind k, const uint8_t ch, const uint32_t line)
{
    const Step *const t = top(&un.done);

    if (!t || !t->in_line || t->from.line != line || k != un.kind || k == K_OTHER)
        return false;

    return k == K_DEL || !is_word_char(ch) || is_word_char(un.typed);
}

/* The command changed the line alone: a line step, unless it runs on in the last. */
static void line_step(const Spot to)
{
    const LineBuf *const b = &uimn.edline;
    uint8_t ch = 0;
    const Kind k = kind_of(b, un.from, &ch);

    if (!runs_on(k, ch, to.line))
        push(&un.done, step_of(true, un.from, to, un.line, un.line_len));

    empty(&un.undone);
    un.kind = k;
    un.typed = ch;
}

void undo_end(Session *const S)
{
    if (un.acting) {
        un.acting = false;
        un.kind = K_NONE;
        return;
    }

    const Spot to = spot();
    const LineBuf *const b = &uimn.edline;

    if (un.ops_len) { /* the store changed: one step */
        drop_line_steps(&un.done);
        push(&un.done, step_of(false, un.from, to, un.ops, un.ops_len));
        empty(&un.undone);
        un.ops_len = 0;
        un.kind = K_NONE;
        return;
    }

    if (to.line != un.from.line) { /* another line: the last one's steps gone */
        drop_line_steps(&un.done);
        drop_line_steps(&un.undone);
    }

    const bool same = un.line_len == b->len && !memcmp(un.line, b->text, b->len);

    if (to.line == un.from.line && un.line_len != UINT16_MAX && !same) {
        line_step(to);
        return;
    }

    un.kind = K_NONE; /* a move or another command ends a run of typing */
}

/* The line step of the cursor's line now, the cursor to. */
static Step line_now(void)
{
    const LineBuf *const b = &uimn.edline;
    return step_of(true, spot(), spot(), b->text, b->len);
}

/* The cursor's line as line step s left it. */
static void line_back(Session *const S, const Step *const s)
{
    LineBuf *const b = &uimn.edline;
    memcpy(edit_lb_need(S, b, s->len), s->data, s->len);
    b->len = (uint16_t)s->len;
    b->col = s->from.col;
    const uint16_t n = (uint16_t)text_get(S, uimn.ed.line, &uimn.line);
    b->dirty = n != b->len || memcmp(uimn.line.text, b->text, n);
}

/* Step s's ops played back (undo), else forward; the views made again. */
static void replay(Session *const S, const Step *const s, const bool back)
{
    uint32_t n = 0;

    for (uint32_t off = 0; off < s->len; n++) {
        if (!GROW(un.at, un.at_cap, n + 1))
            return;

        Op o;
        memcpy(&o, s->data + off, sizeof o);
        un.at[n] = off;
        off += sizeof o + o.was_len + o.now_len;
    }

    void (*const hook)(uint32_t, int32_t) = uimn.text.hook;
    uimn.text.hook = nullptr;

    for (uint32_t i = 0; i < n; i++) {
        Op o;
        const uint8_t *const p = s->data + un.at[back ? n - 1 - i : i];
        memcpy(&o, p, sizeof o);
        const uint8_t *const was = p + sizeof o, *const now = was + o.was_len;
        const bool removes = back ? o.kind == OP_INS : o.kind == OP_DEL;
        text_whole();

        if (removes) {
            text_remove(S, o.line);
            continue;
        }

        const bool old = back && o.kind != OP_INS;
        const TextKind k = {o.file, old ? o.was_asis : o.now_asis};

        text_place(S, o.line, old ? was : now, old ? o.was_len : o.now_len, k, o.kind != OP_REPL);
    }

    uimn.text.hook = hook;
    view_rebuild(S);
}

/* The cursor to p, in its view. */
static void go(Session *const S, const Spot p)
{
    const uint32_t line = p.line < uimn.text.all ? p.line : uimn.text.all - 1;
    edit_load(S, view_goline(S, line));
    uimn.edline.col = p.col;
    uimn.ed.sel_on = false;
    edit_ensure();
}

bool undo_undo(Session *const S)
{
    const uint32_t here = spot().line;
    Step *const t = top(&un.done);
    un.acting = true;

    if (t && t->in_line && t->from.line == here) {
        const Step s = pop(&un.done);
        push(&un.undone, line_now());
        line_back(S, &s);
        pmem_free(s.data);
        return true;
    }

    if (uimn.edline.dirty) { /* no step: the line as stored */
        push(&un.undone, line_now());
        edit_load(S, uimn.ed.line);
        return true;
    }

    drop_line_steps(&un.done);

    if (!un.done.n)
        return false;

    const Step s = pop(&un.done);
    replay(S, &s, true);
    go(S, s.from);
    push(&un.undone, s);
    return true;
}

bool undo_redo(Session *const S)
{
    Step *const t = top(&un.undone);
    un.acting = true;

    if (!t)
        return false;

    if (t->in_line) {
        const Step s = pop(&un.undone);
        push(&un.done, line_now());
        line_back(S, &s);
        pmem_free(s.data);
        return true;
    }

    if (uimn.edline.dirty) /* a change since: nothing to redo (it emptied the steps) */
        return false;

    const Step s = pop(&un.undone);
    replay(S, &s, false);
    go(S, s.to);
    push(&un.done, s);
    return true;
}
