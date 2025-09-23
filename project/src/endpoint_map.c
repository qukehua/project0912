#include "endpoint_map.h"
#include <string.h>
#include <stdio.h>

/*
 * 端点映射（依据 Q/NI XYK2112-2024）
 *
 * 1) APID 高7位（ruid）来自“附录A 以太网设备应用过程标识（APID）”；
 * 2) 对端 IPv6 接口地址来自“附录B 本地链路 IP 地址”（固定末4bit=0x2，路由交换组件为 0x1）；
 * 3) UDP 端口号来自“附录C 应用层端口号”，
 *    其中“与自主管理进行 UDP 通信”的端口统一为 10030（自主管理组件自身端口范围 10031~10062，仅作说明，不作为对端端口）。
 * 4) apid_low_data：文件/工参类多帧传输的数据类型低4位，按附录A备注“0b1111：文件型传输”，统一取 0xF。
 *
 * 说明：
 * - “综合电子组件（星务管理软件）”并非 ETH1 的对端设备，附录B未给出其接口 IPv6，本表不列入；
 * - 若将来自上层的 apid_low_data 指定为非 0，则以上层为准覆盖本表的默认 0xF。
 */

typedef struct {
    uint8_t  ruid;             /* APID 高7位（二进制转为数值存储） */
    char     ip6[40];          /* 对端 IPv6 管理口（附录B“接口IP地址”） */
    uint16_t port;             /* 与自主管理进行 UDP 通信端口（附录C，基本均为 10030） */
    uint16_t apid_low_data;    /* 文件/数据类低4位：0xF=文件型传输 */
    const char* name;          /* 设备名（便于日志） */
} map_rec_t;

/* ========== 静态映射表 ========== */
/* 附录A：将“设备标识高7位（二进制）”转成数值写入 ruid 字段 */
/* 附录B：接口IPv6；附录C：与自主管理进行UDP通信 → 端口 10030 */
static map_rec_t g_table[] = {
    /* 设备名                     ruid(高7位)  IPv6                     端口    数据低4位 */
    { "基带处理组件",            0x18,        "FC00::5A18:0002",       10030,  0xF },
    { "自主管理组件",            0x1B,        "FC00::5A1B:0002",       10031,  0xF }, /* 端口说明：自主管理侧自身范围 10031~10062；这里给出起始值仅用于占位，通常不会作为对端访问 */
    { "星间通信终端1(管理口)",   0x28,        "FC00::5A28:0002",       10030,  0xF },
    { "星间通信终端2(管理口)",   0x2B,        "FC00::5A2B:0002",       10030,  0xF },
    { "星间通信终端3(管理口)",   0x2D,        "FC00::5A2D:0002",       10030,  0xF },
    { "星间通信终端4(管理口)",   0x2E,        "FC00::5A2E:0002",       10030,  0xF },
    { "导航增强",                0x32,        "FC00::5A32:0002",       10030,  0xF },
    { "路由交换组件",            0x35,        "FC00::5A35:0001",       10030,  0xF }, /* 附录B该设备接口地址末4bit为 0x1 */
    { "星上供电控制组件",        0x36,        "FC00::5A36:0002",       10030,  0xF },
    { "星上安全组件",            0x39,        "FC00::5A39:0002",       10030,  0xF },
    { "通用计算组件",            0x3A,        "FC00::5A3A:0002",       10030,  0xF },
    { "预留载荷1",              0x3C,        "FC00::5A3C:0002",       10030,  0xF },
    { "预留载荷2",              0x3F,        "FC00::5A3F:0002",       10030,  0xF },
    { "预留载荷3",              0x5E,        "FC00::5A50:0002",       10030,  0xF },
    { "预留载荷4",              0x60,        "FC00::5A60:0002",       10030,  0xF },
};
/* ============================== */

static int g_count = (int)(sizeof(g_table)/sizeof(g_table[0]));

/* 查询映射：
 *  - 成功返回 0，并写出 ip6/port/apid_low_data
 *  - 失败返回 -1
 */
int endpoint_query_by_ruid(uint8_t ruid, char ip6[40], uint16_t* port, uint16_t* apid_low_data)
{
    for (int i = 0; i < g_count; ++i){
        if ((g_table[i].ruid & 0x7F) == (ruid & 0x7F)){
            if (ip6) strncpy(ip6, g_table[i].ip6, 40);
            if (port) *port = g_table[i].port;
            if (apid_low_data) *apid_low_data = g_table[i].apid_low_data;
            return 0;
        }
    }
    return -1;
}

/* 运行期覆盖/新增：
 *  - 仅支持覆盖已有项（为避免静态数组越界，阶段A不做追加）
 *  - 成功返回 0；未找到返回 -2；参数错误返回 -1
 */
int endpoint_override(uint8_t ruid, const char* ip6, uint16_t port, uint16_t apid_low_data)
{
    if (!ip6 || !*ip6) return -1;

    for (int i = 0; i < g_count; ++i){
        if ((g_table[i].ruid & 0x7F) == (ruid & 0x7F)){
            strncpy(g_table[i].ip6, ip6, sizeof(g_table[i].ip6));
            g_table[i].port = port;
            g_table[i].apid_low_data = apid_low_data;
            return 0;
        }
    }
    return -2;
}
