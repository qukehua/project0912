#ifndef CMD_CODES_H
#define CMD_CODES_H

/* 自动生成自《测试表-ZZGLV0.8-20250423.xlsx》->“指令-以太网”
 * 完整覆盖当前表中出现的全部以太网指令码，以及必要的参数取值/范围宏。
 * 命名策略：
 *   - 通用别名（如 CMD_CODE_FILE_START）+ 按“序号”兜底（CMD_3_1 等）
 *   - 参数宏：CMD_<序号>_PARAM<idx>_OK/ERR/IN_PROGRESS/...；范围给 MIN/MAX
 */

#include <stdint.h>

#define CMD_CODE_START_RECONSTRUCT          0x01AFu  /* 启动固件重构 */
#define CMD_CODE_RECONSTRUCT_RESULT_QUERY   0x01C5u  /* 重构结果查询 */
#define CMD_CODE_RECONSTRUCT_RESULT_RESP    0x01CAu  /* 重构结果查询应答 */
#define CMD_CODE_FW_VERSION_QUERY           0x0101u  /* 固件版本查询 */
#define CMD_CODE_FW_VERSION_RESP            0x0108u  /* 固件版本查询应答 */
#define CMD_CODE_SM_PREPARE_RECON           0x3403u  /* 自主重构组件准备 */
#define CMD_CODE_SM_PREPARE_RECON_RESP      0x3405u  /* 自主重构组件准备应答 */
#define CMD_CODE_SM_START_RECON             0x340Au  /* 自主重构组件开始 */
#define CMD_CODE_SM_RECON_RESULT_QUERY      0x340Eu  /* 自主重构结果查询 */
#define CMD_CODE_SM_RECON_RESULT_RESP       0x340Fu  /* 自主重构结果查询应答 */
#define CMD_CODE_SW_VERSION_QUERY           0x3503u  /* 软件版本查询指令 */
#define CMD_CODE_SW_VERSION_RESP            0x3505u  /* 软件版本查询指令应答 */
#define CMD_CODE_FACTORY_RESET              0x3506u  /* 恢复出厂设置指令 */
#define CMD_CODE_FACTORY_RESET_ACK          0x3407u  /* 恢复出厂设置指令应答 */
#define CMD_CODE_FACTORY_RESET_QUERY        0x3508u  /* 恢复出厂设置结果查询指令 */
#define CMD_CODE_FACTORY_RESET_QUERY_ACK    0x3409u  /* 恢复出厂设置结果查询指令应答 */
#define CMD_CODE_FILE_START                 0x0155u  /* 文件传输开始 */
#define CMD_CODE_FILE_START_ACK             0x015Au  /* 文件传输开始应答 */
#define CMD_CODE_FILE_END                   0x01AAu  /* 文件传输结束 */
#define CMD_CODE_FILE_END_ACK               0x01BBu  /* 文件传输结束应答 */
#define CMD_CODE_FILE_UDP_ACK               0x018Au  /* 文件传输（UDP）应答 */
#define CMD_CODE_FTP_NOTICE                 0x01D1u  /* FTP文件更新通知 */
#define CMD_CODE_FTP_NOTICE_ACK             0x01BFu  /* FTP文件更新通知应答 */
#define CMD_CODE_FTP_PROGRESS_QUERY         0x01DAu  /* FTP文件传输结果查询 */
#define CMD_CODE_FTP_PROGRESS_RESP          0x01DBu  /* FTP文件传输结果查询应答 */
#define CMD_CODE_SOFT_RESET                 0x001Au  /* 复位 */
#define CMD_CODE_LINK_CHECK                 0x001Du  /* 链路检测 */
#define CMD_CODE_LINK_CHECK_ACK             0x001Fu  /* 链路检测应答 */

