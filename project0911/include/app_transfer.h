#ifndef APP_TRANSFER_H
#define APP_TRANSFER_H

#include <stdint.h>
#include "protocol_tx.h"

#ifdef __cplusplus
extern "C"{
#endif

typedef enum {
    TR_OK = 0,
    TR_ERR_SOCKET,
    TR_ERR_PROTOCOL_START,
    TR_ERR_START_RETRY_EXCEEDED,
    TR_ERR_DATA_RETRY_EXCEEDED,
    TR_ERR_END_RETRY_EXCEEDED,
    TR_ERR_FILE_RESTART_EXCEEDED,
    TR_ERR_OPEN_FILE,
    TR_ERR_PROTOCOL_INTERNAL
} transfer_result_t;

/* 通过 UDP 直连进行文件传输（protocol_tx.c 实现） */
transfer_result_t transfer_file_udp(const char *remote_ip, uint16_t remote_port, uint16_t local_port,
                                    const file_meta_t *meta, const proto_cfg_t *cfg);

#ifdef __cplusplus
}
#endif
#endif /* APP_TRANSFER_H */
