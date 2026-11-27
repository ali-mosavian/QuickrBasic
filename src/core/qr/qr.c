/* Quickr, the modern dialect: a Quickr file (its first statement qr_dialect) and the modules it imports loaded, checked, linked and
 * run.
 *
 * # How it works
 * Each source line becomes a line of the program store (LF_TEXT), its body the line's text, so the
 * editor, an error's place and the debugger work line by line as for QBasic. The front end then
 * runs in stages, each over the whole file: lex (qlex), parse (qpar), check (qsem). Every error
 * any stage finds is reported, in the source's order, before anything runs:
 *
 *   prog.bas:3:7: unknown name y
 *     print y
 *           ^
 *
 * Each import loads its module's file once (found by io/locate.c, a .bas whose first statement is qr_dialect): its lines, its own
 * tokens and its statements, after the main file's. The store holds a module's lines before those
 * of the modules importing it, the main's last, so the modules' vars start in that order.
 *
 * A clean program is linked (qlink) and run by the VM as a QBasic one is. A runtime error stops
 * it at its line: prog.bas:5: Division by zero, then the line.
 *
 * The editor keeps a program's lines in the store as it reads them, each file's number its
 * module's, the main's first (ui/text.h); a module is read in after the rest. A run's lines, a
 * generic's instances and the end of the run past the main's last line, go after them (LF_INC)
 * and out again before the next build. The run goes through the modules in the order above, each
 * module's last line on to the next's first (qlink).
 */
#include "qr/qr.h"
#include "qr/qr_int.h"
#include "session.h"
#include "comp/export.h"
#include "comp/link.h"
#include "io/repl.h"
#include "io/errmsg.h"
#include "io/locate.h"
#include "plat/pfil.h"
#include "qbrt/qbrt.h"
#include "qr/qast.h"
#include "qr/qlex.h"
#include "qr/qlink.h"
#include "qr/qpar.h"
#include "qr/qrt.h"
#include "qr/qsem.h"
#include "comp/bind.h"
#include "comp/blocks.h"

enum { QR_CHUNK = 0x10000 }; /* the front end's arena's blocks at least */

const char qr_dialect[] = QLEX_MARK;

bool qr_marker(const char *const text, const uint32_t len) { return qlex_mark(text, len); }

bool qr_blank(const char *const text, const uint32_t len) { return qlex_blank(text, len); }

bool qr_is_file(const char *const path)
{
    PfilHandle h;

    if (pfil_open(path, PFIL_READ, &h))
        return false;

    char buf[512], line[2 * sizeof qr_dialect]; /* a line's first bytes: enough for the mark */
    uint32_t got = 0, n = 0;
    bool stated = false, marked = false; /* a statement met, and is it the mark? */

    while (!stated && !pfil_read(h, buf, sizeof buf, &got) && got)
        for (uint32_t i = 0; i < got && !stated; i++) {
            if (buf[i] != '\n') {
                if (n < sizeof line)
                    line[n++] = buf[i];

                continue;
            }

            stated = !qlex_blank(line, n);
            marked = stated && qlex_mark(line, n);
            n = 0;
        }

    pfil_close(h);
    return stated ? marked : qlex_mark(line, n); /* the last line's, no LF after it */
}

/* A platform error dos, if any, raised as QBasic's. */
static void qr_check(Session *const S, const uint16_t dos)
{
    if (dos)
        fault_check(&S->fault, chnl_doserr(dos));
}

/* The bytes of the file at path in the arena, a 0 after them; *len how many. PFIL_OK, else the DOS
   error. */
static uint16_t qr_bytes(QMem *const m, const char *const path, char **const text, uint32_t *len)
{
    PfilHandle h;
    const uint16_t e = pfil_open(path, PFIL_READ, &h);

    if (e)
        return e;

    uint32_t size = 0, got = 0;
    pfil_seek(h, 0, PFIL_END, &size);
    pfil_seek(h, 0, PFIL_SET, nullptr);
    *text = qast_alloc(m, size + 1);
    const uint16_t st = pfil_read(h, *text, size, &got);
    pfil_close(h);
    *len = got;
    return st;
}

/* The line of text at *at, len bytes in all, its end (LF, CR LF) a 0 in place; *at past it. Its
   length. */
static uint32_t qr_line(char *const text, const uint32_t len, uint32_t *const at)
{
    uint32_t end = *at;

    while (end < len && text[end] != '\n')
        end++;

    const uint32_t w = end - *at - (end > *at && text[end - 1] == '\r');
    text[*at + w] = 0;
    *at = end + 1;
    return w;
}

/* A new module of p at path, its lines next: its number. */
static uint32_t qr_newmod(QProg *const p, QMem *const m, const char *const path)
{
    const uint32_t k = p->nmods;
    const uint32_t n = (uint32_t)strlen(path);
    char *const name = qast_alloc(m, n + 1);
    memcpy(name, path, n);
    p->mods = qast_room(m, p->mods, p->nmods, &p->capmods, sizeof *p->mods);
    p->nmods++;
    p->mods[k] = (QMod){.path = name, .first = p->src.nlines};

    if (k)
        p->mods[k].base = p->mods[k - 1].base + p->mods[k - 1].ntoks;

    return k;
}

/* ln a line of p's, the last; *cap the lines' room. */
static void qr_addline(QProg *const p, QMem *const m, uint32_t *const cap, const QLine ln)
{
    p->src.lines = qast_room(m, p->src.lines, p->src.nlines, cap, sizeof *p->src.lines);
    p->src.lines[p->src.nlines++] = ln;
}

/* Module k, its lines p's last: lexed. */
static QMod *qr_lex(QProg *const p, QMem *const m, const uint32_t k)
{
    QMod *const mod = &p->mods[k];
    mod->nlines = p->src.nlines - mod->first;
    qlex_source(&p->src, mod->first, m, &p->errs);
    mod->toks = p->src.toks;
    mod->ntoks = p->src.ntoks;
    return mod;
}

/* Module k, its lines p's last: lexed and parsed. */
static void qr_parse(QProg *const p, QMem *const m, const uint32_t k)
{
    qpar_program(p, m, qr_lex(p, m, k));
}

/* The file at path a new module of p: its lines, lexed and parsed. Its number. */
static uint32_t qr_module(Session *const S, QProg *const p, QMem *const m, const char *const path)
{
    uint32_t len, cap = p->src.nlines;
    char *text;
    qr_check(S, qr_bytes(m, path, &text, &len));
    const uint32_t k = qr_newmod(p, m, path);

    for (uint32_t at = 0, src = 1; at < len; src++) {
        const uint32_t start = at;
        const uint32_t w = qr_line(text, len, &at);
        qr_addline(
            p,
            m,
            &cap,
            (QLine){
                .text = text + start,
                .len = w,
                .src = (uint16_t)src,
                .mod = (uint16_t)k,
            }

        );
    }

    qr_parse(p, m, k);
    return k;
}

bool qr_text(const Line *const l, const uint32_t file)
{
    return (l->flags & (LF_TEXT | LF_INC)) == LF_TEXT && l->file == file;
}

/* The store's lines of file file before record upto, a new module of p at path: each's text
   copied, its src its number in the file. Its number. */
static uint32_t qr_copy_lines(
    Session *const S,
    QProg *const p,
    QMem *const m,
    const uint32_t file,
    const char *const path,
    const uint32_t upto
)
{
    Prog *const pg = &S->prog;
    const uint32_t k = qr_newmod(p, m, path);
    uint32_t cap = p->src.nlines, src = 1;

    for (uint32_t rec = PROGBASE; rec < upto && prog_line(pg, rec)->len;

         rec += prog_line(pg, rec)->len) {
        Line *const l = prog_line(pg, rec);

        if (!qr_text(l, file))
            continue;

        const char *const body = (const char *)at(prog_addr(pg, rec) + HDR);
        const uint32_t n = (uint32_t)strlen(body);
        char *const text = qast_alloc(m, n + 1);
        memcpy(text, body, n);
        l->src = (uint16_t)src;
        qr_addline(
            p,
            m,
            &cap,
            (QLine){
                .at = rec,
                .text = text,
                .len = n,
                .src = (uint16_t)src,
                .mod = (uint16_t)k,
            }

        );
        src++;
    }

    return k;
}

/* The store's lines of file file, a new module of p at path: each's text copied, its src its
   number in the file; lexed and parsed. */
static void qr_gather(
    Session *const S,
    QProg *const p,
    QMem *const m,
    const uint32_t file,
    const char *const path
)
{
    qr_parse(p, m, qr_copy_lines(S, p, m, file, path, UINT32_MAX));
}

QProg qr_loose(QMem *const m)
{
    return (QProg){.errs = {.mem = m}, .copies = {.mem = m}, .loose = true};
}

