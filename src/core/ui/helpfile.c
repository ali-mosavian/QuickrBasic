/* The help file: BASIC.HLP, as tools/mkhelp.py writes it (its header says how).
 *
 * # How it works
 * hlf_open reads everything but the topics' data into one block: the key index is searched in
 * memory, a binary search over the sorted keys, so a lookup is about a dozen string compares and
 * no disk. hlf_read seeks to one topic, reads its compressed bytes and unpacks them over the
 * dictionary: the file stays closed between reads, as QBasic's did. A damaged file is refused, not
 * trusted: every offset and length is checked.
 */
#include "ui/helpfile.h"
#include "plat/pmem.h"

enum { HEAD = 48, KEY_MAX = 64, TOPIC_ENT = 8, KEY_ENT = 4, VERSION = 1 };

static uint16_t le16(const uint8_t *const p) { return (uint16_t)(p[0] | p[1] << 8); }
static uint32_t le32(const uint8_t *const p) { return le16(p) | (uint32_t)le16(p + 2) << 16; }

static bool read_at(const char *const path, const uint32_t at, void *const dst, const uint32_t n)
{
    PfilHandle h;
    uint32_t got = 0;

    if (pfil_open(path, PFIL_READ, &h) != PFIL_OK)
        return false;

    const bool ok = pfil_seek(h, (int32_t)at, PFIL_SET, nullptr) == PFIL_OK &&
                    pfil_read(h, dst, n, &got) == PFIL_OK && got == n;
    pfil_close(h);
    return ok;
}

void hlf_close(HelpFile *const f)
{
    pmem_free(f->tab);
    pmem_free(f->work);
    *f = (HelpFile){};
}

bool hlf_open(HelpFile *const f, const char *const path)
{
    uint8_t head[HEAD];
    *f = (HelpFile){};
    const uint32_t n = (uint32_t)strlen(path);

    if (n >= sizeof f->path)
        return false;

    memcpy(f->path, path, n + 1);

    if (!read_at(path, 0, head, HEAD) || memcmp(head, "QBHL", 4) || le16(head + 4) != VERSION)
        return false;

    f->ntopics = le16(head + 6);
    f->nkeys = le16(head + 8);
    f->dict_len = le16(head + 10);
    f->contents = le16(head + 12);
    f->index = le16(head + 14);
    f->using_help = le16(head + 16);
    f->status = le16(head + 18);
    f->survival = le16(head + 20);
    f->nnav = le16(head + 22);
    f->off_topics = le32(head + 24);
    f->off_keys = le32(head + 28);
    f->off_pool = le32(head + 32);
    f->off_dict = le32(head + 36);
    f->off_data = le32(head + 40);
    const uint32_t size = f->off_data;

    if (f->off_topics != HEAD || f->off_keys != f->off_topics + f->ntopics * TOPIC_ENT ||
        f->off_pool != f->off_keys + f->nkeys * KEY_ENT || f->off_dict < f->off_pool ||
        f->off_data != f->off_dict + f->dict_len || f->nnav > f->ntopics) {
        *f = (HelpFile){};
        return false;
    }

    f->tab = pmem_resize(nullptr, size);

    if (!f->tab || !read_at(path, 0, f->tab, size)) {
        hlf_close(f);
        return false;
    }

    return true;
}

/* Key number i's bytes: its length, then the characters in *s. */
static uint32_t key_at(const HelpFile *const f, const uint32_t i, const uint8_t **const s)
{
    const uint8_t *const p = f->tab + f->off_pool + le16(f->tab + f->off_keys + i * KEY_ENT);
    *s = p + 1;
    return p[0];
}

/* key as stored: capitals, one space between words, none at the ends; its length, 0 if too long. */
static uint32_t normal(const char *const key, const uint32_t len, uint8_t *const out)
{
    uint32_t n = 0;
    bool gap = false;

    for (uint32_t i = 0; i < len; i++) {
        uint8_t c = (uint8_t)key[i];

        if (c == ' ' || c == '\t') {
            gap = n != 0;
            continue;
        }
        if (gap) {
            if (n >= KEY_MAX)
                return 0;
            out[n++] = ' ';
            gap = false;
        }
        if (n >= KEY_MAX)
            return 0;

        out[n++] = c >= 'a' && c <= 'z' ? (uint8_t)(c - 32) : c;
    }

    return n;
}

static int compare(const uint8_t *const a, const uint32_t na, const uint8_t *const b, const uint32_t nb)
{
    const uint32_t n = na < nb ? na : nb;
    const int c = memcmp(a, b, n);

    return c ? c : (na > nb) - (na < nb);
}

uint16_t hlf_find(const HelpFile *const f, const char *const key, const uint32_t len)
{
    uint8_t want[KEY_MAX];
    const uint32_t n = normal(key, len, want);

    if (!n)
        return HLF_NONE;

    uint32_t lo = 0, hi = f->nkeys;

    while (lo < hi) {
        const uint32_t mid = (lo + hi) / 2;
        const uint8_t *s;
        const uint32_t sn = key_at(f, mid, &s);
        const int c = compare(want, n, s, sn);

        if (!c)
            return le16(f->tab + f->off_keys + mid * KEY_ENT + 2);
        if (c < 0)
            hi = mid;
        else
            lo = mid + 1;
    }

    return HLF_NONE;
}

/* in's n bytes unpacked after the dictionary in buf, size bytes of text; false: damaged. */
static bool unpack(
    const uint8_t *const in,
    const uint32_t n,
    uint8_t *const buf,
    const uint32_t dict,
    const uint32_t size
)
{
    uint32_t i = 0, o = dict;
    const uint32_t end = dict + size;

    while (o < end) {
        if (i >= n)
            return false;

        uint8_t flags = in[i++];

        for (uint32_t b = 0; b < 8 && o < end; b++, flags >>= 1) {
            if (flags & 1) {
                if (i >= n)
                    return false;

                buf[o++] = in[i++];
                continue;
            }
            if (i + 2 > n)
                return false;

            const uint16_t v = le16(in + i);
            i += 2;
            uint32_t len = (v & 7u) + 3;

            if (len == 10) {
                if (i >= n)
                    return false;

                len += in[i++];
            }

            const uint32_t dist = (v >> 3) + 1u;

            if (dist > o || o + len > end)
                return false;

            for (uint32_t k = 0; k < len; k++, o++)
                buf[o] = buf[o - dist];
        }
    }

    return true;
}

const uint8_t *hlf_read(HelpFile *const f, const uint16_t id, uint32_t *const len)
{
    if (id >= f->ntopics)
        return nullptr;

    const uint8_t *const ent = f->tab + f->off_topics + id * TOPIC_ENT;
    const uint32_t off = le32(ent), clen = le16(ent + 4), rlen = le16(ent + 6);
    const uint32_t need = f->dict_len + rlen + clen;

    if (need > f->work_cap) {
        pmem_free(f->work);
        f->work = pmem_resize(nullptr, need);
        f->work_cap = f->work ? need : 0;

        if (!f->work)
            return nullptr;

        memcpy(f->work, f->tab + f->off_dict, f->dict_len);
    }

    uint8_t *const comp = f->work + f->dict_len + rlen;

    if (!read_at(f->path, f->off_data + off, comp, clen) ||
        !unpack(comp, clen, f->work, f->dict_len, rlen))
        return nullptr;

    *len = rlen;
    return f->work + f->dict_len;
}
