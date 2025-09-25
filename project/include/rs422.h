#ifndef __RS422_H__
#define __RS422_H__

#include <stdint.h>
#include <stdbool.h>

/* 常量定义 */
#define RS422_PORT_DEFAULT        "/dev/ttyRS422-0" /* 默认端口名 - Linux RS422端口0 */
#define RS422_PORT_0              "/dev/ttyRS422-0" /* RS422端口0 */
#define RS422_PORT_1              "/dev/ttyRS422-1" /* RS422端口1 */
#define RS422_PORT_2              "/dev/ttyRS422-2" /* RS422端口2 */
#define RS422_PORT_3              "/dev/ttyRS422-3" /* RS422端口3 */
#define RS422_PORT_4              "/dev/ttyRS422-4" /* RS422端口4 */
#define RS422_BAUD_RATE_DEFAULT   115200          /* 默认波特率 */
#define RS422_QUEUE_CAPACITY      64              /* 任务队列容量 */
#define RS422_SEND_BUFFER_SIZE    1024            /* 发送缓冲区大小 */
#define RS422_RECEIVE_BUFFER_SIZE 1024            /* 接收缓冲区大小 */

/* 消息类型定义（与eth1_task.h保持一致） */
typedef enum {
    RS422_MSG_RECEIVED = 1,  /* 接收到数据 */
    RS422_MSG_STATUS = 2,    /* 状态更新 */
    RS422_MSG_ERROR = 3,     /* 错误消息 */
    RS422_MSG_PROGRESS = 4,  /* 进度更新 */
    RS422_MSG_DONE = 5,      /* 任务完成 */
    RS422_MSG_ACK = 6        /* 确认消息 */
} Rs422MsgType;

/* 接收数据消息 */
typedef struct {
    uint32_t task_id;        /* 任务ID */
    uint8_t *data;           /* 数据指针 */
    uint16_t data_len;       /* 数据长度 */
    int status;              /* 状态码 */
} Rs422DataMsg;

/* 状态消息 */
typedef struct {
    uint32_t task_id;        /* 任务ID */
    int status_code;         /* 状态码 */
    char status_msg[256];    /* 状态消息 */
} Rs422StatusMsg;

/* 错误消息 */
typedef struct {
    uint32_t task_id;        /* 任务ID */
    int error_code;          /* 错误码 */
    char error_msg[256];     /* 错误描述 */
} Rs422ErrorMsg;

/* 进度消息 */
typedef struct {
    uint32_t task_id;        /* 任务ID */
    uint32_t progress;       /* 进度值(0-100) */
    uint32_t total;          /* 总工作量 */
} Rs422ProgressMsg;

/* 完成消息 */
typedef struct {
    uint32_t task_id;        /* 任务ID */
    int status;              /* 状态码 */
} Rs422DoneMsg;

/* 确认消息 */
typedef struct {
    uint32_t task_id;        /* 任务ID */
    int status;              /* 状态码 */
} Rs422AckMsg;

/* 统一消息结构 */
typedef struct {
    Rs422MsgType type;       /* 消息类型 */
    union {
        Rs422DataMsg data;     /* 数据消息 */
        Rs422StatusMsg status; /* 状态消息 */
        Rs422ErrorMsg error;   /* 错误消息 */
        Rs422ProgressMsg progress; /* 进度消息 */
        Rs422DoneMsg done;     /* 完成消息 */
        Rs422AckMsg ack;       /* 确认消息 */
    } u;
} Rs422Msg;

/* ===================== 任务类型 ===================== */
typedef enum {
    RS422_TASK_SEND_DATA = 0,          /* 发送数据任务 */
    RS422_TASK_CONFIG_PORT,            /* 配置端口任务 */
    RS422_TASK_RECEIVE_DATA            /* 接收数据任务 */
} Rs422TaskType;

/* 端口配置结构 */
typedef struct {
    char port_name[64];      /* 端口名称 */
    uint32_t baud_rate;      /* 波特率 */
    uint8_t data_bits;       /* 数据位(5-8) */
    uint8_t stop_bits;       /* 停止位(1-2) */
    uint8_t parity;          /* 校验位(0=无,1=奇,2=偶) */
} Rs422PortConfig;

/* 发送数据任务参数 */
typedef struct {
    const uint8_t *data;     /* 数据指针 */
    uint16_t data_len;       /* 数据长度 */
    uint32_t timeout_ms;     /* 超时时间(毫秒) */
} Rs422SendParams;

/* 接收数据任务参数 */
typedef struct {
    uint32_t timeout_ms;     /* 超时时间(毫秒) */
    uint16_t buffer_size;    /* 缓冲区大小 */
} Rs422ReceiveParams;

/* 统一任务结构 */
typedef struct {
    uint32_t task_id;        /* 任务ID */
    Rs422TaskType type;      /* 任务类型 */
    union {
        Rs422SendParams send;     /* 发送参数 */
        Rs422PortConfig config;   /* 配置参数 */
        Rs422ReceiveParams receive; /* 接收参数 */
    } u;
} Rs422Task;

/* 回调函数类型定义 */
typedef void (*Rs422Callback)(void *user, const Rs422Msg *msg);

/* 公共接口函数声明 */

/**
 * @brief 初始化RS422模块
 * @param sink 消息回调函数
 * @param user 用户数据
 * @return 0-成功，其他-失败
 */
int rs422_init(Rs422MsgSink sink, void *user);

/**
 * @brief 关闭RS422模块
 */
void rs422_shutdown(void);

/**
 * @brief 发送数据
 * @param data 数据指针
 * @param data_len 数据长度
 * @param timeout_ms 超时时间(毫秒)
 * @return 发送的字节数，负数表示错误
 */
int rs422_send_data(const uint8_t *data, uint16_t data_len, uint32_t timeout_ms);

/**
 * @brief 配置RS422端口参数
 * @param config 配置参数指针
 * @return 0-成功，其他-失败
 */
int rs422_configure_port(const Rs422PortConfig *config);

/**
 * @brief 接收数据（阻塞）
 * @param buffer 接收缓冲区
 * @param buffer_size 缓冲区大小
 * @param timeout_ms 超时时间(毫秒)
 * @return 接收的字节数，负数表示错误
 */
int rs422_receive_data(uint8_t *buffer, uint16_t buffer_size, uint32_t timeout_ms);

/**
 * @brief 入队任务到RS422服务线程
 * @param task 任务指针
 * @return 0-成功，其他-失败
 */
int rs422_enqueue_task(const Rs422Task *task);

/**
 * @brief 获取任务队列当前大小
 * @return 队列中的任务数
 */
int rs422_queue_size(void);

/**
 * @brief 获取RS422模块初始化状态
 * @return true-已初始化，false-未初始化
 */
bool rs422_is_initialized(void);

#endif /* __RS422_H__ */