/* ===== 指令码（按序号兜底） ===== */
#define CMD_1_1                             0x01AFu  /* 启动固件重构 */
#define CMD_1_2                             0x01C5u  /* 重构结果查询 */
#define CMD_1_3                             0x01CAu  /* 重构结果查询应答 */
#define CMD_1_4                             0x0101u  /* 固件版本查询 */
#define CMD_1_5                             0x0108u  /* 固件版本查询应答 */
#define CMD_2_1                             0x3403u  /* 自主重构组件准备 */
#define CMD_2_2                             0x3405u  /* 自主重构组件准备应答 */
#define CMD_2_3                             0x340Au  /* 自主重构组件开始 */
#define CMD_2_4                             0x340Eu  /* 自主重构结果查询 */
#define CMD_2_5                             0x340Fu  /* 自主重构结果查询应答 */
#define CMD_2_6                             0x3503u  /* 软件版本查询指令 */
#define CMD_2_7                             0x3505u  /* 软件版本查询指令应答 */
#define CMD_2_8                             0x3506u  /* 恢复出厂设置指令 */
#define CMD_2_9                             0x3407u  /* 恢复出厂设置指令应答 */
#define CMD_2_10                            0x3508u  /* 恢复出厂设置结果查询指令 */
#define CMD_2_11                            0x3409u  /* 恢复出厂设置结果查询指令应答 */
#define CMD_3_1                             0x0155u  /* 文件传输开始 */
#define CMD_3_2                             0x015Au  /* 文件传输开始应答 */
#define CMD_3_3                             0x01AAu  /* 文件传输结束 */
#define CMD_3_4                             0x01BBu  /* 文件传输结束应答 */
#define CMD_3_6                             0x018Au  /* 文件传输（UDP）应答 */
#define CMD_3_7                             0x018Fu  /* 文件传输异常终止 */
#define CMD_3_8                             0x018Eu  /* 文件传输异常终止应答 */
#define CMD_4_1                             0x018Cu  /* FTP文件写入查询 */
#define CMD_4_2                             0x018Du  /* FTP文件写入查询应答 */
#define CMD_4_3                             0x01D1u  /* FTP文件更新通知 */
#define CMD_4_4                             0x01BFu  /* FTP文件更新通知应答 */
#define CMD_4_5                             0x01DAu  /* FTP文件传输结果查询 */
#define CMD_4_6                             0x01DBu  /* FTP文件传输结果查询应答 */
#define CMD_5_1                             0x001Au  /* 复位 */
#define CMD_5_2                             0x001Du  /* 链路检测 */
#define CMD_5_3                             0x001Fu  /* 链路检测应答 */
#define CMD_6_1                             0x0001u  /* 邻星及本星星历分发 */
#define CMD_6_2                             0x01CBu  /* 本星星历分发 */

/* ===== 参数取值/范围（按表格） ===== */
/* 1-1 启动固件重构 */
#define CMD_1_1_PARAM1_MIN        0xF0u
#define CMD_1_1_PARAM1_MAX        0xFFu
#define CMD_1_1_PARAM2_MIN        0x00u
#define CMD_1_1_PARAM2_MAX        0xFFu

/* 1-3 重构结果查询应答 */
#define CMD_1_3_PARAM1_OK                             0x00u  /* 重构成功 */
#define CMD_1_3_PARAM1_IN_PROGRESS                    0x11u  /* 重构中 */
#define CMD_1_3_PARAM1_NOT_RECEIVED                   0x22u  /* 未收到启动重构 */
#define CMD_1_3_PARAM1_ERR                            0xFFu  /* 重构失败 */

/* 1-5 固件版本查询应答 */
#define CMD_1_5_PARAM1_MIN        0x00u
#define CMD_1_5_PARAM1_MAX        0xFFu  /* 32字节版本字符串（厂商自定义），范围宏仅作占位 */

/* 2-1 自主重构组件准备 */
#define CMD_2_1_PARAM1_MIN        0x00u
#define CMD_2_1_PARAM1_MAX        0xFFu
#define CMD_2_1_PARAM2_MIN        0x00u
#define CMD_2_1_PARAM2_MAX        0xFFu

/* 2-2 自主重构组件准备应答 */
#define CMD_2_2_PARAM1_OK                             0x00u  /* 准备正常 */
#define CMD_2_2_PARAM1_ERR                            0x11u  /* 错误 */
#define CMD_2_2_PARAM1_IN_PROGRESS                    0x22u  /* 进行中 */
#define CMD_2_2_PARAM1_UNSUPPORTED                    0xFFu  /* 不支持该操作 */

/* 2-5 自主重构结果查询应答 */
#define CMD_2_5_PARAM1_OK                             0x00u  /* 执行正常 */
#define CMD_2_5_PARAM1_ERR                            0x11u  /* 错误 */
#define CMD_2_5_PARAM1_IN_PROGRESS                    0x22u  /* 执行中 */
#define CMD_2_5_PARAM1_UNSUPPORTED                    0xFFu  /* 不支持该操作 */

/* 2-7 软件版本查询指令应答 */
#define CMD_2_7_PARAM1_MIN        0x00u
#define CMD_2_7_PARAM1_MAX        0xFFu  /* 32字节版本字符串（厂商自定义），范围宏仅作占位 */

