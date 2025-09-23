#include "../include/data_frame_rs422.h"
#include <stdlib.h> // 用于malloc和free

// 初始化数据帧（指针初始化为NULL）
void data_frame_init(DataFrame *frame, uint16_t apid, uint8_t grid)
{
    if (frame == (void *)0)
        return;
    frame->header.id = DATA_FRAME_ID;
    frame->header.version = 0b000;     // 固定版本
    frame->header.type = 0b0;          // 固定类型
    frame->header.sub_header = 0b0;    // 无副导头
    frame->header.apid = apid & 0x7FF; // 仅保留低11位
    frame->header.grid = grid & 0x03;  // 仅保留低2位
    frame->header.nuid = 0x0000;       // 初始序列计数
    frame->header.data_len = 0x0000;   // 初始数据长度
    frame->data = (void *)0;           // 指针初始化为NULL
    frame->checksum = 0x0000;
}

// 计算数据帧校验和（基于动态数组）
uint16_t data_calc_checksum(const DataFrame *frame)
{
    if (frame == (void *)0 || frame->data == (void *)0)
        return 0x0000;
    uint32_t sum = 0x000000; // 用32位避免溢出

    // 累加主导头（按字节拆分）
    sum += (uint8_t)(frame->header.id >> 8);
    sum += (uint8_t)(frame->header.id & 0xFF);
    sum += (uint8_t)((frame->header.version << 5) |
                     (frame->header.type << 4) |
                     (frame->header.sub_header << 3) |
                     ((frame->header.apid >> 8) & 0x07)); // apid高3位
    sum += (uint8_t)(frame->header.apid & 0xFF);          // apid低8位
    sum += (uint8_t)((frame->header.grid << 6) |
                     ((frame->header.nuid >> 8) & 0x3F)); // grid+nuid高6位
    sum += (uint8_t)(frame->header.nuid & 0xFF);          // nuid低8位
    sum += (uint8_t)(frame->header.data_len >> 8);
    sum += (uint8_t)(frame->header.data_len & 0xFF);

    // 累加数据域（按字节，基于data_len）
    for (uint16_t i = 0; i <= frame->header.data_len; i++)
    {
        // data_len=实际长度-1
        sum += frame->data[i];
    }

    return (uint16_t)(sum & 0xFFFF); // 取低16位
}

// 设置数据帧数据（动态分配内存）
void data_frame_set_data(DataFrame *frame, const uint8_t *data, uint16_t data_len)
{
    if (frame == (void *)0 || data == (void *)0)
        return;

    // 释放已有内存（防止内存泄漏）
    if (frame->data != (void *)0)
    {
        free(frame->data);
        frame->data = (void *)0;
    }

    // 数据长度校验：不超过最大值，且为偶数字节
    uint16_t len = (data_len > MAX_DATA_LEN) ? MAX_DATA_LEN : data_len;
    len = (len % 2 != 0) ? len - 1 : len; // 确保偶数字节
    if (len == 0)
        return; // 至少2字节（文档要求）

    // 动态分配内存
    frame->data = (uint8_t *)malloc(len);
    if (frame->data == (void *)0)
        return; // 分配失败

    // 复制数据
    for (uint16_t i = 0; i < len; i++)
    {
        frame->data[i] = data[i];
    }
    frame->header.data_len = len - 1; // 数据域长度=实际长度-1
    frame->checksum = data_calc_checksum(frame);
}

// 释放数据帧动态内存
void data_frame_free(DataFrame *frame)
{
    if (frame == (void *)0)
        return;
    if (frame->data != (void *)0)
    {
        free(frame->data);
        frame->data = (void *)0; // 避免野指针
    }
    frame->header.data_len = 0x0000;
    frame->checksum = 0x0000;
}

