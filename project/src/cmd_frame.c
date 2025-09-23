#include "cmd_frame.h"
#include "APID.h"      /* 你的 APID.h */
#include <string.h>
#include <stdio.h>

/* --- 内部工具 --- */
static inline void be16_put(uint8_t* p, uint16_t v){ p[0]=(uint8_t)(v>>8); p[1]=(uint8_t)v; }
static inline uint16_t be16_get(const uint8_t* p){ return (uint16_t)((p[0]<<8)|p[1]); }

uint16_t nixyk_checksum16(const uint8_t* p, size_t n)
{
    uint32_t s = 0;
    for (size_t i=0;i<n;i++) s += p[i];
    s = (~s) & 0xFFFFu; /* 和校验16bit：逐字节累加求和取反，低16位 */
    return (uint16_t)s;
}

/* 将“包主导头（不含标识符）”4B写入 out[2..5]，分组/序列再写入 out[6..7]，保持可读性 */
static void write_main_header(uint8_t* out, uint8_t dev7, uint8_t dtype4, uint8_t seg2, uint16_t seq14)
{
    /* wordA: 版本(3) | 类型(1) | 副导头标识(1) | APID(11)  */
    uint16_t apid11 = NIXYK_APID(dev7 & 0x7F, dtype4 & 0x0F);
    uint16_t wordA  = (uint16_t)((NIXYK_FRAME_VERSION & 0x7) << 13)
                    | (uint16_t)((NIXYK_FRAME_TYPE    & 0x1) << 12)
                    | (uint16_t)((NIXYK_FRAME_SUBHDR  & 0x1) << 11)
                    | (uint16_t)(apid11 & 0x07FF);
    /* wordB: 分组标志(2) | 源包序列计数(14) */
    uint16_t wordB  = (uint16_t)(((seg2 & 0x3) << 14) | (seq14 & 0x3FFF));

    be16_put(out+2, wordA);
    be16_put(out+4, wordB);
}

int nixyk_cmd_build(const nixyk_cmd_frame_t* in,
                    uint8_t* out, size_t cap, size_t* out_len)
{
    if (!in || !out || !out_len) return -1;
    if (in->param_len > NIXYK_CMD_PARAM_MAX_BYTES) return -2;

    /* 数据域：指令码(2) + 参数(N) */
    uint16_t data_len = (uint16_t)(2u + in->param_len);

    /* 帧总长 = 标识符(2) + 主导头(4) + 数据域长度(2) + 数据域(data_len) + 和校验(2) */
    size_t need = 2u + 4u + 2u + (size_t)data_len + 2u;
    if (cap < need) return -3;

    /* 写固定标识符 */
    be16_put(out+0, NIXYK_FRAME_IDENTIFIER);

    /* 主导头：APID + 分组标志（控制/应答=单帧）+ 序列 */
    write_main_header(out, in->dev7, in->dtype4, NIXYK_SEG_SINGLE, in->seq14);

    /* 数据域长度 */
    be16_put(out+6, data_len-1);

    /* 数据域：指令码 + 参数 */
    be16_put(out+8, in->cmd_code);
    if (in->param_len && in->params) {
        memcpy(out+10, in->params, in->param_len);
    }

    /* 和校验：对 [主导头(4B)+数据域长度(2B)+数据域] 计算 */
    uint16_t csum = nixyk_checksum16(out+2, (size_t)4u + 2u + (size_t)data_len);
    be16_put(out+10+in->param_len, csum);

    *out_len = need;
    return 0;
}

int nixyk_cmd_parse_ack(const uint8_t* buf, size_t len, nixyk_cmd_ack_view_t* view)
{
    if (!buf || !view) return -1;
    if (len < 2u+4u+2u+2u+2u) return -2; /* 最小：ID+主导头+len+code+校验 */

    /* 校验标识符 */
    if (be16_get(buf) != NIXYK_ACK_FRAME_IDENTIFIER) return -3;

    uint16_t wordA = be16_get(buf+2);
    uint16_t wordB = be16_get(buf+4);
    uint16_t data_len = be16_get(buf+6);

    /* 长度与边界检查 */
    size_t expect = 2u + 4u + 2u + (size_t)data_len + 2u;
    if (len < expect) return -4;

    /* 校验 */
    uint16_t got = be16_get(buf+8+(data_len+1));
    uint16_t cal = nixyk_checksum16(buf+2, 4u+2u+(size_t)(data_len+1));
    printf("校验：got=0x%04X, cal=0x%04X\n", got, cal);
    if (got != cal) return -5;

    /* 抽取字段 */
    uint16_t apid11 = (uint16_t)(wordA & 0x07FF);
    uint8_t  seg2   = (uint8_t)((wordB >> 14) & 0x3);
    uint16_t seq14  = (uint16_t)(wordB & 0x3FFF);

    /* 数据域至少 2B 指令码 */
    if (data_len < 2u) return -6;
    uint16_t code = be16_get(buf+8);
    uint16_t plen = (uint16_t)(data_len - 2u);

    view->apid11    = apid11;
    view->seg_flag  = seg2;
    view->seq14     = seq14;
    view->cmd_code  = code;
    view->params    = plen ? (buf+10) : NULL;
    view->param_len = plen;

    return 0;
}
