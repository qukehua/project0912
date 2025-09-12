#ifndef CMD_FRAME_H
#define CMD_FRAME_H

#include <stdint.h>
#include <stddef.h>

/* === 帧常量（占位值，可按协议调整） === */
#ifndef CMD_FRAME_IDENTIFIER
#define CMD_FRAME_IDENTIFIER   0x55AA  /* 2B 帧标识 */
#endif

#ifndef CMD_TYPE_CUSTOM
#define CMD_TYPE_CUSTOM        0x55    /* 自定义/应用层命令类型 */
#endif

#ifdef __cplusplus
extern "C"{
#endif

typedef struct cmd_frame_s {
    uint8_t  type;       /* 命令类型 */
    uint8_t  seq;        /* 序号 */
    uint16_t code;       /* 指令码（见 cmd_codes.h） */
    const uint8_t* params; /* 参数区（可为NULL） */
    uint8_t  param_len;  /* 参数长度（<=254） */
} cmd_frame_t;

/* 编解码（见 src/cmd_frame.c） */
int cmd_encode(const cmd_frame_t *in, uint8_t *out, size_t cap, size_t *olen);
int cmd_decode(const uint8_t *buf, size_t len, cmd_frame_t *out, uint8_t *param_store, size_t cap);

#ifdef __cplusplus
}
#endif
#endif /* CMD_FRAME_H */