QParAt *qr_upto(
    Session *const S,
    QProg *const p,
    QMem *const m,
    const uint32_t file,
    const uint32_t rec
)
{
    *p = qr_loose(m);
    return qpar_upto(p, m, qr_lex(p, m, qr_copy_lines(S, p, m, file, "", rec)));
}

/* The line at text, n bytes, its tabs spaces to the next column of 8: in the arena, 0-ended;
   *w its length. */
static const char *qr_untab(QMem *const m, const char *const text, const uint32_t n, uint32_t *w)
{
    uint32_t tabs = 0;

    for (uint32_t i = 0; i < n; i++)
        tabs += text[i] == '\t';

    char *const out = qast_alloc(m, n + tabs * 7 + 1);
    uint32_t o = 0;

    for (uint32_t i = 0; i < n; i++) {
        if (text[i] != '\t') {
            out[o++] = text[i];
            continue;
        }

        do
            out[o++] = ' ';
        while (o % 8);
    }

    out[o] = 0;
    *w = o;
    return out;
}

/* qr_read's, its lines before record at (0: at the store's end). */
static uint16_t qr_read_at(
    Session *const S,
    const char *const path,
    const uint16_t file,
    const uint32_t at
)
{
    QMem m = {.fault = &S->fault};

    arena_init(&m.arena, QR_CHUNK);
    char *text;
    uint32_t len;
    const uint16_t e = qr_bytes(&m, path, &text, &len);
    const char *const eof = e ? nullptr : memchr(text, 0x1A, len);

    if (eof)
        len = (uint32_t)(eof - text);

    S->prog.add_flags = LF_TEXT;
    S->prog.add_file = file;
    S->prog.add_at = at;

    for (uint32_t at = 0, src = 1; !e && (at < len || src == 1); src++) {
        const uint32_t start = at;
        uint32_t w;
        const char *const line = qr_untab(&m, text + start, qr_line(text, len, &at), &w);
        S->prog.add_src = (uint16_t)src;
        prog_add(&S->prog, NONUM, (const uint8_t *)line, w + 1);
    }

    S->prog.add_flags = 0;
    S->prog.add_src = 0;
    S->prog.add_file = 0;
    S->prog.add_at = 0;
    arena_reset(&m.arena);
    return e;
}

uint16_t qr_read(Session *const S, const char *const path, const uint16_t file)
{
    return qr_read_at(S, path, file, 0);
}

/* The store's first LF_TOP: its QBasic modules' lines from there, after the Quickr ones; 0 none. */
static uint32_t qr_first_top(const Prog *const pg)
{
    for (uint32_t rec = PROGBASE; prog_line(pg, rec)->len; rec += prog_line(pg, rec)->len)
        if (prog_line(pg, rec)->flags & LF_TOP)
            return rec;

    return 0;
}

/* Are paths a and b the same, case aside? */
static bool qr_same(const char *a, const char *b)
{
    while (*a && downcase((uint8_t)*a) == downcase((uint8_t)*b))
        a++, b++;

    return !*a && !*b;
}

/* A program being loaded: its tree's, and whether its lines are the store's (the editor's), a
   module read in after them; else each file's text is read into the arena. Each module of p is
   the program's of its number (mem/module.h). A QBasic program's Quickr modules: guest, each read
   already, p's numbers their own; find, module from's imports found, each the program's. */
typedef struct {
    Session *S;
    QProg *p;
    QMem *m;
    bool stored;
    bool guest, find;
    uint32_t from;
} QrLoad;

/* A new module of p at path, QBasic's: its exports later (qr_exports). Its number. */
static uint32_t qr_qbmod(QProg *const p, QMem *const m, const char *const path)
{
    const uint32_t k = qr_newmod(p, m, path);
    p->mods[k].ex = qast_alloc(m, sizeof(Exports));
    return k;
}

/* A QBasic program's guest's import of the QBasic module at path, a native's too: a module of p,
   its exports taken at its stage (qr_stage). Its number. */
static uint32_t qr_guest_qbmod(const QrLoad *const L, const char *const path)
{
    Modu *const mu = &L->S->modu;
    const uint32_t mm = modu_module(mu, path);
    const ModuMod *const mod = modu_mod(mu, mm);
    const uint32_t k = qr_qbmod(L->p, L->m, path);
    L->p->mods[k].modu = mm;
    L->p->mods[k].start = mod->top + prog_line(&L->S->prog, mod->top)->len;
    return k;
}

/* The program's module k, Quickr's (mem/module.h): its lines file's. */
static void qr_ours(Session *const S, const uint32_t k, const uint16_t file)
{
    ModuMod *const mod = modu_mod(&S->modu, k);
    mod->dialect = &dialect_quickr;
    mod->file = file;
}

/* The program's module k, p's next: a QBasic one's, its lines the QBasic loader's (qr_guests,
   qr_qbasic); else read in, into the store when L->stored, its file numbered as the module there
   (else by qr_store). Its number; QMOD_NONE when the editor cannot read it. */
static uint32_t qr_add(const QrLoad *const L, const uint32_t k)
{
    Session *const S = L->S;
    char path[PFIL_PATH]; /* the table may move as it grows */
    strcpy(path, modu_module_path(&S->modu, k));

    if (!qr_is_file(path))
        qr_qbmod(L->p, L->m, path);
    else if (!L->stored)
        qr_module(S, L->p, L->m, path);
    else if (qr_read_at(S, path, (uint16_t)k, qr_first_top(&S->prog))) {
        modu_drop(&S->modu, k);
        return QMOD_NONE;
    } else {
        modu_file(&S->modu, k, path, k);
        qr_gather(S, L->p, L->m, k, path);
    }

    if (qr_is_file(path))
        qr_ours(S, k, (uint16_t)k);

    L->p->mods[k].modu = k;
    return k;
}

/* The module at path: p's of that path, else read in, a new module. Its number; QMOD_NONE when
   the editor cannot read it. */
static uint32_t qr_load(const QrLoad *const L, const char *const path)
{
    QProg *const p = L->p;

    for (uint32_t i = 0; i < p->nmods; i++)
        if (qr_same(p->mods[i].path, path))
            return i;

    if (L->find) { /* the QBasic loader reads it, a QBasic one's and a native's too */
        repl_import(L->S, L->from, path);
        return QMOD_NONE;
    }

    if (L->guest && !qr_is_file(path))
        return qr_guest_qbmod(L, path);
    if (L->guest)
        return QMOD_NONE;

    const uint32_t k = repl_module(L->S, path);
    return k < p->nmods ? k : qr_add(L, k); /* another spelling of its path; else new, p's next */
}

/* Module im, named name (n bytes), not loaded: an error at it, QMOD_NONE its number. */
static void qr_unloaded(
    const QrLoad *const L,
    QImport *const im,
    const char *const name,
    uint32_t n
)
{
    if (im->mod == QMOD_NONE)
        qast_error(&L->p->errs, im->tok, "no module {}", name, n);
}

/* Module k imports im's module: an edge of the program's modules. */
static void qr_edge(const QrLoad *const L, const uint32_t k, const QImport *const im)
{
    if (!L->guest && !L->find && im->mod < modu_count(&L->S->modu))
        modu_import(&L->S->modu, k, im->mod);
}

/* Module im of p, found from module k's file: loaded once, its number in im->mod; a package's
   folder (folder not null) to folder. Not found: an error at its name. */
static void qr_find(const QrLoad *const L, const uint32_t k, QImport *const im, char *const folder)
{
    Session *const S = L->S;
    QProg *const p = L->p;
    char dotted[PFIL_PATH], path[PFIL_PATH];
    uint32_t n = 0;

    for (uint32_t i = 0; i < im->npath; i++) {
        const QTok *const t = im->path[i];

        if (n + t->len + 1 >= sizeof dotted)
            break;
        if (i)
            dotted[n++] = '.';

        memcpy(dotted + n, t->text, t->len);
        n += t->len;
    }

    const ImptName name =
        {.text = (const uint8_t *)dotted, .len = (uint8_t)n, .up = (uint8_t)im->up};

    const Locate where = {
        .main = S->repl.file,
        .lib = syst_env(&S->syst, "BASICLIB"),
        .from = p->mods[k].path,
    };

    bool dir = false;

    if (!locate(&where, &name, folder ? folder : path, folder ? &dir : nullptr)) {
        qast_error(&p->errs, im->tok, "no module {}", n ? dotted : ".", n ? n : 1);
        return;
    }

    if (dir || (folder && natv_package(&S->natv, folder))) { /* a folder, or a .qbx of parts */
        im->folder = true;
        return;
    }

    im->mod = qr_load(L, folder ? folder : path);
    qr_edge(L, k, im);
    qr_unloaded(L, im, dotted, n);
}

