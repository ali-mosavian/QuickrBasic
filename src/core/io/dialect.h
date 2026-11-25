/* The source dialects: what the core asks of a program's language. How it works: dialect.c. */
#ifndef CORE_IO_DIALECT_H
#define CORE_IO_DIALECT_H
#include "base.h"
#include "fault.h"
#include "mem/program.h"

enum { DIAL_MAX = 4 };    /* the dialects at most */
enum { DIAL_SPELL = 40 }; /* a name a text dialect spells: its bytes at most, as QBasic's */
enum { DIAL_HELP = 16 };  /* a help key's bytes, its space after it, at most */

/* An error at a stored line: its record, its column; its message to msg, cap bytes with its 0. */
typedef struct {
    uint32_t rec, col;
    char *msg;
    uint32_t cap;
} DialErr;

/* What a line's text is from a column on, as the editor colours it. */
typedef enum : uint8_t {
    DHL_PLAIN,
    DHL_KW,
    DHL_NAME,
    DHL_TYPE,
    DHL_FN, /* a function's name being declared */
    DHL_PARAM,
    DHL_NUM,
    DHL_STR,
    DHL_OP,
    DHL_COMMENT,
} DialColour;
typedef void DialColourFn(void *ctx, uint32_t col, DialColour c);
/* A completion's kind: a keyword, a fn (rec its line), another name. */
typedef enum : uint8_t { DSG_KW, DSG_FN, DSG_NAME } DialSuggKind;
typedef void DialSuggFn(void *ctx, const char *text, uint32_t len, DialSuggKind kind, uint32_t rec);
typedef void DialNameFn(void *ctx, const uint8_t *name, uint32_t len);

/* A dialect's operations. QBasic's are the core's own; a null hook is one QBasic has none of. */
typedef struct Dialect Dialect;
struct Dialect {
    const char *name;   /* the status bar's */
    const char *button; /* File New's: its name, & before its key */
    bool suffix;        /* a variable shows its type's suffix (% & ! # $) */
    /* The store's program, repl.file the main's: each module its imports name read in. */
    void (*open)(Session *S);
    /* Run repl.file, then exit (its --check, --test, --copies as repl.checking ...). */
    void (*run)(Session *S);
    /* Runtime error code at program line vm.line, the stacks reset. */
    void (*report)(Session *S, uint8_t code);
    /* Runtime error code's message. */
    const char *(*message)(const Session *S, uint8_t code);

    /* A text dialect: its lines stored as typed (LF_TEXT), each hook below its own. */
    bool text;
    const char *mark; /* a file's first line, which makes it this dialect's */
    const char *help; /* its words' help topics: "KEY word"; KEY alone its own */
    size_t size;      /* its state's bytes in a session (dial_state) */
    /* Is the line text, len bytes, mark: case aside, spaces after it? */
    bool (*marker)(const char *text, uint32_t len);
    /* Is the file at path this dialect's: its first line mark? */
    bool (*is_file)(const char *path);
    /* The file at path's lines after the store's, of file file. PFIL_OK, else the DOS error. */
    uint16_t (*read)(Session *S, const char *path, uint16_t file);
    /* open's, then checked and linked, ready to run: its errors' count, the first to *e. */
    uint32_t (*link)(Session *S, DialErr *e);
    /* The program link made run from its first line, under repl_call. Never returns. */
    void (*go)(Session *S);
    /* Is a program link made kept? */
    bool (*linked)(const Session *S);
    /* expr's value printed, as a statement, to d, cap bytes with its 0; false: too long. */
    bool (*print)(const char *expr, char *d, uint32_t cap);
    /* cons.inbuf's line as the direct line's code, its names as at program line rec's statement
       (0 or DLINE: the modules', after their last line). An error: Syntax error raised. */
    void (*direct)(Session *S, uint32_t rec);
    /* Record rec's file's lines checked alone: rec's first error to *e; false none. */
    bool (*syntax)(Session *S, uint32_t rec, DialErr *e);
    /* Each stored line LF_ASIS when syntax finds an error in it, else not. */
    void (*flag)(Session *S);
    /* The line text, len bytes, each part's colour to fn with ctx, by column. Out of memory
       raises through f. */
    void (*colour)(Fault *f, const char *text, uint32_t len, DialColourFn *fn, void *ctx);
    /* The 0-ended line text's names, strings and a comment skipped, each to fn with ctx. */
    void (*names)(const uint8_t *text, DialNameFn *fn, void *ctx);
    /* Is the line text, len bytes, a function's first: its name's column and length to *at, *n. */
    bool (*fn_at)(const char *text, uint32_t len, uint32_t *at, uint32_t *n);
    /* Does the line text, len bytes, import modules? */
    bool (*imports)(const uint8_t *text, uint32_t len);
    /* What fits at a word whose line before it is text, n bytes, the line at rec, each to fn
       with ctx. */
    void (*suggest)(
        Session *S,
        uint32_t rec,
        const char *text,
        uint32_t n,
        DialSuggFn *fn,
        void *ctx
    );
    /* Variable id's name as typed to d, DIAL_SPELL bytes at most: its length, 0 unknown. */
    uint32_t (*spell)(const Session *S, uint32_t id, char *d);
    /* Error code's message of its own, null none. */
    const char *(*errtext)(const Session *S, uint8_t code);
    /* What it keeps of a program dropped: the program edited or new. */
    void (*forget)(Session *S);
    /* A QBasic program's module m, this dialect's: its file read into the store at its end. */
    void (*guest_read)(Session *S, uint32_t m);
    /* The same, its lines in the store: each module its imports name the program's, imported
       by m, as guest_read finds them. */
    void (*guest_find)(Session *S, uint32_t m);
    /* Each of the QBasic program's modules of this dialect checked, their exports declared as
       QBasic's, the link's hooks set (comp/link.h). */
    void (*guest_declare)(Session *S);
};

/* QBasic's: the core's own (io/repl.c). */
extern const Dialect dialect_qbasic;

/* Each dialect's state, made on first use: by its place among them. */
typedef struct {
    void *of[DIAL_MAX];
} Dial;

/* The dialects: QBasic's first. */
uint32_t dial_count(void);
const Dialect *dial_at(uint32_t i);
/* The dialect of the file at path: a text dialect's, else QBasic's. */
const Dialect *dial_of_file(const char *path);
/* The stored program's dialect: its first record's (prog_text). */
const Dialect *dial_program(const Prog *pg);
/* Record rec's dialect: its file's, LF_TEXT's; else QBasic's. */
const Dialect *dial_line(const Prog *pg, uint32_t rec);
/* File file's dialect: its first record's; QBasic's when it has none. */
const Dialect *dial_file(const Prog *pg, uint32_t file);
/* d's place among the dialects, under DIAL_MAX. */
uint32_t dial_index(const Dialect *d);
/* d's state, made zeroed on first use; null none left. */
void *dial_state(Dial *dl, const Dialect *d);
/* d's state, null not made. */
void *dial_kept(const Dial *dl, const Dialect *d);
/* Each dialect's forget. */
void dial_forget(Session *S);
/* Error code's message of a dialect's own, null none. */
const char *dial_errtext(const Session *S, uint8_t code);
#endif
