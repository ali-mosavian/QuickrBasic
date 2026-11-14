/* Completion's harder corpus: a real program, and what only reading it tells: a procedure's
   parameters, a record's fields, a statement's later parts, the blocks open lines above. */
#include "session.h"
#include "unit.h"
#include "corpus.h"

static const char *const program[] = {
    "10 TYPE item",
    "20 nm AS STRING * 10",
    "30 price AS SINGLE",
    "40 END TYPE",
    "50 DIM SHARED stock(20) AS item, count AS INTEGER",
    "60 CONST maxn = 20",
    "70 OPEN \"stock.dat\" FOR INPUT AS #1",
    "80 DO WHILE NOT EOF(1)",
    "90 count = count + 1",
    "100 INPUT #1, stock(count).nm, stock(count).price",
    "110 LOOP",
    "120 CLOSE #1",
    "130 total! = 0",
    "140 FOR k = 1 TO count",
    "150 total! = total! + cost!(stock(k).price, 2)",
    "160 NEXT",
    "170 PRINT USING \"###.##\"; total!",
    "180 SUB report (first AS INTEGER, last AS INTEGER)",
    "190 FOR r = first TO last",
    "200 PRINT stock(r).nm",
    "210 NEXT",
    "220 END SUB",
    "230 FUNCTION cost! (p AS SINGLE, qty AS INTEGER)",
    "240 cost! = p * qty",
    "250 END FUNCTION",
    nullptr,
};

static const CorpusCase cases[] = {
    /* a procedure's parameters: another after one, its ')' after the last */
    {"x = cost!(1.5", "1.5,", ",", "", "1.5)", 0},
    {"x = cost!(1.5, 2", "2)", ")", "", "2,", 0},
    {"x = cost!(stock(1).price, count", "COUNT)", ")", "", "COUNT,", 0},
    {"call report(1", "1,", ",", "", "", 0},
    {"call report(1, maxn", "MAXN)", ")", "", "", 0},
    {"report 1", "1,", ",", "", "", 0},
    {"report 1, maxn", nullptr, "", "", "MAXN,", 0},
    /* the program's files */
    {"close #", "1", "1", "", "", 0},
    {"print #", "1", "1", "", "", 0},
    {"x = eof(", "1", "1", "", "", 0},
    /* a record's fields */
    {"print stock(1).", nullptr, "", "NM|PRICE", "COUNT|PRINT", 0},
    {"print stock(1).p", "PRICE", "RICE", "", "", 0},
    {"x$ = stock(1).", "NM", "", "", "PRICE", 0},
    {"dim x as it", "ITEM", "EM", "", "", 0},
    /* a statement's later parts */
    {"open \"f\" for ", nullptr, "", "INPUT|OUTPUT|APPEND|RANDOM|BINARY", "PRINT|COUNT", 0},
    {"open \"f\" for input", "INPUT AS", " AS", "", "", 0},
    {"open \"f\" for input as", "AS #", " #", "", "", 0},
    {"if count > maxn", "MAXN THEN", " THEN", "", "", 0},
    {"select ", "CASE", "CASE", "", "", 0},
    {"select case cost!(1, 2)", nullptr, "", "", "", 0},
    {"print using \"###.##\"; tot", "TOTAL!", "AL!", "", "", 0},
    /* blocks open lines above */
    {"next ", "K", "K", "", "R", 160},
    {"lo", "LOOP", "OP", "", "", 110},
    {"next ", "R", "R", "", "K", 210},
    /* a procedure's own names */
    {"for r = first to l", "LAST", "AST", "", "", 190},
    {"cost! = p * q", "QTY", "TY", "", "", 240},
    {"cost! = p * ", nullptr, "", "P|QTY", "AS|INTEGER|FIRST", 240},
    {"print stock(r).n", "NM", "M", "", "", 200},
};

/* Every case as expected. */
TEST(suggest_smart) { CHECK(!corpus_run(S, program, cases, countof(cases))); }