/* 2-9 恢复出厂设置指令应答 */
#define CMD_2_9_PARAM1_OK                             0x00u  /* 开始执行 */
#define CMD_2_9_PARAM1_ERR                            0x11u  /* 不具备执行状态 */
#define CMD_2_9_PARAM1_UNSUPPORTED                    0xFFu  /* 不支持该操作 */

/* 2-11 恢复出厂设置结果查询指令应答 */
#define CMD_2_11_PARAM1_OK                            0x00u  /* 执行正常 */
#define CMD_2_11_PARAM1_ERR                           0x11u  /* 执行错误 */
#define CMD_2_11_PARAM1_IN_PROGRESS                   0x22u  /* 执行中 */
#define CMD_2_11_PARAM1_UNSUPPORTED                   0xFFu  /* 不支持该操作 */

/* 3-1 文件传输开始 */
#define CMD_3_1_PARAM1_MIN        0x00u  /* 重构设备标识 */
#define CMD_3_1_PARAM1_MAX        0xFFu
#define CMD_3_1_PARAM2_MIN        0xF0u  /* 文件类型：F0~FF（F0~FE 厂商自定义，FF 固件重构/FE 软件重构） */
#define CMD_3_1_PARAM2_MAX        0xFFu
#define CMD_3_1_PARAM3_MIN        0x00u  /* 文件子类型：00~FF */
#define CMD_3_1_PARAM3_MAX        0xFFu
#define CMD_3_1_PARAM4_MIN        0x00u  /* 是否分段（位域“2/8”），范围仅占位 */
#define CMD_3_1_PARAM4_MAX        0xFFu
#define CMD_3_1_PARAM5_MIN        0x0000u /* 段数（位域“14/8”），范围仅占位 */
#define CMD_3_1_PARAM5_MAX        0xFFFFu
#define CMD_3_1_PARAM6_MIN        0x00000000u /* 文件长度（4B，大端） */
#define CMD_3_1_PARAM6_MAX        0xFFFFFFFFu
#define CMD_3_1_PARAM7_MIN        0x00000000u /* 尾段长度（4B，大端） */
#define CMD_3_1_PARAM7_MAX        0xFFFFFFFFu
#define CMD_3_1_PARAM8_MIN        0x0000u     /* 文件CRC16-CCITT-FALSE（2B，大端） */
#define CMD_3_1_PARAM8_MAX        0xFFFFu

/* 3-4 文件传输结束应答 */
#define CMD_3_4_PARAM1_OK                             0x00u  /* 文件接收正常 */
#define CMD_3_4_PARAM1_ERR                            0x11u  /* 文件接收异常 */

/* 3-6 文件传输（UDP）应答（逐包） */
#define CMD_3_6_PARAM1_OK                             0x00u  /* 接收正常 */
#define CMD_3_6_PARAM1_ERR                            0xFFu  /* 接收异常 */

/* 3-7 文件传输异常终止 */
#define CMD_3_7_PARAM1_MIN        0x00u  /* 可自定义异常码/原因，若有 */
#define CMD_3_7_PARAM1_MAX        0xFFu

/* 4-1 FTP文件写入查询 */
#define CMD_4_1_PARAM1_MIN        0x00u
#define CMD_4_1_PARAM1_MAX        0xFFu

/* 4-2 FTP文件写入查询应答 */
#define CMD_4_2_PARAM1_NO_WRITE                   0x00u  /* 无写入需求 */
#define CMD_4_2_PARAM1_NEED_WRITE                 0xFFu  /* 有写入需求 */
#define CMD_4_2_PARAM2_MIN        0x00u
#define CMD_4_2_PARAM2_MAX        0xFFu  /* 32字节文件名（ASCII，不足补0），范围宏仅作占位 */

/* 4-3 FTP文件更新通知 */
#define CMD_4_3_PARAM1_MIN        0x00u  /* 文件类型（1B） */
#define CMD_4_3_PARAM1_MAX        0xFFu
#define CMD_4_3_PARAM2_MIN        0x00u  /* 文件子类型（1B） */
#define CMD_4_3_PARAM2_MAX        0xFFu
#define CMD_4_3_PARAM3_MIN        0x00000000u /* 文件大小（4B，大端） */
#define CMD_4_3_PARAM3_MAX        0xFFFFFFFFu
#define CMD_4_3_PARAM4_READ                           0x00u  /* 读取，通知从FTP读取 */
#define CMD_4_3_PARAM4_WRITE                          0x11u  /* 写入，通知从FTP写入，如频谱监测数据，数据可能为多份文件需要临存 */
#define CMD_4_3_PARAM5_MIN        0x00u  /* 文件目录及名称（128B ASCII，不足补0，且不得全0），范围宏仅作占位 */
#define CMD_4_3_PARAM5_MAX        0xFFu

