#ifndef UDP_REPORT_H
#define UDP_REPORT_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"{
#endif

typedef struct resp_frame_s resp_frame_t; /* 前置声明 */

typedef void (*udp_ack_cb)(int flow,       /* 0=控制类,1=UDP直传,2=UDP控制FTP */
                           uint16_t cmd_code,
                           const resp_frame_t* ack,
                           const char* request_id,
                           void* user);

typedef void (*udp_prog_cb)(int flow,      /* 1=UDP直传,2=FTP */
                            float progress,/* 0.0~1.0 */
                            uint32_t done_bytes,
                            uint32_t total_bytes,
                            const char* request_id,
                            void* user);

#ifdef __cplusplus
}
#endif
#endif /* UDP_REPORT_H */
