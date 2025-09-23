#ifndef COMMAND_FRAME_H
#define COMMAND_FRAME_H

#include <stdint.h>

// 指令帧固定标识符
#define CMD_FRAME_ID 0xEB90

// 类型标识
#define CMD_TYPE_STANDARD 0x55 // 规范定义
#define CMD_TYPE_CUSTOM 0xAA   // 自行定义

// 重点指令码（16bit）
#define CMD_FILE_START 0x0155     // 文件传输开启
#define CMD_FILE_END 0x01AA       // 文件传输结束
#define CMD_FILE_UPDATE 0x01BB    // 文件更新通知
#define CMD_PROGRESS_QUERY 0x01CC // 文件更新进度查询

// 设备标识（APID高7位，二进制）   数据类型低4位  0001~1111
#define APID_SYS_ELEC 0000000   // 综合电子组件
#define APID_BASEBAND 0011000   // 基带处理组件
#define APID_AUTOMNG 0011011    // 自主管理组件
#define APID_INTERCOM1 0101000  // 星间通信终端1
#define APID_INTERCOM2 0101011  // 星间通信终端2
#define APID_INTERCOM3 0101101  // 星间通信终端3
#define APID_INTERCOM4 0101110  // 星间通信终端4
#define APID_ROUTE 0110101      // 路由交换组件
#define APID_POWER_CTRL 0110110 // 星上馈电控制组件
#define APID_SECURITY 0111001   // 星上安全组件
#define APID_COMPUTE 0111010    // 通用计算组件

// 数据文件类型（APID低4位）
#define FILE_RECONFIG 0b0000        // 重构数据
#define FILE_BEAM_PLAN 0b0001       // 波位规划
#define FILE_ORBIT_ALARM 0b0010     // 升降轨距离预警数据
#define FILE_SPECTRUM 0b0011        // 频谱监测数据
#define FILE_LASER_IMG 0b0100       // 星间激光终端图像信息
#define FILE_POWER_PLAN 0b0101      // 星地馈电计划
#define FILE_EPHEMERIS_BATCH 0b0110 // 直接建链邻星批量星历
#define FILE_IP_UPDATE 0b0111       // 邻星IP更新
#define FILE_EPHEMERIS 0b1000       // 星历数据

// 指令帧结构体
typedef struct
{
    uint16_t id;         // 标识符（固定0xEB90）
    uint8_t type;        // 类型标识
    uint8_t seq_count;   // 指令序列计数（0x00~0xFF）
    uint8_t cmd_len;     // 指令域长度（0x02~0xFF）
    uint16_t cmd_code;   // 指令码
    uint8_t params[254]; // 参数（最长254字节）
    uint8_t checksum;    // 和校验（包头+指令域单字节求和取低8bit）
} CommandFrame;

// 函数声明

/**
 * @brief 初始化指令帧结构体
 * @param frame 指向CommandFrame结构体的指针
 * @param type 类型标识（CMD_TYPE_STANDARD或CMD_TYPE_CUSTOM）
 * @param cmd_code 指令码（16位）
 */
void cmd_frame_init(CommandFrame *frame, uint8_t type, uint16_t cmd_code);

/**
 * @brief 计算指令帧的和校验值
 * @param frame 指向CommandFrame结构体的指针
 * @return 返回计算得出的校验值（包头+指令域单字节求和取低8位）
 */
uint8_t cmd_calc_checksum(const CommandFrame *frame);

/**
 * @brief 设置指令帧的参数数据
 * @param frame 指向CommandFrame结构体的指针
 * @param params 参数数据缓冲区指针
 * @param param_len 参数数据长度（最大254字节）
 */
void cmd_frame_set_param(CommandFrame *frame, const uint8_t *params, uint8_t param_len);

/**
 * @brief 将指令帧编码为字节流
 * @param frame 指向CommandFrame结构体的指针
 * @param buf 输出缓冲区指针，用于存放编码后的数据
 * @param buf_len 输出缓冲区长度
 * @return 返回实际编码的字节数，0表示编码失败
 */
uint16_t cmd_frame_encode(const CommandFrame *frame, uint8_t *buf, uint16_t buf_len);

/**
 * @brief 从字节流解码为指令帧
 * @param frame 指向CommandFrame结构体的指针，用于存放解码结果
 * @param buf 输入缓冲区指针，包含待解码的字节流
 * @param buf_len 输入缓冲区长度
 * @return 返回解码状态：>=0表示成功，<0表示失败（如校验错误、长度不足等）
 */
int8_t cmd_frame_decode(CommandFrame *frame, const uint8_t *buf, uint16_t buf_len);

#endif