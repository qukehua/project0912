#include "data_frame.h"
#include "APID.h"
#include <string.h>

static inline void be16_put(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)v;
}
static inline uint16_t be16_get(const uint8_t *p) { return (uint16_t)((p[0] << 8) | p[1]); }

uint16_t nixyk_data_checksum16(const uint8_t *p, size_t n)
{
    uint32_t s = 0;
    for (size_t i = 0; i < n; i++)
        s += p[i];
    s = (~s) & 0xFFFFu;
    return (uint16_t)s;
}

static void write_main_header(uint8_t *out, uint8_t dev7, uint8_t dtype4, uint8_t seg2, uint16_t seq14)
{
    uint16_t apid11 = NIXYK_APID(dev7 & 0x7F, dtype4 & 0x0F);
    uint16_t wordA = (uint16_t)((NIXYK_FRAME_VERSION & 0x7) << 13) | (uint16_t)((NIXYK_FRAME_TYPE & 0x1) << 12) | (uint16_t)((NIXYK_FRAME_SUBHDR & 0x1) << 11) | (uint16_t)(apid11 & 0x07FF);
    uint16_t wordB = (uint16_t)(((seg2 & 0x3) << 14) | (seq14 & 0x3FFF));
    be16_put(out + 2, wordA);
    be16_put(out + 4, wordB);
}

int nixyk_data_build(const nixyk_data_frame_t *in,
                     uint8_t *out, size_t cap, size_t *out_len)
{
    if (!in || !out || !out_len)
        return -1;
    if (in->data_len > NIXYK_DATA_MAX_BYTES)
        return -2;
    if ((in->seg_flag & 0xFC) != 0)
        return -3; /* 仅2bit有效 */

    /* 数据域 = 段号(2B) + 数据(N) */
    uint16_t data_len = (uint16_t)(2u + in->data_len);

    /* 帧总长 = ID(2) + 主导头(4) + 长度(2) + 数据域(data_len) + 校验(2) */
    size_t need = 2u + 4u + 2u + (size_t)data_len + 2u;
    if (cap < need)
        return -4;

    /* 写标识符 */
    be16_put(out + 0, NIXYK_FRAME_IDENTIFIER);

    /* 主导头 */
    write_main_header(out, in->dev7, in->dtype4, (uint8_t)(in->seg_flag & 0x3), in->seq14);

    /* 数据域长度 */
    be16_put(out + 6, data_len);

    /* 数据域：段号(2B) + 数据 */
    be16_put(out + 8, in->seg_no);
    if (in->data_len && in->data)
    {
        memcpy(out + 10, in->data, in->data_len);
    }

    /* 和校验 */
    uint16_t csum = nixyk_data_checksum16(out + 2, 4u + 2u + (size_t)data_len);
    be16_put(out + 10 + in->data_len, csum);

    *out_len = need;
    return 0;
}
