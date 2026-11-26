/* Completion in a program of both dialects, and after a module's name in either. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "plat/pmem.h"
#include "session.h"
#include "comp/suggest.h"
#include "io/qbxfile.h"
#include "ui/linemap.h"
#include "ui/uimain.h"
#include "edfix.h"
#include "unit.h"

static const char *const files[] = {
    "main.bas",
    "geo.bas",
    "shapes.bas",
    nullptr,
};

#define DIALECT "' OPTION DIALECT QUICKR\n"

static const char geo[] = DIALECT "type Spot\n"
                                  "    x as int\n"
                                  "    y as int\n"
                                  "    fn moved(self) as Spot\n"
                                  "        return Spot(self.x + 1, self.y)\n"
                                  "    end fn\n"
                                  "end type\n"
                                  "enum Colour\n"
                                  "    red\n"
                                  "    green\n"
                                  "end enum\n"
                                  "const limit = 10\n"
                                  "var count as int\n"
                                  "function area(w as int, h as int) as int\n"
                                  "    let total = w * h\n"
                                  "    return total\n"
                                  "end function\n"
                                  "function names() as string[]\n"
                                  "    var n as string[]\n"
                                  "    return n\n"
                                  "end function\n"
                                  "type Bag\n"
                                  "    items as int[]\n"
                                  "end type\n"
                                  "private function helper() as int\n"
                                  "    return 1\n"
                                  "end function\n";

static const char shapes[] = "TYPE Rect\n"
                             "    W AS LONG\n"
                             "    H AS LONG\n"
                             "END TYPE\n"
                             "CONST MAXW = 5\n"
                             "FUNCTION Area& (r AS Rect)\n"
                             "    Area& = r.W * r.H\n"
                             "END FUNCTION\n"
                             "SUB Show (a AS LONG)\n"
                             "    PRINT a\n"
                             "END SUB\n"
                             "SUB Fill (a() AS BYTE)\n"
                             "END SUB\n"
                             "PRIVATE SUB Hidden\n"
                             "END SUB\n";

/* main.bas, its text main, importing geo.bas, Quickr's, and shapes.bas, QBasic's. */
static void open_with(Session *const S, const char *const main)
{
    fx_open_program(S, "");
    fx_write_file("main.bas", main);
    fx_write_file("geo.bas", geo);
    fx_write_file("shapes.bas", shapes);
    uimn_open(S, fx_path_of("main.bas"));
}

/* What completion offers for text typed on the line spelled at: has it want. */
static bool offers(Session *const S, const char *const at, const char *const text, const char *want)
{
    Suggs out = {};

    const uint16_t n = (uint16_t)strlen(text);
    const uint32_t line = fx_line_of(S, at);
    CHECK(line != ~0u);
    bool found = false;

    if (line != ~0u && sugg_find(S, (const uint8_t *)text, n, n, lmap_off(S, line), &out))
        for (uint16_t i = 0; i < out.n; i++)
            found |= !strcmp((const char *)out.list[i].text, want);

    pmem_free(out.list);
    return found;
}

static const char qb_main[] = "IMPORT geo\n"
                              "IMPORT shapes\n"
                              "DIM s AS geo.Spot\n"
                              "DIM r AS shapes.Rect\n"
                              "total = 1\n"
                              "PRINT total\n";

/* In a QBasic program a Quickr module's line was completed as QBasic's: PRINT, not print. */
TEST(mixcomplete_qb_main_completes_quickr_lines_as_quickr)
{
    open_with(S, qb_main);
    CHECK(offers(S, "    return total", "    pri", "print"));
    CHECK(!offers(S, "    return total", "    pri", "PRINT"));
    fx_done(S, files);
}

static const char qr_main[] = DIALECT "import geo\n"
                                      "import shapes\n"
                                      "var s as geo.Spot\n"
                                      "var r as shapes.Rect\n"
                                      "var xs as int[]\n"
                                      "var d as dict[string, int]\n"
                                      "let w = \"hi\"\n"
                                      "let total = 1\n"
                                      "print total\n";

/* In a Quickr program a QBasic module's line was completed as Quickr's: print, not PRINT. */
TEST(mixcomplete_qr_main_completes_qbasic_lines_as_qbasic)
{
    open_with(S, qr_main);
    CHECK(offers(S, "    print a", "    PRI", "PRINT"));
    CHECK(!offers(S, "    print a", "    PRI", "print"));
    fx_done(S, files);
}

/* After a QBasic module's name, a QBasic program offered every name of the program, the module's
   PRIVATE ones and the main's own too. */
