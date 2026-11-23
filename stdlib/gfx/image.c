/* An image by its first bytes (image.h). */
#include "image.h"

ImgStatus img_load(const uint8_t *const data, const uint32_t n, Image *const out)
{
    if (n >= 2 && data[0] == 'B' && data[1] == 'M')
        return img_bmp(data, n, out);

    return img_png(data, n, out);
}
