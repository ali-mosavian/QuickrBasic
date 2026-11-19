/* The help file (ui/helpfile), a topic's lines and links (ui/helpdoc), the history. The file is
   tests/help compiled by tools/mkhelp.py into build/host/test.hlp. */
#include "ui/helpdoc.h"
#include "ui/helpfile.h"
#include "unit.h"

static const char file[] = "build/host/test.hlp";

static bool find(HelpFile *const f, const char *const key, const uint16_t topic)
{
    return hlf_find(f, key, (uint32_t)strlen(key)) == topic;
}

/* The topic keyed key, loaded; false if it is not there. */
static bool load(HelpFile *const f, HelpDoc *const d, const char *const key)
{
    const uint16_t id = hlf_find(f, key, (uint32_t)strlen(key));
    uint32_t len;
    const uint8_t *const text = id == HLF_NONE ? nullptr : hlf_read(f, id, &len);
    return text && hdoc_load(d, text, len);
}

TEST(help_key_found_in_any_case_and_spacing)
{
    HelpFile f;
    CHECK(hlf_open(&f, file));
    const uint16_t alpha = hlf_find(&f, "ALPHA", 5);
    CHECK(alpha != HLF_NONE);
    CHECK(find(&f, "alpha", alpha) && find(&f, "  Alpha$ ", alpha) && find(&f, "al", alpha));
    CHECK(hlf_find(&f, "BETA   THING", 12) == hlf_find(&f, "beta thing", 10));
    CHECK(hlf_find(&f, "nosuch", 6) == HLF_NONE && hlf_find(&f, "", 0) == HLF_NONE);
    CHECK(hlf_find(&f, "ALPH", 4) == HLF_NONE && hlf_find(&f, "ALPHAS", 6) == HLF_NONE);
    CHECK(hlf_find(&f, "~table", 6) == HLF_NONE); /* a hidden key is its text, not the ~ */
    CHECK(hlf_find(&f, "table", 5) == f.contents);
    hlf_close(&f);
}

/* A 400-line topic is past the 8 KB window: its read must give back every byte. */
TEST(help_long_topic_reads_back_whole_and_small)
{
    HelpFile f;
    HelpDoc d;
    CHECK(hlf_open(&f, file));
    CHECK(load(&f, &d, "long"));
    CHECK(d.nlines >= 400);
    CHECK(d.title_len == 10 && !memcmp(d.text, "Long Topic", 10));
    uint8_t ch[80], kind[80];
    const uint16_t n = hdoc_row(&d, 2 + 399, ch, kind, 80); /* nav, blank, then the lines */
    CHECK(n > 40 && !memcmp(ch, " Line 399: the quick", 20));
    CHECK(d.len > 20000);
    hdoc_free(&d);
    hlf_close(&f);
}

TEST(help_status_topic_is_the_status_file)
{
    HelpFile f;
    uint32_t len;
    CHECK(hlf_open(&f, file));
    const uint8_t *const t = hlf_read(&f, f.status, &len);
    CHECK(t && len == 23 && !memcmp(t, "First line\nSecond line\n", 23));
    CHECK(f.nnav == f.status); /* Ctrl+F1 does not step into it */
    hlf_close(&f);
}

TEST(help_damaged_file_is_refused_not_trusted)
{
    static const char bad[] = "build/host/bad.hlp";
    uint8_t buf[4096];
    PfilHandle h;
    uint32_t n = 0;
    CHECK(pfil_open(file, PFIL_READ, &h) == PFIL_OK);
    CHECK(pfil_read(h, buf, sizeof buf, &n) == PFIL_OK && n > 100);
    pfil_close(h);
    buf[0] = 'X'; /* its magic */
    CHECK(pfil_create(bad, &h) == PFIL_OK);
    uint32_t put;
    CHECK(pfil_write(h, buf, n, &put) == PFIL_OK);
    pfil_close(h);
    HelpFile f;
    CHECK(!hlf_open(&f, bad));
    CHECK(!hlf_open(&f, "build/host/nothere.hlp"));
    buf[0] = 'Q';
    CHECK(pfil_create(bad, &h) == PFIL_OK);
    CHECK(pfil_write(h, buf, 100, &put) == PFIL_OK); /* the tables cut off */
    pfil_close(h);
    CHECK(!hlf_open(&f, bad));
    HelpFile whole;
    CHECK(hlf_open(&whole, file));
    CHECK(pfil_create(bad, &h) == PFIL_OK);
    CHECK(pfil_write(h, buf, whole.off_data + 10, &put) == PFIL_OK); /* the data cut off */
    pfil_close(h);
    CHECK(hlf_open(&f, bad)); /* its tables are whole */
    uint32_t len;
    CHECK(!hlf_read(&f, (uint16_t)(f.ntopics - 1), &len)); /* a read past the end is no crash */
    hlf_close(&f);
    hlf_close(&whole);
    CHECK(!pfil_delete(bad));
}

