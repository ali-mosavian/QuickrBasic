/* A help topic's lines and links, and the history (helpdoc.h).
 *
 * # How it works
 * A topic's text (tools/mkhelp.py's format): the title, then lines, 0x0A apart. In a line, 0x01 and
 * a topic id start a link, 0x02 ends it; 0x03 first makes the line a heading. The marks take no
 * columns. hdoc_load scans the text once for each line's offset and each link's columns.
 */
#include "ui/helpdoc.h"
#include "plat/pmem.h"

enum : uint8_t { LINK_BEGIN = 1, LINK_END = 2, HEADING = 3 };

/* One pass over body lines; with fill, the offsets and links are stored; the lines and links counted. */
static void scan(HelpDoc *const d, const bool fill, uint16_t *const lines, uint16_t *const links)
{
    uint32_t i = d->title_len + 1;
    uint16_t nl = 0, nk = 0;

    while (i <= d->len) {
        if (fill)
            d->line[nl] = i;

        uint16_t col = 0;
        HelpLink *open = nullptr;

        while (i < d->len && d->text[i] != '\n') {
            const uint8_t c = d->text[i++];

            if (c == LINK_BEGIN && i + 2 <= d->len) {
                if (fill) {
                    open = &d->link[nk];
                    *open = (HelpLink){nl, col, col, (uint16_t)(d->text[i] | d->text[i + 1] << 8), 0};
                    const uint8_t first = i + 3 < d->len ? d->text[i + 3] : ' '; /* past the arrow */
                    open->letter = first >= 'a' && first <= 'z' ? (uint8_t)(first - 32) : first;
                }

                nk++;
                i += 2;
            } else if (c == LINK_END) {
                if (open)
                    open->ecol = (uint16_t)(col - 1);

                open = nullptr;
            } else if (c != HEADING) {
                col++;
            }
        }

        nl++;
        i++;
    }

    *lines = nl;
    *links = nk;
}

bool hdoc_load(HelpDoc *const d, const uint8_t *const text, const uint32_t len)
{
    *d = (HelpDoc){};
    d->text = pmem_resize(nullptr, len + 1);

    if (!d->text)
        return false;

    memcpy(d->text, text, len);
    d->len = len;

    while (d->title_len < len && text[d->title_len] != '\n')
        d->title_len++;

    uint16_t lines, links;
    scan(d, false, &lines, &links);
    d->line = pmem_resize(nullptr, (lines + 1u) * sizeof *d->line);
    d->link = pmem_resize(nullptr, (links + 1u) * sizeof *d->link);

    if (!d->line || !d->link) {
        hdoc_free(d);
        return false;
    }

    scan(d, true, &d->nlines, &d->nlinks);
    return true;
}

void hdoc_free(HelpDoc *const d)
{
    pmem_free(d->text);
    pmem_free(d->line);
    pmem_free(d->link);
    *d = (HelpDoc){};
}

uint16_t hdoc_row(const HelpDoc *const d, const uint16_t i, uint8_t *const ch, uint8_t *const kind, const uint16_t max)
{
    if (i >= d->nlines)
        return 0;

    uint32_t p = d->line[i];
    uint8_t k = HK_TEXT;
    uint16_t n = 0;

    while (p < d->len && d->text[p] != '\n') {
        const uint8_t c = d->text[p++];

        if (c == LINK_BEGIN) {
            k = HK_LINK;
            p += 2;
        } else if (c == LINK_END) {
            k = HK_TEXT;
        } else if (c == HEADING) {
            k = HK_HEAD;
        } else if (n < max) {
            ch[n] = c;
            kind[n++] = k;
        }
    }

    return n;
}

uint16_t hdoc_step(const HelpDoc *const d, const int32_t cur, const int32_t step)
{
    if (!d->nlinks)
        return HDOC_NONE;

    const int32_t n = d->nlinks;
    const int32_t from = cur < 0 ? (step > 0 ? -1 : 0) : cur;
    return (uint16_t)(((from + step) % n + n) % n);
}

uint16_t hdoc_letter(const HelpDoc *const d, const int32_t cur, const uint8_t letter, const bool backwards)
{
    const int32_t n = d->nlinks;

    for (int32_t k = 1; k <= n; k++) {
        const int32_t at = backwards ? ((cur < 0 ? n : cur) - k + 2 * n) % n : (cur + k + n) % n;

        if (d->link[at].letter == letter)
            return (uint16_t)at;
    }

    return HDOC_NONE;
}

uint16_t hdoc_at(const HelpDoc *const d, const uint16_t line, const uint16_t col)
{
    for (uint16_t k = 0; k < d->nlinks; k++)
        if (d->link[k].line == line && col >= d->link[k].col && col <= d->link[k].ecol)
            return k;

    return HDOC_NONE;
}

uint16_t hdoc_first(const HelpDoc *const d, const uint16_t first, const uint16_t rows)
{
    for (uint16_t k = 0; k < d->nlinks; k++)
        if (d->link[k].line >= first && d->link[k].line < first + rows)
            return k;

    return d->nlinks ? 0 : HDOC_NONE;
}

void hhist_push(HelpHist *const h, const uint16_t id)
{
    if (h->n && h->id[h->n - 1] == id)
        return;

    if (h->n == HIST_MAX) {
        memmove(h->id, h->id + 1, (HIST_MAX - 1) * sizeof h->id[0]);
        h->n--;
    }

    h->id[h->n++] = id;
}

bool hhist_pop(HelpHist *const h, uint16_t *const id)
{
    if (!h->n)
        return false;

    *id = h->id[--h->n];
    return true;
}

static bool word_char(const uint8_t c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' ||
           c == '_' || c == '$' || c == '%' || c == '&' || c == '!' || c == '#';
}

uint16_t hdoc_word(const uint8_t *const s, const uint16_t len, uint16_t col, char *const out, const uint16_t max)
{
    if (col > len)
        return 0;
    if (col == len || !word_char(s[col]))
        col = col && word_char(s[col - 1]) ? col - 1 : len; /* the one just left of the cursor */
    if (col >= len)
        return 0;

    uint16_t a = col, b = col + 1;

    while (a && word_char(s[a - 1]))
        a--;
    while (b < len && word_char(s[b]))
        b++;

    if (b - a + 1u > max)
        return 0;

    memcpy(out, s + a, b - a);
    out[b - a] = 0;
    return (uint16_t)(b - a);
}
