#pragma once
/*
 * udp_sm.h —— UDP 三类流程的状态机
 * 依赖：cmd_frame.h, data_frame.h, APID.h, cmd_codes.h(数值以测试表为准)
 *
 * 你确认的关键口径（已落实到实现）：
 *  - 应答帧 APID 低4位恒为 0x7（ACK）
 *  - 控制/应答均为单帧；数据帧“数据”<=1000B、段号从0开始、每个分片都回 0x018A
 *  - 0x018A 逐包应答：仅 1 字节“状态”
 *      * 0x00 = 接收正常
 *      * 0xFF = 接收异常
 *      * 其它值未定义 → 按异常处理
 *    无段号字段，应答默认对应“当前等待确认”的分片（不得并发/流水发送）
 *  - 超时/重试默认：
 *      * 指令应答超时 5s，重发 3 次
 *      * 单帧数据重发 ≤3 次，等待应答 5s
 *      * 整个文件“从头重发” ≤2 次
 *      * FTP 进度查询周期 ≥3s，查询尝试 >5 次失败
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ===== 常量（按你的口径） ===== */
#define SM_CMD_TIMEOUT_MS           5000u
#define SM_CMD_RETRY_MAX            3u

#define SM_DATA_ACK_TIMEOUT_MS      5000u
#define SM_DATA_RETRY_MAX           3u
#define SM_FILE_RETRY_MAX           2u

#define SM_FTP_QUERY_PERIOD_MS      3000u
#define SM_FTP_QUERY_TRY_MAX        5u

/* 应答帧数据类型恒定 */
#define NIXYK_DTYPE_REQ             0x0u   /* 遥控指令 */
#define NIXYK_DTYPE_ACK             0x7u   /* 应答帧 */
#define NIXYK_DTYPE_FILE            0xFu   /* 文件型传输 */

/* ========== 控制类（7.2.1） ========== */
typedef int  (*udp_send_fn)(const uint8_t* buf, size_t len, void* user); /* 0=ok, <0错误 */
typedef void (*sm_log_fn)(const char* msg, void* user);

typedef struct {
    /* 固定上下文 */
    uint8_t   dev7;
    uint16_t  next_seq14;   /* 初值0，按帧发送自增(14bit) */
    udp_send_fn send; void* send_user;
    sm_log_fn  log;  void* log_user;

    /* 本次任务参数 */
    uint16_t req_code;                /* 请求指令码 */
    uint16_t expect_ack_code;         /* 期望的应答指令码 */
    const uint8_t* req_params; uint16_t req_param_len;

    /* 运行态 */
    uint8_t  retries;                 /* 已重发次数 */
    uint64_t deadline_ms;             /* 5s 超时截止 */
    bool     busy;
} sm_ctrl_t;

/* 对外接口 */
void sm_ctrl_init(sm_ctrl_t* s, uint8_t dev7, udp_send_fn send, void* send_user, sm_log_fn log, void* log_user);
int  sm_ctrl_start(sm_ctrl_t* s, uint16_t req_code, uint16_t expect_ack_code,
                   const uint8_t* params, uint16_t param_len, uint64_t now_ms);
/* 喂入收到的 UDP 字节（帧层应答在此解析、匹配） */
int  sm_ctrl_on_udp(sm_ctrl_t* s, const uint8_t* buf, size_t len, uint64_t now_ms);
/* 周期调用（例如主循环里），处理超时重发 */
int  sm_ctrl_poll(sm_ctrl_t* s, uint64_t now_ms);
/* 是否空闲 */
static inline bool sm_ctrl_idle(const sm_ctrl_t* s){ return !s->busy; }

