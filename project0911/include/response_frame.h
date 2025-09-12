#ifndef RESPONSE_FRAME_H
#define RESPONSE_FRAME_H

#include <stdint.h>

// 应答帧固定标识符
#define RESP_FRAME_ID 0xEB91

// 应答帧固定类型标识
#define RESP_TYPE 0xAA

// 应答长度（固定值）
#define RESP_LEN 0x05

// 应答码2定义（位操作）
#define RESP_NORMAL (0 << 7) // bit7：执行正常0
#define RESP_ERROR (1 << 7)  // bit7：执行错误1
#define TRANS_ING (0 << 0)   // bit1-0：传输中00
#define TRANS_DONE (1 << 0)  // bit1-0：传输完成01
#define TRANS_ERROR (3 << 0) // bit1-0：传输错误11

// 应答帧结构体
typedef struct
{
    uint16_t id;         // 标识符（固定0xEB91）
    uint8_t type;        // 类型标识（固定0xAA）
    uint8_t len;         // 应答长度（固定0x05）
    uint16_t resp_code1; // 应答码1（指令码或数据帧标识符）
    uint16_t resp_code2; // 应答码2（执行结果）
    uint8_t checksum;    // 和校验（包头+数据域单字节求和取低8bit）
} ResponseFrame;

// 函数声明

/**
 * @brief 初始化应答帧结构体
 * @param frame 指向ResponseFrame结构体的指针
 * @param code1 应答码1（指令码或数据帧标识符）
 * @param code2 应答码2（执行结果，包含执行状态和传输状态位）
 */
void resp_frame_init(ResponseFrame *frame, uint16_t code1, uint16_t code2);

/**
 * @brief 计算应答帧的和校验值
 * @param frame 指向ResponseFrame结构体的指针
 * @return 返回计算得出的校验值（包头+数据域单字节求和取低8位）
 */
uint8_t resp_calc_checksum(const ResponseFrame *frame);

/**
 * @brief 将应答帧编码为字节流
 * @param frame 指向ResponseFrame结构体的指针
 * @param buf 输出缓冲区指针，用于存放编码后的数据
 * @param buf_len 输出缓冲区长度
 * @return 返回实际编码的字节数，0表示编码失败
 */
uint16_t resp_frame_encode(const ResponseFrame *frame, uint8_t *buf, uint16_t buf_len);

/**
 * @brief 从字节流解码为应答帧
 * @param frame 指向ResponseFrame结构体的指针，用于存放解码结果
 * @param buf 输入缓冲区指针，包含待解码的字节流
 * @param buf_len 输入缓冲区长度
 * @return 返回解码状态：>=0表示成功，<0表示失败（如校验错误、长度不足等）
 */
int8_t resp_frame_decode(ResponseFrame *frame, const uint8_t *buf, uint16_t buf_len);

#endif