/* A name from gives, of the package in folder: its module, loaded once. */
static void qr_part(
    const QrLoad *const L,
    const uint32_t k,
    const char *const dir,
    QImport *const im
)
{
    QProg *const p = L->p;
    char path[PFIL_PATH];
    const ImptName name = {.text = (const uint8_t *)im->tok->text, .len = (uint8_t)im->tok->len};

    if (!locate_in(dir, &name, nullptr, path)) {
        qast_error(&p->errs, im->tok, "no module {} in the package", im->tok->text, im->tok->len);
        return;
    }

    im->mod = qr_load(L, path);
    qr_edge(L, k, im);
    qr_unloaded(L, im, im->tok->text, im->tok->len);
}

/* Each module module k's imports and froms name, loaded once. */
static void qr_imports(const QrLoad *const L, const uint32_t k)
{
    for (QStmt *s = L->p->mods[k].head; s; s = s->next) {
        if (s->kind == QS_IMPORT)
            for (QImport *im = s->imps; im; im = im->next)
                qr_find(L, k, im, nullptr);

        if (s->kind == QS_FROM) {
            char dir[PFIL_PATH];
            qr_find(L, k, s->imps, dir);

            for (QImport *im = s->names; s->imps->folder && im; im = im->next)
                qr_part(L, k, dir, im);
        }

        if (s == L->p->mods[k].tail)
            break;
    }
}

/* Module k after each module it imports, not yet ordered, in p->order from *n. */
static void qr_visit(QProg *const p, bool *const seen, const uint32_t k, uint32_t *const n)
{
    seen[k] = true;

    for (const QStmt *s = p->mods[k].head; s; s = s->next) {
        const QImport *const lists[] = {
            s->kind == QS_IMPORT || s->kind == QS_FROM ? s->imps : nullptr,
            s->kind == QS_FROM ? s->names : nullptr,
        };

        for (uint32_t j = 0; j < countof(lists); j++)
            for (const QImport *im = lists[j]; im; im = im->next)
                if (im->mod != QMOD_NONE && !seen[im->mod])
                    qr_visit(p, seen, im->mod, n);

        if (s == p->mods[k].tail)
            break;
    }

    p->order[(*n)++] = k;
}

/* p->order: each module after those it imports, the main's first met, the main's last. */
static void qr_order(QProg *const p, QMem *const m)
{
    bool *const seen = qast_alloc(m, p->nmods);
    uint32_t n = 0;
    p->order = qast_alloc(m, p->nmods * sizeof *p->order);
    qr_visit(p, seen, 0, &n);
    const uint32_t main = n - 1;

    for (uint32_t k = 1; k < p->nmods; k++)
        if (!seen[k]) /* a QBasic module's import's, or an edited one */
            qr_visit(p, seen, k, &n);

    memmove(p->order + main, p->order + main + 1, (n - main - 1) * sizeof *p->order);
    p->order[n - 1] = 0;
}

/* The program's lines into the store, a module's after those it imports, the main's last: so each
   module's vars start in that order, before the main's first line. Each file numbered after the
   QBasic ones, the main's 0. */
static void qr_store(Session *const S, QProg *const p, QMem *const m)
{
    qr_order(p, m);
    S->prog.add_flags = LF_TEXT;

    for (uint32_t i = 0; i < p->nmods; i++) {
        const QMod *const mod = &p->mods[p->order[i]];

        if (mod->ex)
            continue;

        const uint16_t file = p->order[i] ? (uint16_t)modu_files(&S->modu) : 0;
        modu_file(&S->modu, file, mod->path, 0);
        qr_ours(S, mod->modu, file);
        S->prog.add_file = file;

        for (uint32_t j = mod->first; j < mod->first + mod->nlines; j++) {
            QLine *const ln = &p->src.lines[j];
            ln->at = prog_at(&S->prog);
            S->prog.add_src = ln->src;
            prog_add(&S->prog, NONUM, (const uint8_t *)ln->text, ln->len + 1);
        }
    }

    S->prog.add_flags = 0;
    S->prog.add_src = 0;
    S->prog.add_file = 0;
}

/* Each QBasic module of p's code run from its first line, past its LF_TOP. */
static void qr_starts(Session *const S, QProg *const p)
{
    for (uint32_t k = 0; k < p->nmods; k++) {
        QMod *const mod = &p->mods[k];

        if (!mod->ex)
            continue;

        const uint32_t top = modu_top_of(&S->modu, mod->modu);
        mod->start = top + prog_line(&S->prog, top)->len;
    }
}

/* p's modules, the main's read: each's imports loaded, a Quickr one's read and parsed, a QBasic
   one's read into the store by the QBasic loader (io/repl.c) with its own imports, till none is
   new. */
static void qr_load_all(const QrLoad *const L)
{
    Session *const S = L->S;
    QProg *const p = L->p;
    modu_file(&S->modu, 0, S->repl.file, 0); /* the QBasic files' numbers after */
    qr_ours(S, 0, 0);

    for (uint32_t k = 0, read = 1; k < p->nmods;) {
        for (; k < p->nmods; k++)
            qr_imports(L, k);

        repl_guests(S, read);
        read = modu_count(&S->modu);

        for (uint32_t m = p->nmods; m < read; m++) /* those the QBasic modules import */
            qr_add(L, m);
    }

    qr_starts(S, p);
}

/* Each QBasic module in's, declared, its exports, its procedures' variables declared first
   (comp/link.h); none, nothing. */
static void qr_qbexports(Session *const S, QProg *const p, QMem *const m, const bool *const in)
{
    bool any = false;

    for (uint32_t k = 0; k < p->nmods; k++)
        any |= p->mods[k].ex != nullptr && in[k];

    if (!any)
        return;

    link_paired(&S->comp);

    for (uint32_t k = 0; k < p->nmods; k++) {
        QMod *const mod = &p->mods[k];

        if (!mod->ex || !in[k])
            continue;

        Exports *const x = qast_alloc(m, sizeof *x);
        *x = expo_qbasic(&S->comp, mod->modu, &m->arena);
        mod->ex = x;
    }
}

/* The line text, then a ^ under its byte col, tabs kept so it lines up. */
static void qr_mark(Cons *const co, const char *const text, const uint32_t len, const uint32_t col)
{
    cons_spaces(co, 2);

    for (uint32_t i = 0; i < len; i++)
        cons_putc(co, (uint8_t)text[i]);

    cons_nl(co);
    cons_spaces(co, 2);

    for (uint32_t i = 0; i < col && i < len; i++)
        cons_putc(co, text[i] == '\t' ? '\t' : ' ');

    cons_putc(co, '^');
    cons_nl(co);
}

/* file:line:col: message, then the line marked. */
static void qr_say(Session *const S, const QProg *const p, const QErr *const e)
{
    Cons *const co = &S->cons;
    const QLine *const ln = e->line < p->src.nlines ? &p->src.lines[e->line] : nullptr;
    cons_puts(co, p->mods[ln ? ln->mod : 0].path);
    cons_putc(co, ':');
    cons_uint(co, ln ? ln->src : p->src.nlines);
    cons_putc(co, ':');
    cons_uint(co, e->col + 1);
    cons_puts(co, ": ");

    for (uint32_t i = 0; i < e->len; i++)
        cons_putc(co, (uint8_t)e->text[i]);

    cons_nl(co);

    if (ln)
        qr_mark(co, ln->text, ln->len, e->col);
}

/* Line ln of p's copied, the run's own, at the store's end: its source's mark kept, so a
   breakpoint stops the copy too. Its index. */
static uint32_t qr_copy(Session *const S, QProg *const p, QMem *const m, uint32_t *cap, QLine ln)
{
    const Line *const from = ln.at ? prog_line(&S->prog, ln.at) : nullptr;
    S->prog.add_flags = LF_TEXT | LF_INC | (from ? from->flags & LF_BP : 0);
    S->prog.add_src = ln.src;
    S->prog.add_file = from ? from->file : ln.mod; /* prog_origin finds its line by these */
    ln.at = prog_at(&S->prog);
    prog_add(&S->prog, NONUM, (const uint8_t *)ln.text, ln.len + 1);
    S->prog.add_flags = 0;
    S->prog.add_src = 0;
    S->prog.add_file = 0;
    qr_addline(p, m, cap, ln);
    return p->src.nlines - 1;
}

/* A line of the run's own for statement s, a copy of its token's, at the store's end. */
static void qr_own(Session *const S, QProg *const p, QMem *const m, uint32_t *const cap, QStmt *s)
{
    s->line = qr_copy(S, p, m, cap, p->src.lines[s->tok->line]);
}

/* Each instance's lines, a generic record's and a generic fn's, copies of its generic's at the
   store's end, its statements on them; each block lambda's so too, its lines in the store a
   procedure's own. One code block a line, each copy keeping its source's number and text. */