/* ========== 文件类（7.2.2，含重构的数据通道） ========== */
typedef struct {
    /* 固定上下文 */
    uint8_t   dev7;
    uint16_t  next_seq14;           /* 14bit，自增 */
    udp_send_fn send; void* send_user;
    sm_log_fn  log;  void* log_user;

    /* 控制指令码（以测试表为准；必要时在 cmd_codes.h 调整） */
    uint16_t code_file_start;       /* 0x0155 */
    uint16_t code_file_start_ack;   /* 0x015A */
    uint16_t code_file_udp_ack;     /* 0x018A —— 分片应答（仅1字节状态） */
    uint16_t code_file_end;         /* 0x01AA */
    uint16_t code_file_end_ack;     /* 0x01BB */

    /* 本次任务输入（由上层按测试表构造） */
    const uint8_t* start_params; uint16_t start_param_len;  /* FILE_START 参数 */
    const uint8_t* end_params;   uint16_t end_param_len;    /* FILE_END   参数 */
    const uint8_t* data;         size_t   data_len;         /* 整体文件数据 */

    /* 分片进度（不得并发/流水发送） */
    uint16_t seg_no;              /* 当前段号（从0开始） */
    size_t   off;                 /* 数据偏移 */
    uint8_t  chunk_retry;         /* 该分片重试计数（<=3） */
    uint8_t  file_retry;          /* 整个文件从头重发计数（<=2） */

    /* 状态控制 */
    enum { FS_IDLE, FS_WAIT_START_ACK, FS_SEND_CHUNK, FS_WAIT_CHUNK_ACK,
           FS_WAIT_END_ACK, FS_DONE, FS_FAIL } st;
    uint8_t  cmd_retry;           /* START/END 的“指令重试计数”（<=3） */
    uint64_t deadline_ms;         /* 当前等待的 5s 截止 */
} sm_file_t;

void sm_file_init(sm_file_t* f, uint8_t dev7, udp_send_fn send, void* send_user,
                  sm_log_fn log, void* log_user);

/* 启动一次文件发送流程（start/end 参数、文件数据均由上层按测试表准备好） */
int  sm_file_start(sm_file_t* f,
                   uint16_t code_file_start, uint16_t code_file_start_ack,
                   uint16_t code_file_udp_ack,
                   uint16_t code_file_end,   uint16_t code_file_end_ack,
                   const uint8_t* start_params, uint16_t start_param_len,
                   const uint8_t* end_params,   uint16_t end_param_len,
                   const uint8_t* data, size_t data_len,
                   uint64_t now_ms);

/* 收到任一 UDP 帧喂入；内部只关心应答类指令帧（0x7） */
int  sm_file_on_udp(sm_file_t* f, const uint8_t* buf, size_t len, uint64_t now_ms);
/* 周期调用；处理超时与重发（分片/整文件） */
int  sm_file_poll(sm_file_t* f, uint64_t now_ms);
/* 是否完成 / 失败 / 空闲 */
static inline bool sm_file_done (const sm_file_t* f){ return f->st == FS_DONE; }
static inline bool sm_file_fail (const sm_file_t* f){ return f->st == FS_FAIL; }
static inline bool sm_file_idle (const sm_file_t* f){ return f->st == FS_IDLE; }

/* ========== UDP 控制下的 FTP 传输（7.2.3） ========== */
typedef struct {
    uint8_t   dev7;
    uint16_t  next_seq14;
    udp_send_fn send; void* send_user;
    sm_log_fn  log;  void* log_user;

    /* 指令码（以测试表为准） */
    uint16_t code_notice;       /* 0x01D1 通知 */
    uint16_t code_notice_ack;   /* 0x01BF 通知应答 */
    uint16_t code_prog_query;   /* 0x01DA 进度查询 */
    uint16_t code_prog_resp;    /* 0x01DB 进度应答 */

    /* 本次任务输入 */
    const uint8_t* notice_params; uint16_t notice_param_len;

    /* 状态 */
    enum { FTP_IDLE, FTP_WAIT_NOTICE_ACK, FTP_QUERYING, FTP_DONE, FTP_FAIL } st;
    uint8_t  notice_retry;    /* 3 次 */
    uint8_t  query_tries;     /* <=5 次 */
    uint64_t deadline_ms;     /* 等 ACK 的 5s */
    uint64_t next_query_ms;   /* 下次查询时间（>=3s） */
} sm_ftp_t;

void sm_ftp_init(sm_ftp_t* s, uint8_t dev7, udp_send_fn send, void* send_user, sm_log_fn log, void* log_user);
int  sm_ftp_start(sm_ftp_t* s,
                  uint16_t code_notice,     uint16_t code_notice_ack,
                  uint16_t code_prog_query, uint16_t code_prog_resp,
                  const uint8_t* notice_params, uint16_t notice_param_len,
                  uint64_t now_ms);
int  sm_ftp_on_udp(sm_ftp_t* s, const uint8_t* buf, size_t len, uint64_t now_ms);
int  sm_ftp_poll(sm_ftp_t* s, uint64_t now_ms);
static inline bool sm_ftp_idle(const sm_ftp_t* s){ return s->st == FTP_IDLE; }

#ifdef __cplusplus
}
#endif