// 数据帧编码：将结构体序列化为字节流，返回实际长度（含校验和）
uint16_t data_frame_encode(const DataFrame *frame, uint8_t *buf, uint16_t buf_len)
{
    if (frame == NULL || buf == NULL || frame->data == NULL)
    {
        return 0;
    }

    // 计算所需缓冲区长度：包头 8 字节 + 数据域（data_len+1） + 校验和 2 字节
    uint16_t required_len = 8 + (frame->header.data_len + 1) + 2;

    if (buf_len < required_len)
    {
        return 0; // 缓冲区不足
    }

    uint16_t pos = 0;
    // 主导头：标识符（16bit，大端）
    buf[pos++] = (frame->header.id >> 8) & 0xFF;
    buf[pos++] = frame->header.id & 0xFF;
    // 版本号 (3bit)+ 类型 (1bit)+ 副导头标识 (1bit)+APID 高 3bit (3bit)
    uint8_t byte1 = 0;
    byte1 |= (frame->header.version & 0x07) << 5;    // 版本号占 bit5-7
    byte1 |= (frame->header.type & 0x01) << 4;       // 类型占 bit4
    byte1 |= (frame->header.sub_header & 0x01) << 3; // 副导头标识占 bit3
    byte1 |= (frame->header.apid >> 8) & 0x07;       // APID 高 3bit 占 bit0-2
    buf[pos++] = byte1;
    // APID 低 8bit
    buf[pos++] = frame->header.apid & 0xFF;
    // 分组标志 (2bit)+ 源包序列计数高 6bit
    uint8_t byte3 = 0;
    byte3 |= (frame->header.grid & 0x03) << 6; // 分组标志占 bit6-7
    byte3 |= (frame->header.nuid >> 8) & 0x3F; // nuid 高 6bit 占 bit0-5
    buf[pos++] = byte3;
    // 源包序列计数低 8bit
    buf[pos++] = frame->header.nuid & 0xFF;
    // 数据域长度（16bit，大端）
    buf[pos++] = (frame->header.data_len >> 8) & 0xFF;
    buf[pos++] = frame->header.data_len & 0xFF;

    // 数据域（data_len+1 字节）
    for (uint16_t i = 0; i <= frame->header.data_len; i++)
    {
        buf[pos++] = frame->data[i];
    }

    // 计算并写入校验和（覆盖结构体中可能的旧值）
    uint16_t checksum = data_calc_checksum(frame);
    buf[pos++] = (checksum >> 8) & 0xFF;
    buf[pos++] = checksum & 0xFF;

    return pos; // 总长度 = 8 + (data_len+1) + 2
}

// 数据帧解码：从字节流解析到结构体，返回 0 成功，<0 失败
int8_t data_frame_decode(DataFrame *frame, const uint8_t *buf, uint16_t buf_len)
{
    if (frame == NULL || buf == NULL || frame->data != NULL)
    {
        return -1; // 需先调用 init 初始化（data 为 NULL）
    }

    // 基础包头长度 8 字节，校验和 2 字节，数据域至少 1 字节（data_len>=0）
    if (buf_len < 8 + 1 + 2)
    {
        return -2; // 长度不足
    }

    // 解析标识符
    frame->header.id = (buf[0] << 8) | buf[1];
    if (frame->header.id != DATA_FRAME_ID)
    {
        return -3; // 标识符错误
    }

    // 解析版本号、类型、副导头标识、APID
    uint8_t byte1 = buf[2];
    frame->header.version = (byte1 >> 5) & 0x07;
    frame->header.type = (byte1 >> 4) & 0x01;
    frame->header.sub_header = (byte1 >> 3) & 0x01;
    frame->header.apid = ((byte1 & 0x07) << 8) | buf[3]; // 高 3bit + 低 8bit
    // 解析分组标志和源包序列计数
    uint8_t byte3 = buf[4];
    frame->header.grid = (byte3 >> 6) & 0x03;
    frame->header.nuid = ((byte3 & 0x3F) << 8) | buf[5]; // 高 6bit + 低 8bit
    // 解析数据域长度并校验
    frame->header.data_len = (buf[6] << 8) | buf[7];
    uint16_t data_actual_len = frame->header.data_len + 1; // 实际数据长度

    // 总长度应为：8（包头） + data_actual_len + 2（校验和）
    if (buf_len != 8 + data_actual_len + 2)
    {
        return -4; // 数据长度不匹配
    }

    // 分配数据域内存并复制数据
    frame->data = (uint8_t *)malloc(data_actual_len);
    if (frame->data == NULL)
    {
        return -5; // 内存分配失败
    }
    for (uint16_t i = 0; i < data_actual_len; i++)
    {
        frame->data[i] = buf[8 + i]; // 8 是包头长度
    }

    // 校验和验证
    frame->checksum = (buf[buf_len - 2] << 8) | buf[buf_len - 1];
    uint16_t calc_checksum = data_calc_checksum(frame);
    if (frame->checksum != calc_checksum)
    {
        data_frame_free(frame); // 释放已分配内存
        return -6;              // 校验和错误
    }

    return 0; // 成功
}
