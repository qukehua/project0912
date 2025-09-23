#ifndef APP_REPORT_H
#define APP_REPORT_H

#include <stdint.h>
#include "udp_report.h"

#ifdef __cplusplus
extern "C"{
#endif

/* 在 app_transfer.c 中实现，用于注册回调 */
void app_transfer_register_cbs(udp_ack_cb ack_cb, udp_prog_cb prog_cb, void* user, const char* request_id);

#ifdef __cplusplus
}
#endif
#endif /* APP_REPORT_H */