/* 4-4 FTP文件更新通知应答 */
#define CMD_4_4_PARAM1_OK                             0x00u  /* 正常 */
#define CMD_4_4_PARAM1_ERR                            0xFFu  /* 异常 */
#define CMD_4_4_PARAM2_MIN        0x00u
#define CMD_4_4_PARAM2_MAX        0xFFu  /* 32字节文件名（ASCII，不足补0），范围宏仅作占位 */

/* 4-6 FTP文件传输结果查询应答 */
#define CMD_4_6_PARAM1_IN_PROGRESS                    0x00u  /* 文件正在更新 */
#define CMD_4_6_PARAM1_DONE                           0x11u  /* 文件更新完成（正常） */
#define CMD_4_6_PARAM1_ERR                            0xFFu  /* 文件更新异常 */

/* 5-3 链路检测应答 */
#define CMD_5_3_PARAM1_MIN        0x00u
#define CMD_5_3_PARAM1_MAX        0xFFu  /* 若有扩展可用 */

/* 6-1 邻星及本星星历分发 */
#define CMD_6_1_PARAM1_MIN        0x00000000u  /* 本星星历时标（s） */
#define CMD_6_1_PARAM1_MAX        0xFFFFFFFFu
#define CMD_6_1_PARAM2_MIN        0x0000u      /* 星号 */
#define CMD_6_1_PARAM2_MAX        0xFFFFu
#define CMD_6_1_PARAM3_MIN        0x0000u
#define CMD_6_1_PARAM3_MAX        0xFFFFu
#define CMD_6_1_PARAM4_MIN        0x0000u
#define CMD_6_1_PARAM4_MAX        0xFFFFu
#define CMD_6_1_PARAM5_MIN        0x00000000u
#define CMD_6_1_PARAM5_MAX        0xFFFFFFFFu
#define CMD_6_1_PARAM6_MIN        0x00000000u
#define CMD_6_1_PARAM6_MAX        0xFFFFFFFFu
#define CMD_6_1_PARAM7_MIN        0x00000000u
#define CMD_6_1_PARAM7_MAX        0xFFFFFFFFu
#define CMD_6_1_PARAM8_MIN        0x00000000u
#define CMD_6_1_PARAM8_MAX        0xFFFFFFFFu
#define CMD_6_1_PARAM9_MIN        0x00000000u
#define CMD_6_1_PARAM9_MAX        0xFFFFFFFFu
#define CMD_6_1_PARAM10_MIN       0x00000000u
#define CMD_6_1_PARAM10_MAX       0xFFFFFFFFu
#define CMD_6_1_PARAM11_MIN       0x00000000u
#define CMD_6_1_PARAM11_MAX       0xFFFFFFFFu
#define CMD_6_1_PARAM12_MIN       0x00u
#define CMD_6_1_PARAM12_MAX       0xFFFFFFFFu

/* 6-2 本星星历分发 */
#define CMD_6_2_PARAM1_MIN        0x00u
#define CMD_6_2_PARAM1_MAX        0xFFFFFFFFu
#define CMD_6_2_PARAM2_MIN        0x00u
#define CMD_6_2_PARAM2_MAX        0xFFFFu
#define CMD_6_2_PARAM3_MIN        0x00u
#define CMD_6_2_PARAM3_MAX        0xFFFFu
#define CMD_6_2_PARAM4_VALID                       0x55u  /* 有效 */
#define CMD_6_2_PARAM4_INVALID                      0xAAu  /* 无效 */
#define CMD_6_2_PARAM5_MIN        0x00u
#define CMD_6_2_PARAM5_MAX        0xFFFFFFFFu
#define CMD_6_2_PARAM6_MIN        0x00u
#define CMD_6_2_PARAM6_MAX        0xFFFFFFFFu
#define CMD_6_2_PARAM7_MIN        0x00u
#define CMD_6_2_PARAM7_MAX        0xFFFFFFFFu
#define CMD_6_2_PARAM8_MIN        0x00u
#define CMD_6_2_PARAM8_MAX        0xFFFFFFFFu
#define CMD_6_2_PARAM9_MIN        0x00u
#define CMD_6_2_PARAM9_MAX        0xFFFFFFFFu
#define CMD_6_2_PARAM10_MIN       0x00u
#define CMD_6_2_PARAM10_MAX       0xFFFFFFFFu

#endif /* CMD_CODES_H */
