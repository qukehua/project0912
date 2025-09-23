#pragma once
/*
 * data_frame.h
 * 应用层通用底层协议 —— 数据帧（文件型/业务型）
 * 仅组帧，不做解析；应答由 cmd_frame 解析。
 *
 * 规范要点：
 *   标识符=0xEB90；版本=0；类型=0；副导头标识=0；
 *   APID=11bit（见 APID.h，数据类型通常为 0xF=文件型 或其他业务类型）；
 *   分组标志(2bit)：0b01=首段、0b00=中间段、0b10=尾段、0b11=单帧；
 *   源包序列计数(14bit)：调用者维护，初值0；
 *   包数据域 = [ 文件段号(2B，起始0) | 数据(0..1000B) ]；
 *   数据域长度 = 2 + 数据长度（最大 0x03EA=1002），满足“数据不超过1000B”的硬要求；
 *   和校验16bit = 对 [主导头(不含标识符)+数据域长度+数据域] 单字节累加求和取反（~sum），取低16bit。
 */

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NIXYK_FRAME_IDENTIFIER         0xEB90u
#define NIXYK_FRAME_VERSION            0u
#define NIXYK_FRAME_TYPE               0u
#define NIXYK_FRAME_SUBHDR             0u

/* 分组标志（2bit） */
#define NIXYK_SEG_MIDDLE               0u     /* 0b00：中间段 */
#define NIXYK_SEG_HEAD                 1u     /* 0b01：首段 */
#define NIXYK_SEG_TAIL                 2u     /* 0b10：尾段 */
#define NIXYK_SEG_SINGLE               3u     /* 0b11：单帧 */

#define NIXYK_SEQ14_MAX                0x3FFFu
// static inline uint16_t nixyk_seq14_inc(uint16_t s){ return (uint16_t)((s + 1u) & NIXYK_SEQ14_MAX); }

/* 数据长度硬限制（仅“数据”部分），来自规范图示“0~1000 Byte” */
#define NIXYK_DATA_MAX_BYTES           1000u

typedef struct {
    uint8_t   dev7;          /* APID 高7位（设备） */
    uint8_t   dtype4;        /* APID 低4位（数据类型），文件型传输通常=0xF */
    uint8_t   seg_flag;      /* 2bit 分组标志：HEAD/MIDDLE/TAIL/SINGLE */
    uint16_t  seq14;         /* 源包序列计数（14bit） */
    uint16_t  seg_no;        /* 文件段号（2B），从 0 起 */
    const uint8_t* data;     /* 数据指针（可为 NULL，当 data_len=0） */
    uint16_t  data_len;      /* 数据长度（0..1000） */
} nixyk_data_frame_t;

/* 组帧（单个分片） */
int nixyk_data_build(const nixyk_data_frame_t* in,
                     uint8_t* out, size_t cap, size_t* out_len);

/* 供测试/自检 */
uint16_t nixyk_data_checksum16(const uint8_t* p, size_t n);

#ifdef __cplusplus
}
#endif