static void qr_instances(Session *const S, QProg *const p, QMem *const m)
{
    uint32_t cap = p->src.nlines;

    for (uint32_t i = 0; i < p->tys.n; i++) { /* first: a lambda's, a generic method's own after */
        const QTy *const y = &p->tys.at[i];

        if (!y->of)
            continue;

        const QStmt *const from = qast_ty(p, y->of)->def;
        const uint32_t base = p->src.nlines;
        const uint32_t first = from->tok->line;

        for (uint32_t k = first; k <= from->link->tok->line; k++)
            qr_copy(S, p, m, &cap, p->src.lines[k]);

        for (QStmt *s = y->def; s && s != y->def->link->next; s = s->next)
            s->line = base + s->tok->line - first;
    }

    for (const QFn *f = p->fns; f; f = f->next) {
        if (!f->of && !(f->lamof && f->sig))
            continue;

        const QFn *const from = f->of ? f->of : f;
        const uint32_t base = p->src.nlines;
        const uint32_t first = from->def->tok->line;

        for (uint32_t i = first; i <= from->end->tok->line; i++)
            qr_copy(S, p, m, &cap, p->src.lines[i]);

        for (QStmt *s = f->def;; s = s->next) {
            s->line = base + s->tok->line - first;

            if (s == f->end)
                break;
        }
    }

    for (uint32_t i = 0; i < p->nruns; i++) /* last: the run ends there, past every procedure */
        qr_own(S, p, m, &cap, p->runs[i]);

    if (p->atend)
        qr_own(S, p, m, &cap, p->atend);

    if (p->testend) /* it exits; the handlers past it are jumped to */
        qr_own(S, p, m, &cap, p->testend);

    for (uint32_t i = 0; i < p->nruns; i++)
        qr_own(S, p, m, &cap, p->runs[i]->link);
}

/* p, the main's and its modules, checked by stages (mem/module.h), its QBasic modules declared
   between (io/repl.c repl_stages): an error in a module QBasic calls ends it, unless lenient (the
   editor's), its QBasic modules declared still. The exports QBasic calls, by module number. */
static const Exports *qr_stages(Session *S, QProg *p, QMem *m, bool lenient);

uint32_t qr_build(Session *const S)
{
    QMem m = {.fault = &S->fault};

    arena_init(&m.arena, QR_CHUNK);
    QProg p = {.errs = {.mem = &m}, .copies = {.mem = &m}, .testing = S->repl.testing};

    const QrLoad L = {.S = S, .p = &p, .m = &m};

    S->repl.batch = true;
    modu_clear(&S->modu, S->repl.file);
    qr_module(S, &p, &m, S->repl.file);
    qr_load_all(&L);
    qr_store(S, &p, &m);
    const Exports *const ex = qr_stages(S, &p, &m, false);

    for (uint32_t i = 0; i < p.errs.n; i++)
        qr_say(S, &p, &p.errs.at[i]);

    if (!p.errs.n && S->repl.copying) {
        qast_sort(&p.copies);

        for (uint32_t i = 0; i < p.copies.n; i++)
            qr_say(S, &p, &p.copies.at[i]);
    }

    if (!p.errs.n) {
        qr_instances(S, &p, &m);
        qlink_program(&S->comp, &p, &m, ex, S->dbug.emit);
    }

    arena_reset(&m.arena);
    return p.errs.n;
}

void qr_run(Session *const S)
{
    const uint32_t errors = qr_build(S);

    if (errors || S->repl.checking || S->repl.copying)
        cons_exit(&S->cons, errors ? 1 : 0);

    stmt_runfirst(S);
    exec_run(S);
}

void qr_go(Session *const S)
{
    S->vm.stop = 0; /* a key the editor kept */
    music_reset();
    stmt_runfirst(S);
    exec_run(S);
}

const char *qr_message(const Session *const S, const uint8_t code)
{
    return code == E_USR && S->vm.err.user == QRT_ASSERT ? "assert failed"
                                                         : emsg_text(S->vm.err.user, code);
}

void qr_report(Session *const S, const uint8_t code)
{
    Cons *const co = &S->cons;
    const Line *const l = prog_line(&S->prog, S->vm.line);
    const char *const text = (const char *)at(prog_addr(&S->prog, S->vm.line) + HDR);
    cons_puts(co, modu_path(&S->modu, l->file));
    cons_putc(co, ':');
    cons_uint(co, l->src);
    cons_puts(co, ": ");
    cons_puts(co, qr_message(S, code));
    cons_nl(co);
    cons_spaces(co, 2);
    cons_puts(co, text);
    cons_nl(co);
}

/* ---- the editor's */

/* The run's own lines out of the store: a build's. */
static void qr_unrun(Prog *const pg)
{
    for (uint32_t rec = PROGBASE; prog_line(pg, rec)->len;)
        if ((prog_line(pg, rec)->flags & (LF_TEXT | LF_INC)) == (LF_TEXT | LF_INC))
            prog_cut(pg, rec);
        else
            rec += prog_line(pg, rec)->len;
}

/* Module d's lines out of the store, those of the files after it a number down, a QBasic
   module's (qbasic) and its LF_TOP's too (each file its module's); d out of the modules. */
static void qr_drop(Session *const S, const uint32_t d, const bool qbasic)
{
    Prog *const pg = &S->prog;

    if (qbasic)
        repl_cut_module(S, d);

    for (uint32_t rec = PROGBASE; prog_line(pg, rec)->len;) {
        Line *const l = prog_line(pg, rec);

        if (l->flags & LF_TEXT && l->file == d) {
            prog_cut(pg, rec);
            continue;
        }

        if (l->file > d)
            l->file--;

        rec += l->len;
    }

    modu_drop(&S->modu, d);
    modu_relink(&S->modu, S->repl.file, modu_count(&S->modu));
}

/* What the store holds of a module: nothing, Quickr's lines, a QBasic module's after its LF_TOP. */
enum : uint8_t { QR_NONE, QR_LINES, QR_TOP };

/* What the store holds of each module, by number: in the arena. */
static uint8_t *qr_stored(Session *const S, QMem *const m)
{
    const Prog *const pg = &S->prog;
    const uint32_t n = modu_count(&S->modu);
    uint8_t *const held = qast_alloc(m, n);

    for (uint32_t rec = PROGBASE; prog_line(pg, rec)->len; rec += prog_line(pg, rec)->len) {
        const Line *const l = prog_line(pg, rec);

        if (l->flags & LF_TOP && l->file && l->file < n)
            held[l->file] = QR_TOP;
        else if (qr_text(l, l->file) && l->file < n)
            held[l->file] = QR_LINES;
    }

    return held;
}

/* p's QBasic modules (the editor's: each p's number its module's): read into the store when not
   there, each's own imports with it (io/repl.c); each's code run from its first line. */
static void qr_qbasic(const QrLoad *const L, const uint32_t from)
{
    Session *const S = L->S;
    QProg *const p = L->p;
    uint32_t *const mods = qast_alloc(L->m, (p->nmods ? p->nmods : 1) * sizeof *mods);
    uint32_t n = 0;

    for (uint32_t k = 0; k < p->nmods; k++)
        if (p->mods[k].ex)
            mods[n++] = k;

    if (!n && !qr_first_top(&S->prog))
        return;

    repl_guests_stored(S, mods, n, from);
    qr_starts(S, p);
}

/* The Quickr modules the QBasic modules import, p's none: each read into the store. Any? */
static bool qr_new_ours(const QrLoad *const L)
{
    Session *const S = L->S;
    bool any = false;

    for (uint32_t k = L->p->nmods; k < modu_count(&S->modu); k++) {
        if (modu_mod(&S->modu, k)->dialect != &dialect_quickr)
            continue;

        char path[PFIL_PATH]; /* the table may move as it grows */
        strcpy(path, modu_module_path(&S->modu, k));
        qr_check(S, qr_read_at(S, path, (uint16_t)k, qr_first_top(&S->prog)));
        modu_file(&S->modu, k, path, k);
        any = true;
    }

    return any;
}

/* The store's program, repl.file the main's, as L's: the run's own lines out; each Quickr module's
   lines gathered, then the modules their imports name read in after the rest, a Quickr one's
   lines before the QBasic ones'; p->order each module after those it imports, the main's last. A
   Quickr module no import reaches is dropped and the lot gathered again, unless edited: that one
   is the program's still, before the main. A QBasic module is kept (qr_qbasic); a Quickr one a
   QBasic module imports read in, and the lot gathered again. */