TEST(mixcomplete_qb_main_offers_a_qbasic_modules_exports)
{
    open_with(S, qb_main);
    CHECK(offers(S, "print total", "a = shapes.", "Area"));
    CHECK(offers(S, "print total", "shapes.", "Show"));
    CHECK(offers(S, "print total", "a = shapes.", "MAXW"));
    CHECK(offers(S, "print total", "dim q as shapes.", "Rect"));
    CHECK(!offers(S, "print total", "shapes.", "Hidden"));
    CHECK(!offers(S, "print total", "a = shapes.", "total"));
    CHECK(offers(S, "print total", "a = shapes.Ar", "Area"));
    CHECK(fx_typed_over(S, fx_line_of(S, "private sub Hidden"), "sub Shown"));
    CHECK(offers(S, "print total", "shapes.", "Shown"));
    fx_done(S, files);
}

/* After a Quickr module's name, a QBasic program offered its own names, not the module's. */
TEST(mixcomplete_qb_main_offers_a_quickr_modules_exports)
{
    open_with(S, qb_main);
    CHECK(offers(S, "print total", "a = geo.", "area"));
    CHECK(offers(S, "print total", "dim q as geo.", "Spot"));
    CHECK(!offers(S, "print total", "a = geo.", "helper"));
    CHECK(!offers(S, "print total", "a = geo.", "limit")); /* QBasic sees no const */
    CHECK(!offers(S, "print total", "a = geo.", "Area"));
    CHECK(!offers(S, "print total", "a = geo.", "total"));
    fx_done(S, files);
}

/* After a QBasic module's name a Quickr program offered nothing, nor after a value of one of its
   TYPEs; after a module's enum, nothing. */
TEST(mixcomplete_qr_main_offers_a_qbasic_modules_exports)
{
    open_with(S, qr_main);
    CHECK(offers(S, "print total", "let a = shapes.", "Area"));
    CHECK(offers(S, "print total", "shapes.", "Show"));
    CHECK(offers(S, "print total", "var q as shapes.", "Rect"));
    CHECK(!offers(S, "print total", "shapes.", "Hidden"));
    CHECK(!offers(S, "print total", "let a = shapes.", "MAXW")); /* Quickr sees no CONST */
    CHECK(offers(S, "print total", "let a = r.", "W"));
    CHECK(offers(S, "print total", "let a = r.", "H"));
    CHECK(fx_typed_over(S, fx_line_of(S, "private sub Hidden"), "sub Shown"));
    CHECK(fx_typed_over(S, fx_line_of(S, "print total"), "print total + 1"));
    CHECK(offers(S, "print total + 1", "shapes.", "Shown"));
    CHECK(offers(S, "print total + 1", "shapes.", "Area"));
    fx_done(S, files);
}

/* qxt.qbx as tools/qbx.py pack lays it out: a slice for no machine, empty, and the module's own
   interface iface, both at 64, the interface followed by 1Ah. */
static void write_qbx(const char *const iface)
{
    uint8_t f[256] = {};

    const uint32_t n = (uint32_t)strlen(iface);
    memcpy(f, "QBX\x1a", 4);
    store16(f + 4, QBXF_FORMAT);
    store16(f + 6, 1);
    store16(f + 10, 1);
    store16(f + 12, 1);
    strcpy((char *)f + 16, "nosuch"); /* the slice: tag, at, bytes */
    store32(f + 32, 64);
    store32(f + 56, 64); /* the part "": its name all 0, at, bytes */
    store32(f + 60, n);
    memcpy(f + 64, iface, n);
    f[64 + n] = 0x1a;

    FILE *const out = fopen(fx_path_of("qxt.qbx"), "wb");
    fwrite(f, 1, 64 + n + 1, out);
    fclose(out);
}

/* After a native module's name the IDE's Quickr program offered nothing: its .qbx was read as
   text, its DECLAREs lost. */
TEST(mixcomplete_qr_main_offers_a_native_modules_exports)
{
    static const char *const names[] = {"main.bas", "qxt.qbx", nullptr};

    fx_open_program(S, "");
    fx_write_file("main.bas", DIALECT "import qxt\nlet total = 1\nprint total\n");
    write_qbx("declare function add% cdecl (byval a%, byval b%)\n");
    uimn_open(S, fx_path_of("main.bas"));
    CHECK(offers(S, "print total", "let a = qxt.", "add"));
    fx_done(S, names);
}

/* After a QBasic module's name a Quickr program offered a SUB it can't call: a BYTE array
   parameter. */
TEST(mixcomplete_qr_main_leaves_out_what_it_cant_use)
{
    open_with(S, qr_main);
    CHECK(offers(S, "print total", "shapes.", "Show"));
    CHECK(!offers(S, "print total", "shapes.", "Fill"));
    fx_done(S, files);
}

/* After a module's enum, m.E., a Quickr program offered nothing: its values were not found. */
TEST(mixcomplete_qr_main_offers_a_modules_enum_values)
{
    open_with(S, qr_main);
    CHECK(offers(S, "print total", "let a = geo.", "Colour"));
    CHECK(offers(S, "print total", "let a = geo.Colour.", "red"));
    CHECK(offers(S, "print total", "let a = geo.Colour.", "green"));
    CHECK(!offers(S, "print total", "let a = geo.", "helper"));
    fx_done(S, files);
}

