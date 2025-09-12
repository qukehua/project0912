
#include "../include/update_runner.h"
#include "../include/app_report.h"

#include <string.h>
#include <stdio.h>

/* 构造一个不带参数的cmd帧 */
static void make_simple_cmd(uint16_t code, cmd_frame_t* c){
    memset(c, 0, sizeof(*c));
    c->type = 0x00;
    c->seq  = 0x00;
    c->code = code;
    c->params = NULL;
    c->param_len = 0;
}

int run_update_udp(const char* ip, uint16_t ctrl_port, uint16_t cmd_prepare, uint16_t cmd_start_reconst, uint16_t remote_port, uint16_t local_port, const file_meta_t* meta, const proto_cfg_t* cfg, udp_ack_cb cb, udp_prog_cb prog_cb, void* user, const char* request_id)
{
    /* 1) 准备重构 */
    cmd_frame_t c1; resp_frame_t r1;
    make_simple_cmd(cmd_prepare, &c1);
    int rc = udp_send_cmd_wait_ack(ip, ctrl_port, &c1, &r1, 5000, 3, cb, user, 0, request_id);
    if(rc!=0) return -10;

    /* 2) 文件分片直传（内部状态机/参数保持不变） */
    app_transfer_register_cbs(cb, prog_cb, user, request_id); /* 上报ACK和进度信息 */
    transfer_result_t tr = transfer_file_udp(ip, remote_port, local_port, meta, cfg);
    if(tr != TR_OK) return -20;

    /* 3) 开始重构 */
    cmd_frame_t c3; resp_frame_t r3;
    make_simple_cmd(cmd_start_reconst, &c3);
    rc = udp_send_cmd_wait_ack(ip, ctrl_port, &c3, &r3, 5000, 3, cb, user, 0, request_id);
    if(rc!=0) return -30;

    return 0;
}

int run_update_ftp(const char* ip, uint16_t ctrl_port, uint16_t cmd_prepare, uint16_t cmd_start_reconst, const udp_ftp_cfg_t* ftp_cfg, udp_ack_cb cb, void* user, const char* request_id)
{
    /* 1) 准备重构 */
    cmd_frame_t c1; resp_frame_t r1;
    make_simple_cmd(cmd_prepare, &c1);
    int rc = udp_send_cmd_wait_ack(ip, ctrl_port, &c1, &r1, 5000, 3, cb, user, 0, request_id);
    if(rc!=0) return -10;

    /* 2) UDP控制FTP：注册回调并执行 */
    extern void udp_ctrl_ftp_register_cb(udp_ack_cb cb, void* user, const char* request_id);
    udp_ctrl_ftp_register_cb(cb, user, request_id);
    rc = udp_ctrl_ftp_run(ftp_cfg);
    if(rc!=0) return -20;

    /* 3) 开始重构 */
    cmd_frame_t c3; resp_frame_t r3;
    make_simple_cmd(cmd_start_reconst, &c3);
    rc = udp_send_cmd_wait_ack(ip, ctrl_port, &c3, &r3, 5000, 3, cb, user, 0, request_id);
    if(rc!=0) return -30;

    return 0;
}
