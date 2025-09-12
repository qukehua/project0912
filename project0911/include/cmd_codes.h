#ifndef CMD_CODES_H
#define CMD_CODES_H

/* 来自《测试表-ZZGLV0.8-20250423.xlsx》->“指令-以太网”sheet 的实际常量表
 * 仅列出当前 UDP 控制/更新流程会用到的指令码；其余可按需补充。
 */

/* ========== 基础与查询类 ========== */
#define CMD_CODE_SOFT_RESET                  0x001A  /* 复位 */
#define CMD_CODE_LINK_CHECK                  0x001D  /* 链路检测（心跳） */
#define CMD_CODE_LINK_CHECK_ACK              0x001F  /* 链路检测应答 */

#define CMD_CODE_FW_VERSION_QUERY            0x0101  /* 固件版本查询 */
#define CMD_CODE_FW_VERSION_RESP             0x0108  /* 固件版本查询应答 */

/* ========== 重构（非自主管理编排） ========== */
#define CMD_CODE_START_RECONSTRUCT           0x01AF  /* 启动固件重构 */
#define CMD_CODE_RECONSTRUCT_RESULT_QUERY    0x01C5  /* 重构结果查询 */
#define CMD_CODE_RECONSTRUCT_RESULT_RESP     0x01CA  /* 重构结果查询应答 */

/* ========== 文件直传（UDP 分片） ========== */
#define CMD_CODE_FILE_START                  0x0155  /* 文件传输开始 */
#define CMD_CODE_FILE_START_ACK              0x015A  /* 文件传输开始应答 */
#define CMD_CODE_FILE_END                    0x01AA  /* 文件传输结束 */
#define CMD_CODE_FILE_END_ACK                0x01BB  /* 文件传输结束应答 */

/* ========== 自主管理编排（准备/开始/查询） ========== */
#define CMD_CODE_PREPARE_RECON               0x3403  /* 自主重构组件准备 */
#define CMD_CODE_PREPARE_RECON_RESP          0x3405  /* 自主重构组件准备应答 */
#define CMD_CODE_START_RECON                 0x340A  /* 自主重构组件开始 */
#define CMD_CODE_SM_RECON_RESULT_QUERY       0x340E  /* 自主重构结果查询 */
#define CMD_CODE_SM_RECON_RESULT_RESP        0x340F  /* 自主重构结果查询应答 */

#define CMD_CODE_FACTORY_RESET               0x3506  /* 恢复出厂设置指令 */
#define CMD_CODE_FACTORY_RESET_ACK           0x3407  /* 恢复出厂设置指令应答 */
#define CMD_CODE_FACTORY_RESET_QUERY         0x3508  /* 恢复出厂设置结果查询 */
#define CMD_CODE_FACTORY_RESET_QUERY_ACK     0x3409  /* 恢复出厂设置结果查询应答 */

/* ========== FTP 控制（由 UDP 触发/查询） ========== */
#define CMD_CODE_FTP_NOTICE                  0x01D1  /* FTP文件更新通知 */
#define CMD_CODE_FTP_NOTICE_ACK              0x01BF  /* FTP文件更新通知应答 */
#define CMD_CODE_FTP_PROGRESS_QUERY          0x01DA  /* FTP文件传输结果查询 */
#define CMD_CODE_FTP_PROGRESS_RESP           0x01DB  /* FTP文件传输结果查询应答 */

/* ========== 通用应答字段（resp2 低8位状态建议位义） ==========
 * bit7 = 1 表示错误；bit1:0 状态：00=进行中，01=完成，11=错误（其余保留） */
#define RSP2_ERR_BIT                         0x80
#define RSP2_STATUS_MASK                     0x03
#define RSP2_STATUS_IN_PROGRESS              0x00
#define RSP2_STATUS_DONE                     0x01
#define RSP2_STATUS_ERROR                    0x03

#endif /* CMD_CODES_H */