/* A QBasic program offered its modules' own names where they are not seen: a module's SUB in the
   main, a Quickr module's lines read as tokens. FROM's names are the main's. */
TEST(mixcomplete_qb_main_offers_its_modules_own_names)
{
    open_with(S, "IMPORT geo\nFROM shapes IMPORT Show\ntotal = 1\nPRINT total\n");
    CHECK(offers(S, "print total", "a = ", "total"));
    CHECK(offers(S, "print total", "", "Show"));
    CHECK(!offers(S, "print total", "", "Hidden"));
    CHECK(!offers(S, "print total", "a = ", "MAXW"));
    CHECK(!offers(S, "print total", "a = ", "helper"));
    CHECK(offers(S, "    Area& = r.W * r.H", "    a = ", "MAXW"));
    CHECK(!offers(S, "    Area& = r.W * r.H", "    a = ", "total"));
    fx_done(S, files);
}

/* In a Quickr program a QBasic module's line was offered the Quickr files' names. */
TEST(mixcomplete_qr_main_offers_a_qbasic_lines_module_names)
{
    open_with(S, qr_main);
    CHECK(offers(S, "    Area& = r.W * r.H", "    a = ", "MAXW"));
    CHECK(!offers(S, "    Area& = r.W * r.H", "    a = ", "total"));
    CHECK(!offers(S, "    Area& = r.W * r.H", "    a = ", "helper"));
    fx_done(S, files);
}

/* After a list's, a dict's or a string's dot, a Quickr program offered nothing: no built-in
   method. */
TEST(mixcomplete_qr_main_offers_built_in_methods)
{
    open_with(S, qr_main);
    CHECK(offers(S, "print total", "xs.", "push"));
    CHECK(offers(S, "print total", "xs.", "sort"));
    CHECK(offers(S, "print total", "let a = xs.", "map"));
    CHECK(!offers(S, "print total", "let a = xs.", "keys"));
    CHECK(offers(S, "print total", "let a = d.", "keys"));
    CHECK(offers(S, "print total", "let a = d.", "get"));
    CHECK(!offers(S, "print total", "let a = d.", "push"));
    CHECK(offers(S, "print total", "let a = w.", "count"));
    CHECK(!offers(S, "print total", "let a = w.", "push"));
    CHECK(offers(S, "print total", "let a = s.", "moved"));
    CHECK(offers(S, "print total", "let a = s.", "x"));
    fx_done(S, files);
}

/* After a Quickr module's name a QBasic program offered a function and a record it can't use:
   a list result, a list field. */
TEST(mixcomplete_qb_main_leaves_out_what_it_cant_use)
{
    open_with(S, qb_main);
    CHECK(offers(S, "print total", "a = geo.", "area"));
    CHECK(!offers(S, "print total", "a = geo.", "names"));
    CHECK(!offers(S, "print total", "dim q as geo.", "Bag"));
    fx_done(S, files);
}

/* An edit to a Quickr module's exports reached a QBasic program's completion only at the next
   run: helper no more private was not offered; one made wrong kept the old exports. */
TEST(mixcomplete_qb_main_follows_a_quickr_modules_edits)
{
    open_with(S, qb_main);
    CHECK(!offers(S, "print total", "a = geo.", "helper"));
    fx_typed_over(S, fx_line_of(S, "private function helper() as int"), "function helper() as int");
    CHECK(offers(S, "print total", "a = geo.", "helper"));
    CHECK(offers(S, "print total", "a = shapes.", "Area"));
    CHECK(offers(S, "print total", "a = shapes.", "MAXW"));
    fx_typed_over(S, fx_line_of(S, "    return 1"), "    return \"x\"");
    CHECK(!offers(S, "print total", "a = geo.", "area")); /* an error: none till it checks */
    fx_typed_over(S, fx_line_of(S, "    return \"x\""), "    return 1");
    CHECK(offers(S, "print total", "a = geo.", "area"));
    fx_done(S, files);
}

/* An edit to a QBasic module's exports reached a Quickr program's completion only at the next
   run: a SUB and a TYPE field renamed were offered by their old names. */
TEST(mixcomplete_qr_main_follows_a_qbasic_modules_edits)
{
    open_with(S, qr_main);
    fx_typed_over(S, fx_line_of(S, "sub Show (a as long)"), "sub Shew (a as long)");
    CHECK(offers(S, "print total", "shapes.", "Shew"));
    CHECK(!offers(S, "print total", "shapes.", "Show"));
    fx_typed_over(S, fx_line_of(S, "    W as long"), "    Wd as long");
    CHECK(offers(S, "print total", "r.", "Wd"));
    CHECK(!offers(S, "print total", "r.", "W"));
    fx_done(S, files);
}
