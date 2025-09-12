#ifndef UDP_CTRL_FTP_H
#define UDP_CTRL_FTP_H

#include <stdint.h>
#include "udp_report.h"

#ifdef __cplusplus
extern "C"{
#endif

/* UDP 控 FTP 配置 */
typedef struct udp_ftp_cfg_s {
    /* 对端（被控组件）UDP 接口 */
    char     peer_ip[64];
    uint16_t peer_port;

    /* FTP 服务端信息（由被控侧去“拉取/写入”） */
    char     ftp_host[64];
    uint16_t ftp_port;     /* 默认 21 */
    char     user[32];
    char     pass[32];
    uint8_t  passive;      /* 1=PASV */

    /* 操作：1=PUT(写入: 从对端到FTP)，2=GET(读取: 从FTP到对端) */
    uint8_t  op;

    /* 路径（ASCII） */
    char     remote_path[256]; /* FTP 端完整路径（含目录+文件名） */
    char     local_path[256];  /* 对端本地保存路径（或期望文件名） */

    /* 时序参数 */
    int notice_timeout_ms;     /* 单次通知等待应答超时，默认 5000 */
    int notice_max_attempts;   /* 通知重试次数，默认 5 */
    int query_timeout_ms;      /* 单次查询等待应答超时，默认 5000 */
    int query_interval_ms;     /* 轮询间隔，默认 3000 */
} udp_ftp_cfg_t;

/* 回调注册（用于 ACK / 进度 / 完成/错误上报，带 requestId 透传） */
void udp_ctrl_ftp_register_cb(udp_ack_cb cb, void* user, const char* request_id);

/* 启动一次 FTP 控制流程（通知 -> 轮询查询直到完成/错误） */
int udp_ctrl_ftp_run(const udp_ftp_cfg_t* cfg);

#ifdef __cplusplus
}
#endif
#endif /* UDP_CTRL_FTP_H */
