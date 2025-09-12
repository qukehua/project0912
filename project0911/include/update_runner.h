#ifndef UPDATE_RUNNER_H
#define UPDATE_RUNNER_H

#include <stdint.h>
#include "udp_report.h"
#include "udp_cmd.h"
#include "app_transfer.h"
#include "udp_ctrl_ftp.h"
#include "cmd_frame.h"
#include "resp_frame.h"
#include "cmd_codes.h"

#ifdef __cplusplus
extern "C"{
#endif

int run_update_udp(const char* ip, uint16_t ctrl_port, uint16_t cmd_prepare, uint16_t cmd_start_reconst, uint16_t remote_port, uint16_t local_port, const file_meta_t* meta, const proto_cfg_t* cfg, udp_ack_cb cb, udp_prog_cb prog_cb, void* user, const char* request_id);
int run_update_ftp(const char* ip, uint16_t ctrl_port, uint16_t cmd_prepare, uint16_t cmd_start_reconst, const udp_ftp_cfg_t* ftp_cfg, udp_ack_cb cb, void* user, const char* request_id);

#ifdef __cplusplus
}
#endif
#endif /* UPDATE_RUNNER_H */