static void qr_enter(const QrLoad *const L)
{
    Session *const S = L->S;
    QProg *const p = L->p;
    const QProg fresh = *p;
    qr_unrun(&S->prog);
    modu_relink(&S->modu, S->repl.file, modu_count(&S->modu));
    const uint8_t *const held = qr_stored(S, L->m);
    const uint32_t nheld = modu_count(&S->modu);

    for (uint32_t k = 0; k < nheld; k++) {
        const char *const path = modu_module_path(&S->modu, k);

        if (held[k] == QR_TOP || (k && held[k] == QR_NONE && !qr_is_file(path)))
            qr_qbmod(p, L->m, path);
        else {
            qr_gather(S, p, L->m, k, path);
            qr_ours(S, k, (uint16_t)k);
        }

        p->mods[k].modu = k;
    }

    for (uint32_t k = 0; k < p->nmods; k++)
        qr_imports(L, k);

    qr_order(p, L->m);
    const uint32_t found = modu_count(&S->modu); /* those after, a QBasic module's imports */
    repl_guests_found(S);
    const uint32_t gone = modu_unreached(&S->modu, found);

    if (gone) {                                 /* imported no more: dropped before it is read */
        qr_drop(S, gone, held[gone] == QR_TOP); /* gone < nheld: one added is imported */
        *p = fresh;
        qr_enter(L);
        return;
    }

    qr_qbasic(L, found);

    if (qr_new_ours(L)) { /* a QBasic module's imports: gathered with the rest */
        *p = fresh;
        qr_enter(L);
    }
}

/* Error e of p's to *out: its line's record, its column, its message. One past the lines: the
   last's end. */
static void qr_said(const QProg *const p, const QErr *const e, DialErr *const out)
{
    const bool past = e->line >= p->src.nlines;
    const QLine *const ln =
        p->src.nlines ? &p->src.lines[past ? p->src.nlines - 1 : e->line] : nullptr;
    const uint32_t n = out->cap && e->len >= out->cap ? out->cap - 1 : e->len;
    out->rec = ln ? ln->at : PROGBASE;
    out->col = ln && past ? ln->len : e->col;

    if (!out->cap)
        return;

    memcpy(out->msg, e->text, n);
    out->msg[n] = 0;
}

void qr_open(Session *const S)
{
    QMem m = {.fault = &S->fault};

    arena_init(&m.arena, QR_CHUNK);
    QProg p = {.errs = {.mem = &m}, .copies = {.mem = &m}};

    qr_enter(&(QrLoad){S, &p, &m, .stored = true});

    qr_stages(S, &p, &m, true);
    arena_reset(&m.arena);
}

/* The run's end past the main's last line: an empty line of the run's own, p->end. */
static void qr_end(Session *const S, QProg *const p, QMem *const m)
{
    uint32_t cap = p->src.nlines;
    p->end = qr_copy(S, p, m, &cap, (QLine){.text = ""}) + 1;
}

void qr_forget(QrKeep *const k)
{
    arena_reset(&k->mem.arena);
    qrt_sorts_free(&k->sorts);
    k->prog = nullptr;
    k->said[0] = 0;
}

/* ---- a QBasic program's Quickr modules */

/* Each module module k's imports name, of a file at k's path: found, each the program's. */
static void qr_finds(Session *const S, const uint32_t k, const uint16_t file, const char *path)
{
    QMem m = {.fault = &S->fault};

    arena_init(&m.arena, QR_CHUNK);
    QProg p = {.errs = {.mem = &m}, .copies = {.mem = &m}};

    qr_gather(S, &p, &m, file, path);
    qr_imports(&(const QrLoad){S, &p, &m, .find = true, .from = k}, 0); /* errors: the check's */
    arena_reset(&m.arena);
}

void qr_guest_find(Session *const S, const uint32_t m)
{
    char path[PFIL_PATH]; /* the table may move as it grows */
    strcpy(path, modu_module_path(&S->modu, m));
    qr_finds(S, m, modu_mod(&S->modu, m)->file, path);
}

void qr_guest_read(Session *const S, const uint32_t m)
{
    char path[PFIL_PATH]; /* the table may move as it grows */
    strcpy(path, modu_module_path(&S->modu, m));
    const uint16_t file = S->repl.nfile++;
    modu_file(&S->modu, file, path, m);
    modu_mod(&S->modu, m)->file = file;
    qr_check(S, qr_read(S, path, file));
    qr_finds(S, m, file, path);
}

/* Type t as a QBasic value's (comp/export.h): ET_IMP none; false QBasic has no equal for it. */
static bool qr_et(const QProg *const p, const QType t, uint32_t *const et)
{
    static const uint8_t ets[] = {
        [QY_VOID] = ET_IMP,
        [QY_INT] = ET_I4,
        [QY_SINGLE] = ET_R4,
        [QY_DOUBLE] = ET_R8,
        [QY_STRING] = ET_SD,
        [QY_BOOL] = ET_I2,
        [QY_SHORT] = ET_SH,
        [QY_UBYTE] = ET_BY,
    };

    if (t < countof(ets) && (ets[t] || t == QY_VOID)) {
        *et = ets[t];
        return true;
    }

    const QTy *const y = qast_compound(t) ? qast_ty(p, t) : nullptr;

    if (!y || y->kind != QT_REC || y->ntp)
        return false;

    for (uint32_t k = 0; k < y->nfields; k++) {
        const QType ft = y->fields[k].type;
        const QType elem = qast_array(p, ft) ? qast_ty(p, ft)->elem : ft; /* a TYPE's array field */
        uint32_t f;

        if (y->fields[k].priv || qast_array(p, elem) || !qr_et(p, elem, &f) || f == ET_IMP)
            return false;
    }

    *et = ET_REC + y->td;
    return true;
}

/* Parameter type t as a QBasic parameter's, v's et and array: a list one of an array, by
   reference; false QBasic has no equal for it. */
static bool qr_param(const QProg *const p, const QType t, ExpVal *const v)
{
    const QTy *const y = qast_array(p, t) ? qast_ty(p, t) : nullptr;
    v->array = y && y->kind == QT_DYN && !y->grows;

    if (y && (!v->array || qast_array(p, y->elem)))
        return false;

    return qr_et(p, v->array ? y->elem : t, &v->et) && v->et != ET_IMP &&
           (v->array || v->et != ET_BY); /* a BYTE parameter is an array's, as QBasic's */
}

/* The module token t is in. */
static uint32_t qr_modof(const QProg *const p, const QTok *const t)
{
    return t->line < p->src.nlines ? p->src.lines[t->line].mod : 0;
}

/* String constant s's text, its escapes made their characters, in m's room: false longer than a
   QBasic literal's 255 bytes. */
static bool qr_const_text(
    const QPiece *s,
    QMem *const m,
    const char **const text,
    uint32_t *const len
)
{
    uint32_t n = 0;

    for (const QPiece *q = s; q; q = q->next)
        n += q->lit->len;

    char *const t = qast_alloc(m, n ? n : 1);
    n = 0;

    for (; s; s = s->next)
        for (uint32_t i = 0; i < s->lit->len; i++) {
            const char c = s->lit->text[i];
            const char e = c == '\\' ? s->lit->text[++i] : 0;
            t[n++] = !e ? c : e == 'n' ? '\n' : e == 't' ? '\t' : e;
        }

    *text = t;
    *len = n;
    return n <= UINT8_MAX;
}

/* Module k's public consts into x, each of a type QBasic has: an integer's, a bool's -1 or 0, a
   float's, a string's of 255 bytes at most. */
static void qr_consts(const QProg *const p, const uint32_t k, QMem *const m, Exports *const x)
{
    const QMod *const mod = &p->mods[k];
    uint32_t n = 0;

    for (QStmt *s = mod->head; s; s = qsem_nextin(mod, s))
        for (const QDecl *d = s->kind == QS_DECL && s->op == QK_CONST ? s->decls : nullptr; d;

             d = d->next)
            n++;

    x->consts = qast_alloc(m, (n ? n : 1) * sizeof *x->consts);

    for (QStmt *s = mod->head; s; s = qsem_nextin(mod, s)) {
        if (s->kind != QS_DECL || s->op != QK_CONST || s->priv)
            continue;

        for (const QDecl *d = s->decls; d; d = d->next) {
            const QVar *const v = d->var;
            ExpConst e = {.name = d->name->text, .len = d->name->len, .known = true};

            if (!v || !v->constant || !qr_et(p, v->type, &e.et) || e.et == ET_IMP || e.et >= ET_REC)
                continue;
            if (e.et == ET_SD && !qr_const_text(v->val.s, m, &e.s, &e.slen))
                continue;

            e.i = v->type == QY_BOOL ? -(int64_t)(v->val.i != 0) : v->val.i;
            e.d = qast_integer(v->type) ? (double)v->val.i : v->val.d;
            x->consts[x->nconsts++] = e;
        }
    }
}

