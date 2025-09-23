#ifndef DATA_FRAME_ETH1_H
#define DATA_FRAME_ETH1_H

#include <stdint.h>
#include <stddef.h>

#ifndef DATA_FRAME_IDENTIFIER
#define DATA_FRAME_IDENTIFIER   0xDADA  /* 2B 帧标识（占位值） */
#endif

#ifndef DATA_PAYLOAD_MAX
#define DATA_PAYLOAD_MAX        1400    /* 以太网单包最大有效载荷（保守值） */
#endif

typedef enum {
    GRID_SINGLE = 0,  /* 单包 */
    GRID_FIRST  = 1,  /* 分段-首包 */
    GRID_MIDDLE = 2,  /* 分段-中包 */
    GRID_LAST   = 3   /* 分段-尾包 */
} grid_t;

typedef struct data_frame_s {
    uint16_t apid;        /* 低11位有效 */
    grid_t   grid;        /* 2bit */
    uint16_t nul_id;      /* 14bit 序号 */
    const uint8_t* payload;
    uint16_t payload_len;
} data_frame_t;

#ifdef __cplusplus
extern "C"{
#endif

int data_encode(const data_frame_t *in, uint8_t *out, size_t cap, size_t *olen);
int data_decode(const uint8_t *buf, size_t len, data_frame_t *out, uint8_t *payload, size_t cap);

#ifdef __cplusplus
}
#endif
#endif /* DATA_FRAME_ETH1_H */