TEST(help_links_have_columns_and_row_cells_hide_the_marks)
{
    HelpFile f;
    HelpDoc d;
    CHECK(hlf_open(&f, file));
    CHECK(load(&f, &d, "alpha"));
    /* QuickSCREEN, Details, Example, Contents, Index, then Beta, Gamma, Alpha again, Zulu */
    CHECK(d.nlinks == 9);
    CHECK(d.link[0].line == 0 && d.link[0].col == 3 && d.link[0].ecol == 15); /* spread over the row */
    CHECK(d.link[3].topic == f.contents && d.link[4].topic == f.index);
    uint8_t ch[80], kind[80];
    uint16_t n = hdoc_row(&d, 0, ch, kind, 80);
    CHECK(n >= 69 && !memcmp(ch + 3, "\x11" "QuickSCREEN\x10", 13) && !memcmp(ch + 62, "\x11" "Index\x10", 7));
    CHECK(kind[3] == HK_LINK && kind[15] == HK_LINK && kind[16] == HK_TEXT);
    n = hdoc_row(&d, 1, ch, kind, 80); /* the rule under it */
    CHECK(n == 79 && ch[0] == ' ' && ch[1] == 0xC4 && ch[78] == 0xC4);
    n = hdoc_row(&d, 5, ch, kind, 80); /* the line under the heading's */
    CHECK(n > 0);
    uint16_t head = 0;
    for (uint16_t i = 0; i < d.nlines; i++)
        if (hdoc_row(&d, i, ch, kind, 80) && kind[0] == HK_HEAD)
            head = i;
    CHECK(head && hdoc_row(&d, head, ch, kind, 80) == 6 && !memcmp(ch, " Notes", 6)); /* its 0x03 is no column */
    CHECK(hdoc_at(&d, 0, 5) == 0 && hdoc_at(&d, 0, 16) == HDOC_NONE && hdoc_at(&d, 0, 63) == 4);
    hdoc_free(&d);
    hlf_close(&f);
}

TEST(help_tab_wraps_and_a_letter_jumps)
{
    HelpFile f;
    HelpDoc d;
    CHECK(hlf_open(&f, file));
    CHECK(load(&f, &d, "alpha"));
    CHECK(hdoc_step(&d, -1, 1) == 0 && hdoc_step(&d, -1, -1) == 8); /* none: the first, the last */
    CHECK(hdoc_step(&d, 8, 1) == 0 && hdoc_step(&d, 0, -1) == 8);
    CHECK(hdoc_letter(&d, 0, 'Z', false) == 8);           /* Zulu */
    CHECK(hdoc_letter(&d, 8, 'B', false) == 5);       /* wraps to Beta */
    CHECK(hdoc_letter(&d, 5, 'B', false) == 5);           /* from Beta, the only B is Beta */
    CHECK(hdoc_letter(&d, 8, 'A', true) == 7);            /* backwards: Alpha again */
    CHECK(hdoc_letter(&d, 0, 'X', false) == HDOC_NONE);
    CHECK(hdoc_first(&d, 0, 5) == 0 && hdoc_first(&d, 7, 20) == 8 && hdoc_first(&d, 20, 5) == 0); /* in view, else the first */
    hdoc_free(&d);
    hlf_close(&f);
}

TEST(help_history_keeps_twenty_skips_repeats_and_pops_newest_first)
{
    HelpHist h = {};
    uint16_t id;
    CHECK(!hhist_pop(&h, &id));
    hhist_push(&h, 7);
    hhist_push(&h, 7);
    CHECK(h.n == 1);
    for (uint16_t i = 1; i <= 25; i++)
        hhist_push(&h, i);
    CHECK(h.n == HIST_MAX);
    CHECK(hhist_pop(&h, &id) && id == 25);
    CHECK(hhist_pop(&h, &id) && id == 24);
    for (int i = 0; i < 18; i++)
        CHECK(hhist_pop(&h, &id));
    CHECK(id == 6 && !hhist_pop(&h, &id)); /* 26 pushes, the oldest 6 dropped: 6 is the last kept */
}

static bool word_is(const char *const line, const uint16_t col, const char *const want)
{
    char w[32];
    const uint16_t n = hdoc_word((const uint8_t *)line, (uint16_t)strlen(line), col, w, sizeof w);
    return want ? n == strlen(want) && !strcmp(w, want) : n == 0;
}

/* F1 on a keyword's last letter, the space after it, or past the line's end found nothing: the
   cursor sits there as often as on the word's first letter. */
TEST(help_word_under_the_cursor)
{
    CHECK(word_is("PRINT a$", 0, "PRINT") && word_is("PRINT a$", 4, "PRINT"));
    CHECK(word_is("PRINT a$", 5, "PRINT"));  /* on the space after it */
    CHECK(word_is("PRINT a$", 6, "a$") && word_is("PRINT a$", 8, "a$")); /* past the end of the line */
    CHECK(word_is("x = LEFT$(a$, 2)", 6, "LEFT$"));
    CHECK(word_is("  ", 1, nullptr) && word_is("", 0, nullptr));
    CHECK(word_is("a   b", 1, "a")); /* a gap's first space is the word before's */
    CHECK(word_is("a   b", 2, nullptr)); /* inside a gap: none */
    CHECK(word_is("PRINT", 9, nullptr)); /* far past the end */
}

/* Every topic had no Details or Example: QuickSCREEN's nav now reaches both, and each leads back. */
TEST(help_quickscreen_details_and_example_link_both_ways)
{
    HelpFile f;
    HelpDoc d;
    CHECK(hlf_open(&f, file));
    CHECK(load(&f, &d, "alpha"));
    const uint16_t quick = hlf_find(&f, "alpha", 5);
    static const char *const want[] = {"Alpha Statement Details", "Alpha Statement Example"};

    for (uint16_t k = 0; k < 2; k++) {
        const uint16_t id = d.link[1 + k].topic;
        uint32_t len;
        const uint8_t *const text = hlf_read(&f, id, &len);
        HelpDoc part;
        CHECK(text && hdoc_load(&part, text, len));
        CHECK(part.title_len == strlen(want[k]) && !memcmp(part.text, want[k], part.title_len));
        CHECK(part.link[0].topic == quick && part.link[1 + (1 - k)].topic != id);
        hdoc_free(&part);
    }

    CHECK(hlf_find(&f, "Alpha Statement Details", 23) == HLF_NONE); /* parts are not keywords */
    hdoc_free(&d);
    hlf_close(&f);
}