/* Module k's public functions, methods aside, and records, as QBasic sees them. */
static Exports qr_exported(const QProg *const p, const uint32_t k, QMem *const m)
{
    static const char why[] = "a type QBasic has no equal for";

    uint32_t nf = 0, nt = 0;

    for (const QFn *f = p->fns; f; f = f->next)
        nf++;

    Exports x = {
        .procs = qast_alloc(m, (nf ? nf : 1) * sizeof *x.procs),
        .types = qast_alloc(m, (p->tys.n ? p->tys.n : 1) * sizeof *x.types),
    };

    for (const QFn *f = p->fns; f; f = f->next) {
        if (f->priv || f->in || f->op || f->test || f->lamof || f->of || qr_modof(p, f->name) != k)
            continue;

        ExpProc *const e = &x.procs[x.nprocs++];
        *e = (ExpProc){
            .name = f->name->text,
            .len = f->name->len,
            .npar = f->npar,
            .params = qast_alloc(m, (f->npar + 1) * sizeof *e->params),
            .resarg = f->byarg,
        };

        bool ok = !f->ntp && !f->ytx && !f->native && qr_et(p, f->result, &e->result.et) &&
                  e->result.et != ET_BY;

        if (!ok && f->rtype) /* a FUNCTION still, an error where called */
            e->result.et = ET_I4;

        for (uint32_t j = 0; j < f->npar; j++) { /* a generic's have no var: ok false before */
            ExpVal *const v = &e->params[j];
            ok = ok && qr_param(p, f->params[j].var->type, v);
            v->byref = f->params[j].mut || v->array;
            v->name = f->params[j].name->text;
            v->len = f->params[j].name->len;
        }

        e->params[f->npar] = e->result;
        e->why = ok ? nullptr : why;
    }

    for (uint32_t i = 0; i < p->tys.n; i++) {
        const QTy *const y = &p->tys.at[i];
        uint32_t et;

        if (y->kind != QT_REC || !y->def || y->def->priv || y->ntp || y->of ||
            qr_modof(p, y->def->tok) != k)
            continue;

        x.types[nt++] = (ExpType){
            .name = y->name->text,
            .len = y->name->len,
            .td = y->td,
            .why = qr_et(p, QY_FIRST + i, &et) ? nullptr : why,
        };
    }

    x.ntypes = nt;
    qr_consts(p, k, m, &x);
    return x;
}

/* The tree kept no more. */
static void qr_guest_forget(QrGuests *const g)
{
    if (g->prog)
        arena_reset(&g->mem.arena);

    g->prog = nullptr;
}

/* Which of p's Quickr modules QBasic calls: a QBasic program's each, else those a QBasic module
   imports (QMod.called). */
static void qr_called(Session *const S, QProg *const p)
{
    for (uint32_t k = 1; k < p->nmods; k++)
        p->mods[k].called = !p->mods[k].ex && (p->guest || modu_crossed(&S->modu, p->mods[k].modu));
}

/* Has a module QBasic calls an error, so its exports are not to be declared? */
static bool qr_called_wrong(const QProg *const p)
{
    for (uint32_t i = 0; i < p->errs.n && p->src.nlines; i++) {
        const uint32_t line = p->errs.at[i].line;
        const QLine *const ln = &p->src.lines[line < p->src.nlines ? line : p->src.nlines - 1];

        if (p->mods[ln->mod].called)
            return true;
    }

    return false;
}

/* p's modules of stage stage (mem/module.h) checked by sem, top the last: its QBasic ones'
   exports taken, its Quickr ones checked; unless one QBasic calls has an error, their records
   TYPEs, their exports (ex, by number) declared as QBasic's. Has one? */
static bool qr_staged(
    Session *const S,
    QProg *const p,
    QMem *const m,
    QSem *const sem,
    Exports *const ex,
    const uint32_t stage,
    const uint32_t top
)
{
    bool *const in = qast_alloc(m, p->nmods ? p->nmods : 1);

    for (uint32_t k = 0; k < p->nmods; k++)
        in[k] = modu_mod(&S->modu, p->mods[k].modu)->stage == stage;

    qr_qbexports(S, p, m, in);
    qsem_check(sem, in);

    if (stage == top)
        qsem_finish(sem);

    qast_sort(&p->errs);

    if (qr_called_wrong(p))
        return true;

    qlink_guest_types(&S->comp, p, m); /* those of the stages before laid already */

    for (uint32_t k = 1; k < p->nmods; k++) {
        if (!in[k] || !p->mods[k].called)
            continue;

        ex[k] = qr_exported(p, k, m);
        modu_mod(&S->modu, p->mods[k].modu)->clash =
            expo_declare(&S->comp, p->mods[k].modu, &ex[k]);
    }

    return false;
}

static const Exports *qr_stages(Session *const S, QProg *const p, QMem *const m, const bool lenient)
{
    const uint32_t top = repl_stages(S);
    QSem *const sem = qsem_start(p, m);
    Exports *const ex = qast_alloc(m, (p->nmods ? p->nmods : 1) * sizeof *ex);
    qr_called(S, p);
    bool wrong = false; /* the later stages' QBasic modules not declarable */

    for (uint32_t stage = 0; stage <= top && (lenient || !wrong); stage++) {
        repl_declare_stage(S, stage);
        wrong = wrong || qr_staged(S, p, m, sem, ex, stage, top);
    }

    repl_declared(S);
    return ex;
}

/* The QBasic program's Quickr modules, each its own; a QBasic one each imports, its exports later
   (qr_staged): G's program, ordered, its check started. */
static void qr_guest_start(Session *const S, QrGuests *const G)
{
    qr_guest_forget(G);
    G->mem = (QMem){.fault = &S->fault};

    arena_init(&G->mem.arena, QR_CHUNK);
    QMem *const m = &G->mem;
    QProg *const p = G->prog = qast_alloc(m, sizeof *p);
    *p = (QProg){.errs = {.mem = m}, .copies = {.mem = m}, .guest = true, .keep = true};

    qr_newmod(p, m, S->repl.file); /* 0: the QBasic program's, no lines */

    for (uint32_t mm = 0; mm < modu_count(&S->modu); mm++) {
        const ModuMod *const mod = modu_mod(&S->modu, mm);

        if (mod->dialect != &dialect_quickr)
            continue;

        const uint32_t k = p->nmods;
        qr_gather(S, p, m, mod->file, modu_module_path(&S->modu, mm));
        p->mods[k].modu = mm;
        p->mods[k].start = mod->top + prog_line(&S->prog, mod->top)->len;
    }

    const QrLoad L = {S, p, m, .guest = true};

    for (uint32_t k = 0; k < p->nmods; k++)
        qr_imports(&L, k);

    qr_order(p, m);
    qr_called(S, p);
    G->ex = qast_alloc(m, p->nmods * sizeof *G->ex);
    G->sem = qsem_start(p, m);
}

/* p's errors said, unless lenient: raised, the editor's first kept. */
static void qr_guest_failed(Session *const S, QrState *const q, const QProg *const p, bool lenient)
{
    if (lenient)
        return;

    for (uint32_t i = 0; i < p->errs.n; i++)
        qr_say(S, p, &p->errs.at[i]);

    if (S->repl.batch)
        cons_exit(&S->cons, 1);

    DialErr e = {.msg = q->keep.said, .cap = sizeof q->keep.said}; /* the editor's */
    qr_said(p, &p->errs.at[0], &e);
    fault_raise(&S->fault, (Failure){.code = E_SN, .line = e.rec, .at = FAULT_KEEP});
}

void qr_guest_declare(
    Session *const S,
    const uint32_t stage,
    const uint32_t top,
    const bool lenient
)
{
    QrState *const q = qr_state(S);
    QrGuests *const G = &q->guests;

    if (!stage) {
        q->keep.said[0] = 0;
        qr_guest_start(S, G);
    }

    if (!G->prog) /* a stage's errors, lenient */
        return;

    if (qr_staged(S, G->prog, &G->mem, G->sem, G->ex, stage, top)) {
        qr_guest_failed(S, q, G->prog, lenient);
        qr_guest_forget(G);
        return;
    }

    if (stage < top)
        return;

    qr_instances(S, G->prog, &G->mem);
    S->comp.guest = &G->link;
}

/* The QBasic link's: each Quickr procedure and variable made (qlink_guest_made). */
static void qr_guest_made(LinkGuest *const g, Comp *const C)
{
    QrGuests *const G = container_of(g, QrGuests, link);
    G->of = qlink_guest_made(C, G->prog, &G->mem, G->ex);
}

static void qr_guest_lines(LinkGuest *const g, Comp *const C)
{
    QrGuests *const G = container_of(g, QrGuests, link);
    qlink_guest_lines(C, G->prog, &G->mem, G->of, *G->emit);
}

static void qr_guest_resolve(LinkGuest *const g, Comp *const C)
{
    const QrGuests *const G = container_of(g, QrGuests, link);
    qlink_guest_resolve(C, G->prog);
}

