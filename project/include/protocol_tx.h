#ifndef PROTOCOL_TX_H
#define PROTOCOL_TX_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"{
#endif

typedef struct file_meta_s {
    const char* filepath;     /* 本地文件路径 */
    uint16_t    apid;         /* 低11位有效 */
    uint8_t     device_id7;   /* 目标设备标识（7bit） */
    uint8_t     file_type;    /* 4bit */
    uint8_t     version;      /* 4bit */
    uint8_t     storage_area; /* 存储区域/盘符 */
    uint32_t    file_size;    /* 字节数（可预先给出；0时由实现读取文件获取） */
    uint16_t    file_crc16;   /* 0时由实现自动计算（CRC16-CCITT-FALSE） */
} file_meta_t;

typedef struct proto_cfg_s {
    uint32_t cmd_timeout_ms;        /* 默认 5000 */
    uint32_t data_timeout_ms;       /* 默认 5000 */
    uint8_t  cmd_max_retry;         /* 默认 3 */
    uint8_t  data_max_retry;        /* 默认 3 */
    uint8_t  file_max_restart;      /* 默认 2 */
    uint16_t data_chunk_bytes;      /* 默认 1200, 最大 1400 */
    /* 可按设备选择“开始传输”的等待超时 */
    uint32_t (*select_start_timeout_ms)(uint8_t device_id7);
    /* 进度回调函数 */
    void (*progress_cb)(float progress, uint32_t done, uint32_t total);
} proto_cfg_t;

typedef enum {
    PERR_NONE = 0,
    PERR_CMD_START_RETRY_EXCEEDED,
    PERR_DATA_RETRY_EXCEEDED,
    PERR_CMD_END_RETRY_EXCEEDED,
    PERR_FILE_RESTART_EXCEEDED,
    PERR_IO_OPEN_FAIL,
    PERR_IO_READ_FAIL,
    PERR_PROTOCOL_START,
    PERR_INTERNAL_BUG,
    PERR_PROTOCOL_INTERNAL
} proto_err_t;

void proto_init(int sock_fd, const proto_cfg_t *cfg);
int  proto_start_file(const file_meta_t *meta);
void proto_on_rx(const uint8_t *buf, size_t len);
void proto_tick(void);
float proto_progress(void);
int   proto_is_finished(void);
int   proto_is_error(void);
proto_err_t proto_last_error(void);

#ifdef __cplusplus
}
#endif
#endif /* PROTOCOL_TX_H */
