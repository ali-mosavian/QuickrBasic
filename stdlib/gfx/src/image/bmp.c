/* BMP to pixels and palette (image.h). */
#include "image/image.h"
#include "core/mem.h"

enum {
    FILE_HEAD = 14,
    INFO_HEAD = 40, /* BITMAPINFOHEADER: the least read */
    COLOURS = 256,
    MOST = 1 << 14, /* pixels a side */
};

static uint32_t le16(const uint8_t *const p) { return p[0] | (uint32_t)p[1] << 8; }

static uint32_t le32(const uint8_t *const p) { return le16(p) | le16(p + 2) << 16; }

ImgStatus img_bmp(const uint8_t *const data, const uint32_t n, Image *const out)
{
    if (n < FILE_HEAD + INFO_HEAD || data[0] != 'B' || data[1] != 'M')
        return IMG_NOT_ONE;

    const uint8_t *const info = data + FILE_HEAD;
    const uint32_t bits_at = le32(data + 10), info_size = le32(info);
    const int32_t w = (int32_t)le32(info + 4), height = (int32_t)le32(info + 8);
    const int32_t h = height < 0 ? -height : height;
    const uint32_t used = le32(info + 32), colours = used ? used : COLOURS;
    const uint32_t pitch = ((uint32_t)w + 3) & ~3u, palette_at = FILE_HEAD + info_size;

    if (info_size < INFO_HEAD || le16(info + 12) != 1 || le16(info + 14) != 8 || le32(info + 16) ||
        w <= 0 || w > MOST || !h || h > MOST || colours > COLOURS || palette_at > n ||
        colours * 4 > n - palette_at || bits_at > n || (uint64_t)pitch * h > n - bits_at)
        return IMG_NOT_ONE;

    uint8_t *const pixels = mem_alloc((uint32_t)(w * h));
    uint32_t *const palette = mem_alloc(COLOURS * sizeof *palette);

    if (!pixels || !palette) {
        mem_free(pixels);
        mem_free(palette);
        return IMG_NO_MEMORY;
    }

    for (uint32_t i = 0; i < colours; i++) {
        const uint8_t *const c = data + palette_at + i * 4;
        palette[i] = (uint32_t)c[2] << 16 | (uint32_t)c[1] << 8 | c[0];
    }

    for (int32_t y = 0; y < h; y++) {
        const int32_t row = height < 0 ? y : h - 1 - y;
        mem_copy(pixels + y * w, data + bits_at + (uint32_t)row * pitch, (size_t)w);
    }

    *out = (Image){
        .w = w,
        .h = h,
        .format = PX_8,
        .pixels = pixels,
        .palette = palette,
    };

    return IMG_OK;
}
