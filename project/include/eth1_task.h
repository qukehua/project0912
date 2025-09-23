#pragma once
/*
 * ETH1 任务/消息定义（阶段A）
 * - 任务由上层（ETH0/main）塞入 ETH1 的任务队列
 * - ETH1 线程处理后，按应答/进度/完成把消息回传给上层
 * - 本文件只定义数据结构与默认口径，不涉及具体业务实现
 */

#include <stdint.h>
#include <stddef.h>

/* ===================== 任务类型（你确定的6类） ===================== */
typedef enum {
    ETH1_TASK_SWVER_QUERY = 0,          /* 软件版本查询 */
    ETH1_TASK_FACTORY_RESET_NOTIFY,     /* 恢复出厂设置通知 */
    ETH1_TASK_FACTORY_RESET_QUERY,      /* 恢复出厂设置结果查询 */
    ETH1_TASK_ROLLBACK_SET,             /* 软件回退设置 */
    ETH1_TASK_ROLLBACK_QUERY,           /* 软件回退结果查询 */
    ETH1_TASK_RECONFIG_EXECUTE,         /* 重构编排：准备→文件传输→开始 */
} eth1_task_type_t;

/* ===================== 文件传输方式（两种都支持） ===================== */
typedef enum {
    FT_UDP_CTRL_FTP = 0,                /* UDP 控制 FTP */
    FT_UDP_SEGMENTED = 1,               /* UDP 分片直传 */
} transfer_method_t;

/* ===================== 统一超时/重试口径（可覆盖） ===================== */
typedef struct {
    uint32_t t_cmd_rsp_ms;      /* 指令应答等待：默认 5000ms */
    uint8_t  max_retries_cmd;   /* 指令重试：默认 3 次      */
    uint32_t t_frame_ack_ms;    /* 单帧ACK等待：默认 5000ms */
    uint8_t  max_retries_frame; /* 单帧重传：默认 3 次      */
    uint8_t  max_retries_file;  /* 整文件重传：默认 2 次    */
} CtrlPolicy;

#define CTRL_POLICY_DEFAULT() ((CtrlPolicy){5000u,3u,5000u,3u,2u})

/* ===================== 目标设备/APID 信息（由上层给） ===================== */
typedef struct {
    uint8_t  ruid;              /* APID 高7位（设备识别符），上层直接给 */
    uint16_t apid_low_ctrl;     /* 控制指令低4位：通常为0x0，可留0表示默认 */
    uint16_t apid_low_data;     /* 文件/数据低4位：按规范附录A，如未知可留0表示由映射表给出 */
} EthTarget;

/* ===================== 文件传输规格（由上层给） ===================== */
typedef struct {
    transfer_method_t method;       /* 使用哪种传输方法 */
    const char*       file_path;    /* 待传文件路径（本地路径） */

    /* UDP 控制 FTP 所需（保持精简；详细字段可在阶段B扩展） */
    struct {
        const char* url;            /* 对端从此URL拉取，如 ftp://10.0.0.1/path/file */
        const char* user;           /* 账号 */
        const char* pass;           /* 密码 */
        int  passive;               /* 1=被动模式，0=主动 */
        int  use_tls;               /* 1=FTPS，0=FTP */
        uint8_t file_type;          /* 阶段B新增：文件类型(1B)，如 0xFF/0xFE */
        uint8_t sub_type;           /* 阶段B新增：子类型(1B) */
        uint8_t op_type;            /* 阶段B新增：操作类型(1B)，0x00=读取，0x11=写入 */
    } ftp;

    /* UDP 分片直传所需（阶段B按 data_frame.* 与图5 实现） */
    struct {
        uint32_t slice_size;        /* 分片大小，字节，建议 ≤1000 */
        uint32_t throttle_pps;      /* 发送节流（包/秒），0=不限 */
        int      enable_crc;        /* 是否对整文件做 CRC16-CCITT-FALSE（Start参数需带） */
        uint8_t  file_type;         /* 阶段B新增：文件类型(1B)，如 0xFF/0xFE */
        uint8_t  sub_type;          /* 阶段B新增：子类型(1B) */
    } seg;
} FileTransferSpec;

/* ===================== 重构编排计划（由上层给） ===================== */
typedef struct {
    EthTarget        target;        /* 目标设备/APID 信息 */
    CtrlPolicy       policy;        /* 超时/重试口径 */
    FileTransferSpec xfer;          /* 文件传输规格 */

    /* 指令码（准备/开始）。准备需等待ACK；开始不等待ACK */
    uint16_t         cmd_prepare_req;
    uint16_t         cmd_prepare_ack;
    uint16_t         cmd_start_req; /* 开始指令，无ACK */
} ReconfigPlan;

/* ===================== 入队任务结构 ===================== */
typedef struct {
    uint32_t            task_id;    /* 由上层分配的任务ID，用于回传对齐 */
    eth1_task_type_t    type;       /* 任务类型 */

    EthTarget           target;
    CtrlPolicy          policy;

    /* 任务载荷：不同任务使用不同字段 */
    union {
        /* 控制类任务一般无额外参数；如有，可在阶段B扩展 */
        struct { int reserved; } ctrl;

        /* 重构任务：包含完整编排计划 */
        ReconfigPlan reconfig;
    } u;
} Eth1Task;

/* ===================== 回传消息（ETH1 → main/ETH0） ===================== */
typedef enum {
    ETH1_MSG_ACK = 0,       /* 收到对端应答（原始参数字节一并带回） */
    ETH1_MSG_PROGRESS,      /* 进度/阶段更新（文件传输等） */
    ETH1_MSG_DONE,          /* 任务结束（成功/失败） */
    ETH1_MSG_LOG            /* 简单日志/提示 */
} eth1_msg_type_t;

/* 原始字节（注：内部会解析以驱动状态机，但对外仍回原始参数区字节） */
typedef struct { uint8_t buf[1024]; uint16_t len; } eth1_raw_resp_t;

/* 各类消息体 */
typedef struct {
    uint32_t task_id;
    uint8_t  ruid;
    uint16_t opcode;            /* 应答对应的指令码 */
    eth1_raw_resp_t payload;    /* 原始参数字节 */
} Eth1MsgAck;

typedef struct {
    uint32_t task_id;
    uint8_t  ruid;
    int      pct;               /* 0~100 */
    int      stage;             /* 自定义阶段编号（阶段B具体定义） */
    char     note[96];          /* 简短说明 */
} Eth1MsgProgress;

typedef struct {
    uint32_t task_id;
    uint8_t  ruid;
    int      status;            /* 0=成功；<0=失败（errno风格） */
    char     note[96];          /* 失败原因/最终说明 */
    eth1_raw_resp_t last_resp;  /* 可选：最后一次应答原始字节 */
} Eth1MsgDone;

typedef struct {
    uint32_t task_id;
    char     line[120];
} Eth1MsgLog;

/* 统一消息封装 */
typedef struct {
    eth1_msg_type_t type;
    union {
        Eth1MsgAck      ack;
        Eth1MsgProgress progress;
        Eth1MsgDone     done;
        Eth1MsgLog      log;
    } u;
} Eth1Msg;

/* ETH1 → main 回传函数类型（由 main 注册） */
typedef void (*Eth1MsgSink)(void* user, const Eth1Msg* msg);

