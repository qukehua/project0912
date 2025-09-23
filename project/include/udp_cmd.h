#ifndef UDP_CMD_H
#define UDP_CMD_H

#include <stdint.h>
#include <stddef.h>
#include "udp_report.h"
#include "cmd_frame.h"
#include "resp_frame.h"

#ifdef __cplusplus
extern "C"{
#endif

/* 发送一条指令并等待应答（超时重试），并可选上报回调 */
int udp_send_cmd_wait_ack(const char* ip, uint16_t port,
                          const cmd_frame_t* req, resp_frame_t* ack_out,
                          int timeout_ms,     /* 默认5000 */
                          int max_retry,      /* 默认3 */
                          udp_ack_cb cb, void* cb_user,
                          int flow_tag,       /* 0=控制类,1=UDP直传,2=FTP */
                          const char* request_id);

#ifdef __cplusplus
}
#endif
#endif /* UDP_CMD_H */
