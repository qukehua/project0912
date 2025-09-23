#pragma once
/*
 * cmd_frame.h
 * 应用层通用底层协议 —— 控制指令/参数型注入/应答帧（单帧）
 * 符合《以太网通信协议（星务管理）》：
 *   标识符=0xEB90；版本=0；类型=0；副导头标识=0；
 *   APID=11bit（见 APID.h）；分组标志=0b11（单帧）；源包序列计数=14bit（调用者维护，初值0）
 *   数据域 = [ 指令码(2B) | 参数(0..N) ]，N ≤ 1000（保证单帧且与数据帧上限一致）
 *   数据域长度 = 2 + N
 *   和校验16bit = 对 [主导头(不含标识符) + 数据域长度(2B) + 数据域] 单字节累加求和取反（~sum），取低16bit
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 固定字段（统一） */
#define NIXYK_FRAME_IDENTIFIER         0xEB90u
#define NIXYK_ACK_FRAME_IDENTIFIER     0x1ACFu
#define NIXYK_FRAME_VERSION            0u     /* 3bit */
#define NIXYK_FRAME_TYPE               0u     /* 1bit */
#define NIXYK_FRAME_SUBHDR             0u     /* 1bit */

/* 分组标志（2bit）—— 控制/应答固定为单帧 */
#define NIXYK_SEG_MIDDLE               0u     /* 0b00：中间段 */
#define NIXYK_SEG_HEAD                 1u     /* 0b01：首段 */
#define NIXYK_SEG_TAIL                 2u     /* 0b10：尾段 */
#define NIXYK_SEG_SINGLE               3u     /* 0b11：单帧（控制/应答固定） */

/* 统一阈值：控制帧单帧参数上限；与“数据帧数据域中的数据”1000B保持一致，保证不会触发分段 */
#define NIXYK_CMD_PARAM_MAX_BYTES      1000u

/* 14bit 源包序列计数封顶 */
#define NIXYK_SEQ14_MAX                0x3FFFu
static inline uint16_t nixyk_seq14_inc(uint16_t s) { return (uint16_t)((s + 1u) & NIXYK_SEQ14_MAX); }

/* 组帧输入 */
typedef struct {
    uint8_t   dev7;           /* APID 高7位（设备标识）——见 APID.h */
    uint8_t   dtype4;         /* APID 低4位（数据类型），控制=0x0，应答=0x7 ——见 APID.h */
    uint16_t  seq14;          /* 源包序列计数（14bit），调用者维护，初值0 */
    uint16_t  cmd_code;       /* 指令码（来自 cmd_codes.h）*/
    const uint8_t* params;    /* 参数指针（可为 NULL）*/
    uint16_t  param_len;      /* 参数长度（0..1000）*/
} nixyk_cmd_frame_t;

/* 编码：生成控制/参数注入/应答帧（单帧） */
int nixyk_cmd_build(const nixyk_cmd_frame_t* in,
                    uint8_t* out, size_t cap, size_t* out_len);

/* 解析：从应答帧中提取指令码和参数（含基本一致性校验） */
typedef struct {
    uint16_t apid11;          /* 原始 APID 11bit（便于上层按设备/类型路由）*/
    uint8_t  seg_flag;        /* 2bit 分组标志（应为单帧=0b11）*/
    uint16_t seq14;           /* 源包序列计数（14bit）*/
    uint16_t cmd_code;        /* 指令码 */
    const uint8_t* params;    /* 指向缓冲区内参数区（不拷贝）*/
    uint16_t param_len;       /* 参数长度 */
} nixyk_cmd_ack_view_t;

/* 仅做帧内字段与校验检查；不做“语义状态”解释 */
int nixyk_cmd_parse_ack(const uint8_t* buf, size_t len, nixyk_cmd_ack_view_t* view);

/* 供测试/自检：计算 16bit 和校验（~sum16） */
uint16_t nixyk_checksum16(const uint8_t* p, size_t n);

#ifdef __cplusplus
}
#endif
