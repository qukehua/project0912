#ifndef DATA_FRAME_H
#define DATA_FRAME_H

#include <stdint.h>

// 数据帧固定标识符
#define DATA_FRAME_ID 0x1ACF
// 最大数据长度（文档定义最长65536字节）
#define MAX_DATA_LEN 65536

// 分组标志（GrID）
#define GRID_FIRST 0b01  // 首段（多帧起始）
#define GRID_MIDDLE 0b00 // 中间段
#define GRID_LAST 0b10   // 尾段（多帧结束）
#define GRID_SINGLE 0b11 // 单帧（未分段）

// 数据帧主导头（位域定义）
typedef struct
{
    uint16_t id;            // 标识符（固定0x1ACF）
    uint8_t version : 3;    // 版本号（固定0b000）
    uint8_t type : 1;       // 类型（固定0b0）
    uint8_t sub_header : 1; // 副导头标识（固定0b0）
    uint16_t apid : 11;     // 应用过程标识（11bit）
    uint8_t grid : 2;       // 分组标志
    uint16_t nuid : 14;     // 源包序列计数
    uint16_t data_len;      // 数据域长度（包数据长度减1）
} DataHeader;

// 数据帧结构体（动态数组指针形式）
typedef struct
{
    DataHeader header; // 主导头
    uint8_t *data;     // 数据域指针（动态分配）
    uint16_t checksum; // 和校验（包头+数据域双字节求和取低16bit）
} DataFrame;

// 函数声明

/**
 * @brief 初始化数据帧结构体
 * @param frame 指向DataFrame结构体的指针
 * @param apid 应用过程标识（11位，0~2047）
 * @param grid 分组标志（GRID_FIRST/GRID_MIDDLE/GRID_LAST/GRID_SINGLE）
 */
void data_frame_init(DataFrame *frame, uint16_t apid, uint8_t grid);

/**
 * @brief 计算数据帧的和校验值
 * @param frame 指向DataFrame结构体的指针
 * @return 返回计算得出的校验值（包头+数据域双字节求和取低16位）
 */
uint16_t data_calc_checksum(const DataFrame *frame);

/**
 * @brief 设置数据帧的数据域内容
 * @param frame 指向DataFrame结构体的指针
 * @param data 数据缓冲区指针
 * @param data_len 数据长度（最大65536字节）
 * @note 该函数会动态分配内存存储数据，使用后需调用data_frame_free释放
 */
void data_frame_set_data(DataFrame *frame, const uint8_t *data, uint16_t data_len);

/**
 * @brief 释放数据帧的动态内存
 * @param frame 指向DataFrame结构体的指针
 * @note 释放data字段动态分配的内存，防止内存泄漏
 */
void data_frame_free(DataFrame *frame);

/**
 * @brief 将数据帧编码为字节流
 * @param frame 指向DataFrame结构体的指针
 * @param buf 输出缓冲区指针，用于存放编码后的数据
 * @param buf_len 输出缓冲区长度
 * @return 返回实际编码的字节数，0表示编码失败
 */
uint16_t data_frame_encode(const DataFrame *frame, uint8_t *buf, uint16_t buf_len);

/**
 * @brief 从字节流解码为数据帧
 * @param frame 指向DataFrame结构体的指针，用于存放解码结果
 * @param buf 输入缓冲区指针，包含待解码的字节流
 * @param buf_len 输入缓冲区长度
 * @return 返回解码状态：>=0表示成功，<0表示失败（如校验错误、长度不足等）
 * @note 解码成功后会动态分配内存存储数据，使用后需调用data_frame_free释放
 */
int8_t data_frame_decode(DataFrame *frame, const uint8_t *buf, uint16_t buf_len);

#endif
