#ifndef RESP_FRAME_H
#define RESP_FRAME_H

#include <stdint.h>
#include <stddef.h>

/* === 帧常量（占位值，可按协议调整） ===
 * 格式：
 *  [0..1] 2B: 标识
 *  [2]    1B: 类型
 *  [3]    1B: 固定长度（本实现=0x04，对应resp1/resp2各2B）
 *  [4..5] 2B: resp1（回传或应答码）
 *  [6..7] 2B: resp2（状态/错误位）
 *  [8]    1B: 校验（sum8）
 */
#ifndef RESP_FRAME_IDENTIFIER
#define RESP_FRAME_IDENTIFIER  0xA55A
#endif
#ifndef RESP_TYPE
#define RESP_TYPE              0xAA
#endif
#ifndef RESP_LEN_FIXED
#define RESP_LEN_FIXED         0x04
#endif

#ifdef __cplusplus
extern "C"{
#endif

typedef struct resp_frame_s {
    uint16_t resp1;
    uint16_t resp2;
} resp_frame_t;

int resp_encode(const resp_frame_t *in, uint8_t *out, size_t cap, size_t *olen);
int resp_decode(const uint8_t *buf, size_t len, resp_frame_t *out);

#ifdef __cplusplus
}
#endif
#endif /* RESP_FRAME_H */