/* The QBasic link's hooks, emit the session's dbug.emit; no tree. */
static void qr_guest_init(QrGuests *const g, const bool *const emit)
{
    *g = (QrGuests){
        .link = {.made = qr_guest_made, .lines = qr_guest_lines, .resolve = qr_guest_resolve},
        .emit = emit,
    };
}

QrState *qr_state(Session *const S)
{
    QrState *const q = dial_state(&S->dial, &dialect_quickr);

    if (!q)
        fault_error(&S->fault, E_OM);
    if (!q->guests.emit) /* new */
        qr_guest_init(&q->guests, &S->dbug.emit);

    return q;
}

uint32_t qr_link(Session *const S, DialErr *const e)
{
    QrKeep *const k = &qr_state(S)->keep;
    qr_forget(k);
    k->mem = (QMem){.fault = &S->fault};

    arena_init(&k->mem.arena, QR_CHUNK);
    QMem *const m = &k->mem;
    QProg *const p = qast_alloc(m, sizeof *p);
    *p = (QProg){.errs = {.mem = m}, .copies = {.mem = m}, .keep = true};

    qr_enter(&(QrLoad){S, p, m, .stored = true});

    const Exports *const ex = qr_stages(S, p, m, false);

    if (p->errs.n) {
        qr_said(p, &p->errs.at[0], e);
        qr_forget(k);
        return p->errs.n;
    }

    qr_end(S, p, m);
    qr_instances(S, p, m);
    qlink_program(&S->comp, p, m, ex, S->dbug.emit);
    k->prog = p;
    return 0;
}

/* The checked program q keeps: the link's, else a QBasic program's modules'; null none. */
static QProg *qr_kept(const QrState *const q)
{
    return !q ? nullptr : q->keep.prog ? q->keep.prog : q->guests.prog;
}

uint32_t qr_spell(const Session *const S, const uint32_t id, char *const d)
{
    const QProg *const p = qr_kept(dial_kept(&S->dial, &dialect_quickr));
    const QVar *v = p ? p->vars : nullptr;

    while (v && (v->id != id || !v->name))
        v = v->next;

    if (!v)
        return 0;

    const uint32_t n = v->name->len < DIAL_SPELL ? v->name->len : DIAL_SPELL;
    memcpy(d, v->name->text, n);
    return n;
}

/* The names in sight at program line rec's statement; null the modules' alone. */
static const QScopeAt *qr_sight(const QProg *const p, const uint32_t rec)
{
    for (const QStmt *s = p->first; rec && rec != DLINE && s; s = s->next)
        if (p->src.lines[s->line].at == rec && s->at)
            return s->at;

    return nullptr;
}

/* Program line rec's module of p's; 0 none. */
static uint16_t qr_mod_at(const QProg *const p, const uint32_t rec)
{
    for (uint32_t i = 0; i < p->src.nlines; i++)
        if (p->src.lines[i].at == rec)
            return p->src.lines[i].mod;

    return 0;
}

/* The probe's line, cons.inbuf's text: lexed as p's last line, in module mod; its first token. */
static const QTok *qr_probe_line(
    Session *const S,
    QProg *const p,
    QMem *const m,
    const uint16_t mod
)
{
    const char *const text = (const char *)S->cons.inbuf;
    uint32_t cap = p->src.nlines;
    qr_addline(
        p,
        m,
        &cap,
        (QLine){
            .at = DLINE,
            .text = text,
            .len = (uint32_t)strlen(text),
            .mod = mod,
        }

    );
    qlex_source(&p->src, p->src.nlines - 1, m, &p->errs);
    return p->src.toks;
}

/* Is p's last error a value written alone? */
static bool qr_alone(const QProg *const p)
{
    static const char alone[] = "a value alone";
    const QErr *const e = p->errs.n ? &p->errs.at[p->errs.n - 1] : nullptr;
    return e && e->len >= sizeof alone - 1 && !memcmp(e->text, alone, sizeof alone - 1);
}

/* cons.inbuf's text as print text. */
static void qr_printed(Session *const S)
{
    static const char pre[] = "print ";
    const size_t n = strlen((const char *)S->cons.inbuf);
    const size_t room = INMAX - (sizeof pre - 1) - 1;
    const size_t k = n < room ? n : room;
    uint8_t *const b = cons_inroom(&S->cons, (uint32_t)(sizeof pre + k));
    memmove(b + sizeof pre - 1, b, k);
    memcpy(b, pre, sizeof pre - 1);
    b[sizeof pre - 1 + k] = 0;
}

void qr_direct_code(Session *const S, const uint32_t rec)
{
    QrState *const q = qr_state(S);
    QrKeep *const k = &q->keep;
    QProg *const p = qr_kept(q);
    k->said[0] = 0;

    if (!p) {
        strcpy(k->said, "the program is not linked: run it first");
        fault_error(&S->fault, E_SN);
    }

    QMem m = {.fault = &S->fault}; /* the probe's own: dropped after, p as it was */
    arena_init(&m.arena, QR_CHUNK);
    const QSrc src = p->src;
    const QTys tys = p->tys;
    const QErrs errs = p->errs;
    QVar *last = p->vars;

    while (last && last->next)
        last = last->next;

    p->errs.mem = &m;
    const QScopeAt *const at = qr_sight(p, rec);
    const uint16_t mod = at ? qr_mod_at(p, rec) : 0;
    QStmt *s = qpar_line(p, &m, qr_probe_line(S, p, &m, mod));

    if (!s && qr_alone(p)) {
        p->errs.n = errs.n;
        p->src = src;
        qr_printed(S);
        s = qpar_line(p, &m, qr_probe_line(S, p, &m, mod));
    }

    const bool ok =
        s && qsem_probe(p, &m, at, s) && p->tys.n == tys.n &&
        qlink_direct(&S->comp, p, &m, last ? last->next : p->vars, s, addr_of(S->cons.inbuf));

    if (!ok && p->errs.n > errs.n) {
        const QErr *const e = &p->errs.at[errs.n];
        const uint32_t n = e->len < sizeof k->said - 1 ? e->len : sizeof k->said - 1;
        memcpy(k->said, e->text, n);
        k->said[n] = 0;
    } else if (!ok) {
        strcpy(k->said, "this can't run here");
    }

    if (last)
        last->next = nullptr;
    else
        p->vars = nullptr;

    p->src = src;
    p->tys = tys;
    p->errs = errs;
    arena_reset(&m.arena);

    if (!ok)
        fault_error(&S->fault, E_SN);
}

void qr_lines(Session *const S, QProg *const p, QMem *const m, const uint32_t file)
{
    qr_gather(S, p, m, file, "");
    qast_sort(&p->errs);
}

bool qr_syntax(Session *const S, const uint32_t rec, DialErr *const e)
{
    QMem m = {.fault = &S->fault};

    arena_init(&m.arena, QR_CHUNK);
    QProg p = qr_loose(&m);

    qr_lines(S, &p, &m, prog_line(&S->prog, rec)->file);
    bool found = false;

    for (uint32_t i = 0; !found && i < p.errs.n; i++)
        if (p.errs.at[i].line < p.src.nlines && p.src.lines[p.errs.at[i].line].at == rec) {
            qr_said(&p, &p.errs.at[i], e);
            found = true;
        }

    arena_reset(&m.arena);
    return found;
}

void qr_flag(Session *const S)
{
    QMem m = {.fault = &S->fault};

    arena_init(&m.arena, QR_CHUNK);

    for (uint32_t f = 0; f < S->modu.nfiles; f++) {
        if (dial_file(&S->prog, f) != &dialect_quickr)
            continue;

        QProg p = qr_loose(&m);

        qr_lines(S, &p, &m, f);

        for (uint32_t i = 0; i < p.src.nlines; i++)
            prog_line(&S->prog, p.src.lines[i].at)->flags &= ~LF_ASIS;

        for (uint32_t i = 0; i < p.errs.n; i++)
            if (p.errs.at[i].line < p.src.nlines)
                prog_line(&S->prog, p.src.lines[p.errs.at[i].line].at)->flags |= LF_ASIS;
    }

    arena_reset(&m.arena);
}

bool qr_namec(const char c) { return is_letter((uint8_t)c) || is_digit((uint8_t)c) || c == '_'; }

/* Past the word w at text's *at and the blanks after it: true, *at past them. */
static bool qr_word(const char *const text, const uint32_t len, uint32_t *const at, const char *w)
{
    const uint32_t n = (uint32_t)strlen(w);

    if (*at + n > len || memcmp(text + *at, w, n) || (*at + n < len && qr_namec(text[*at + n])))
        return false;

    *at += n;

    while (*at < len && text[*at] == ' ')
        (*at)++;

    return true;
}

bool qr_fn_at(const char *const text, const uint32_t len, uint32_t *const at, uint32_t *const n)
{
    uint32_t i = 0;

    while (i < len && text[i] == ' ')
        i++;

    qr_word(text, len, &i, "public");
    qr_word(text, len, &i, "native");

    if (!qr_word(text, len, &i, "function"))
        return false;

    uint32_t end = i;

    if (qr_word(text, len, &end, "operator")) /* its operator too: function operator + */
        while (end < len && text[end] != ' ' && text[end] != '(')
            end++;
    else
        while (end < len && qr_namec(text[end]))
            end++;

    if (end == i || is_digit((uint8_t)text[i]))
        return false;

    *at = i;
    *n = end - i;
    return true;
}

uint32_t qr_lexed(
    QMem *const m,
    const char *const text,
    const uint32_t len,
    const QTok **const toks
)
{
    QErrs errs = {.mem = m};

    QLine ln = {.text = text, .len = len};

    QSrc src = {.lines = &ln, .nlines = 1};

    qlex_source(&src, 0, m, &errs);
    uint32_t n = src.ntoks;

    while (n && (src.toks[n - 1].kind == QK_NL || src.toks[n - 1].kind == QK_EOF))
        n--;

    *toks = src.toks;
    return n;
}

uint32_t qr_tokens(
    Fault *const f,
    const char *const text,
    const uint32_t len,
    QrTokFn *const fn,
    void *const ctx
)
{
    QMem m = {.fault = f};

    arena_init(&m.arena, QR_LINE_CHUNK);
    const QTok *toks;
    const uint32_t n = qr_lexed(&m, text, len, &toks);
    uint32_t end = 0;

    for (uint32_t i = 0; i < n; i++) {
        if (toks[i].kind == QK_NL)
            continue;

        fn(ctx, &toks[i]);
        end = toks[i].col + toks[i].len;
    }

    arena_reset(&m.arena);

    while (end < len && text[end] != '\'')
        end++;

    return end;
}

/* ---- colours */

enum : uint8_t { QR_HS_AS = 1, QR_HS_DEF = 2, QR_HS_NAME = 4 };

/* A line being coloured, its parts to fn with ctx: the token before (QK_EOF none); what it makes
   of the next, as ui/highlight.c's QBasic state: after AS or TYPE a type, after FUNCTION or FN a
   name being declared, then its parameters in the ( ) after it, depth deep; in a fn's [ ] its
   type parameters. */
typedef struct {
    DialColourFn *fn;
    void *ctx;
    QKind prev;
    uint8_t state, depth;
    bool generic;
} QrHl;

/* A token's colour; hl->state the next one's. */
static DialColour qr_hl_slot(QrHl *const hl, const QTok *const t)
{
    const uint8_t state = hl->state;
    hl->state = 0;

    switch (t->kind) {
    case QK_NAME:
        if (state & QR_HS_AS || hl->generic)
            return DHL_TYPE;
        if (state & QR_HS_DEF) {
            hl->state = QR_HS_NAME;
            return DHL_FN;
        }

        return hl->depth ? DHL_PARAM : DHL_NAME;

    case QK_INT:
    case QK_REAL:
        return DHL_NUM;

    case QK_STR:
    case QK_FSTR:
    case QK_FPART:
    case QK_FSPEC:
    case QK_FEND:
        return DHL_STR;

    case QK_AS:
    case QK_TYPE:
    case QK_ENUM:
        hl->state = hl->prev == QK_END ? 0 : QR_HS_AS;
        return DHL_KW;

    case QK_FUNCTION:
    case QK_FN: /* a method's name after it; a lambda's ( */
        hl->state = hl->prev == QK_END ? 0 : QR_HS_DEF;
        return DHL_KW;

    case QK_LBRACK: /* a generic's parameters: function at[T] */
        hl->generic = state & QR_HS_NAME;
        hl->state = state;
        return DHL_PLAIN;

    case QK_RBRACK:
        hl->state = hl->generic ? QR_HS_NAME : 0;
        hl->generic = false;
        return DHL_PLAIN;

    case QK_LPAREN:
        if (state & QR_HS_NAME)
            hl->depth = 1;
        else if (hl->depth)
            hl->depth++;

        return DHL_PLAIN;

    case QK_RPAREN:
        if (hl->depth)
            hl->depth--;

        return DHL_PLAIN;

    case QK_COMMA:
        hl->state = state & QR_HS_AS ? 0 : state;
        return DHL_PLAIN;

    default:
        break;
    }

    if (t->kind >= QK_AND && t->kind <= QK_FUNCTION)
        return DHL_KW;
    if (t->kind >= QK_QUEST && t->kind < QK_BAD)
        return DHL_OP;

    return DHL_PLAIN;
}

/* qr_tokens's: token t's colour. */
static void qr_hl_token(void *const ctx, const QTok *const t)
{
    QrHl *const hl = ctx;
    hl->fn(hl->ctx, t->col, qr_hl_slot(hl, t));
    hl->prev = t->kind;
}

/* The line text, len bytes: its tokens' colours by Quickr's lexer, then its comment's. */
static void qr_colour(
    Fault *const f,
    const char *const text,
    const uint32_t len,
    DialColourFn *const fn,
    void *const ctx
)
{
    QrHl hl = {.fn = fn, .ctx = ctx, .prev = QK_EOF};

    const uint32_t comment = qr_tokens(f, text, len, qr_hl_token, &hl);

    if (comment < len)
        fn(ctx, comment, DHL_COMMENT);
}

/* ---- the editor's other hooks */

/* A line's names, text as typed: each to fn with ctx, strings and a comment skipped. */
static void qr_names(const uint8_t *p, DialNameFn *const fn, void *const ctx)
{
    while (*p && *p != '\'') {
        if (*p == '"') {
            for (p++; *p && *p != '"'; p++)
                ;

            p += !!*p;
            continue;
        }

        if (!is_word_char(*p) || (*p >= '0' && *p <= '9')) {
            p++;
            continue;
        }

        const uint8_t *const w = p;

        while (is_word_char(*p))
            p++;

        fn(ctx, w, (uint32_t)(p - w));
    }
}

/* Is the line t, n bytes, an import or a from? */
static bool qr_imports_line(const uint8_t *const t, const uint32_t n)
{
    uint32_t i = 0;

    while (i < n && t[i] == ' ')
        i++;

    return (n - i > 7 && !memcmp(t + i, "import ", 7)) || (n - i > 5 && !memcmp(t + i, "from ", 5));
}

/* expr's value printed: print(expr), to d, cap bytes with its 0; false too long. */
static bool qr_print(const char *const expr, char *const d, const uint32_t cap)
{
    const size_t n = strlen(expr);

    if (n + 8 > cap)
        return false;

    memcpy(d, "print(", 6);
    memcpy(d + 6, expr, n);
    memcpy(d + 6 + n, ")", 2);
    return true;
}

/* Is a program qr_link made kept? */
static bool qr_linked(const Session *const S)
{
    return qr_kept(dial_kept(&S->dial, &dialect_quickr)) != nullptr;
}

/* The last probe's error, Syntax error's: its message; null none. */
static const char *qr_errtext(const Session *const S, const uint8_t code)
{
    const QrState *const q = dial_kept(&S->dial, &dialect_quickr);
    return q && code == E_SN && q->keep.said[0] ? q->keep.said : nullptr;
}

/* The kept programs forgotten, the link's and a QBasic program's modules'. */
static void qr_forget_kept(Session *const S)
{
    QrState *const q = dial_kept(&S->dial, &dialect_quickr);

    if (!q)
        return;
    if (q->keep.prog) /* none: none, or qr_link's, being made */
        qr_forget(&q->keep);

    qr_guest_forget(&q->guests);

    if (S->comp.guest == &q->guests.link)
        S->comp.guest = nullptr;
}

const Dialect dialect_quickr = {
    .name = "Quickr",
    .button = "&Quickr",
    .open = qr_open,
    .run = qr_run,
    .report = qr_report,
    .message = qr_message,
    .text = true,
    .mark = qr_dialect,
    .help = "QUICKR",
    .size = sizeof(QrState),
    .marker = qr_marker,
    .blank = qr_blank,
    .is_file = qr_is_file,
    .read = qr_read,
    .link = qr_link,
    .go = qr_go,
    .linked = qr_linked,
    .redeclare = qr_open,
    .print = qr_print,
    .direct = qr_direct_code,
    .syntax = qr_syntax,
    .flag = qr_flag,
    .colour = qr_colour,
    .blocks = qr_blocks,
    .names = qr_names,
    .fn_at = qr_fn_at,
    .imports = qr_imports_line,
    .suggest = qr_suggest,
    .spell = qr_spell,
    .errtext = qr_errtext,
    .forget = qr_forget_kept,
    .guest_read = qr_guest_read,
    .guest_find = qr_guest_find,
    .guest_declare = qr_guest_declare,
};
