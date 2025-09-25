#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <pthread.h>
#include <sys/ioctl.h>

#include "rs422.h"

/* 帧头帧尾定义（与数据帧规范保持一致）*/
#define NIXYK_FRAME_HEAD_BYTE1 ((NIXYK_FRAME_IDENTIFIER >> 8) & 0xFF)
#define NIXYK_FRAME_HEAD_BYTE2 (NIXYK_FRAME_IDENTIFIER & 0xFF)
#define NIXYK_FRAME_TAIL_BYTE1 0x0D
#define NIXYK_FRAME_TAIL_BYTE2 0x0A

/* ===================== 内部状态和队列实现 ===================== */
static int g_inited = 0;
static int g_shutdown = 0;
static pthread_t g_thr;
static pthread_t g_recv_thr;
static pthread_mutex_t g_mutex;
static pthread_cond_t g_cond;

/* 回传回调 */
static Rs422Callback g_callback = NULL;
static void *g_callback_userdata = NULL;

/* 端口句柄 */
static int g_port_handle = -1;

/* 端口配置 */
static Rs422PortConfig g_current_config = {
    RS422_PORT_DEFAULT,
    RS422_BAUD_RATE_DEFAULT,
    8, /* 数据位 */
    1, /* 停止位 */
    0  /* 无校验 */
};

/* 环形任务队列 */
typedef struct
{
    Rs422Task items[RS422_QUEUE_CAPACITY];
    int head;  /* 读指针 */
    int tail;  /* 写指针 */
    int count; /* 当前元素数 */
    pthread_mutex_t mtx;
    pthread_cond_t cv_put;
    pthread_cond_t cv_get;
} TaskQueue;

static TaskQueue g_q;

/* ===================== 内部函数声明 ===================== */
static int q_push(const Rs422Task *t);
static int q_pop(Rs422Task *t);
static int open_port(const char *port_name);
static int close_port(void);
static int configure_port(const Rs422PortConfig *config);
static int send_data(const uint8_t *data, uint16_t data_len, uint32_t timeout_ms);
static int receive_data(uint8_t *buffer, uint16_t buffer_len, uint32_t timeout_ms);
static void emit_msg(const Rs422Msg *msg);
static void emit_status(uint32_t task_id, int status_code, const char *status_msg);
static void emit_error(uint32_t task_id, int error_code, const char *error_msg);
static void emit_received_data(uint32_t task_id, const uint8_t *data, uint16_t data_len);

/* ===================== 队列操作函数 ===================== */
static int q_push(const Rs422Task *t)
{
    pthread_mutex_lock(&g_q.mtx);
    if (g_q.count == RS422_QUEUE_CAPACITY)
    {
        pthread_mutex_unlock(&g_q.mtx);
        return -1; /* 队列已满 */
    }
    g_q.items[g_q.tail] = *t; /* 结构体整体拷贝 */
    g_q.tail = (g_q.tail + 1) % RS422_QUEUE_CAPACITY;
    g_q.count++;
    pthread_cond_signal(&g_q.cv_get);
    pthread_mutex_unlock(&g_q.mtx);
    return 0;
}

static int q_pop(Rs422Task *t)
{
    pthread_mutex_lock(&g_q.mtx);
    while (g_q.count == 0 && !g_shutdown)
    {
        pthread_cond_wait(&g_q.cv_get, &g_q.mtx);
    }
    if (g_shutdown && g_q.count == 0)
    {
        pthread_mutex_unlock(&g_q.mtx);
        return -1;
    }
    *t = g_q.items[g_q.head];
    g_q.head = (g_q.head + 1) % RS422_QUEUE_CAPACITY;
    g_q.count--;
    pthread_mutex_unlock(&g_q.mtx);
    return 0;
}

/* ===================== 端口操作函数 ===================== */
/* Linux/Unix 平台的实现 */
static int open_port(const char *port_name)
{
    g_port_handle = open(port_name, O_RDWR | O_NOCTTY | O_NDELAY);
    if (g_port_handle < 0)
    {
        emit_error(0, errno, "Failed to open RS422 port");
        return -1;
    }

    /* 设置为阻塞模式 */
    fcntl(g_port_handle, F_SETFL, 0);

    return 0;
}

static int close_port()
{
    if (g_port_handle >= 0)
    {
        close(g_port_handle);
        g_port_handle = -1;
    }
    return 0;
}

static int configure_port(const Rs422PortConfig *config)
{
    if (g_port_handle < 0)
    {
        emit_error(0, -1, "Port not open");
        return -1;
    }

    struct termios options;
    if (tcgetattr(g_port_handle, &options) < 0)
    {
        emit_error(0, errno, "Failed to get port attributes");
        return -1;
    }

    /* 设置波特率 */
    cfsetispeed(&options, config->baud_rate);
    cfsetospeed(&options, config->baud_rate);

    /* 设置数据位、停止位和校验位 */
    options.c_cflag &= ~CSIZE;
    switch (config->data_bits)
    {
    case 5:
        options.c_cflag |= CS5;
        break;
    case 6:
        options.c_cflag |= CS6;
        break;
    case 7:
        options.c_cflag |= CS7;
        break;
    case 8:
    default:
        options.c_cflag |= CS8;
        break;
    }

    /* 设置停止位 */
    if (config->stop_bits == 2)
    {
        options.c_cflag |= CSTOPB;
    }
    else
    {
        options.c_cflag &= ~CSTOPB;
    }

    /* 设置校验位 */
    switch (config->parity)
    {
    case 1: /* 奇校验 */
        options.c_cflag |= (PARODD | PARENB);
        break;
    case 2: /* 偶校验 */
        options.c_cflag |= PARENB;
        options.c_cflag &= ~PARODD;
        break;
    case 0: /* 无校验 */
    default:
        options.c_cflag &= ~PARENB;
        break;
    }

    /* 设置为原始模式 */
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    options.c_iflag &= ~(IXON | IXOFF | IXANY);
    options.c_oflag &= ~OPOST;

    /* 设置超时 */
    options.c_cc[VMIN] = 0;
    options.c_cc[VTIME] = 10; /* 1秒超时 */

    /* 应用配置 */
    if (tcsetattr(g_port_handle, TCSANOW, &options) < 0)
    {
        emit_error(0, errno, "Failed to set port attributes");
        return -1;
    }

    /* 保存当前配置 */
    memcpy(&g_current_config, config, sizeof(Rs422PortConfig));

    return 0;
}

static int send_data(const uint8_t *data, uint16_t data_len, uint32_t timeout_ms)
{
    if (g_port_handle < 0)
    {
        emit_error(0, -1, "Port not open");
        return -1;
    }

    /* 设置超时 */
    struct termios options;
    if (tcgetattr(g_port_handle, &options) < 0)
    {
        emit_error(0, errno, "Failed to get port attributes");
        return -1;
    }

    /* 保存原有超时设置 */
    speed_t old_ispeed = cfgetispeed(&options);
    speed_t old_ospeed = cfgetospeed(&options);

    /* 应用新的超时设置 */
    options.c_cc[VTIME] = timeout_ms / 100; /* 转换为1/10秒 */

    if (tcsetattr(g_port_handle, TCSANOW, &options) < 0)
    {
        emit_error(0, errno, "Failed to set port timeout");
        return -1;
    }

    /* 发送数据 */
    int bytes_written = write(g_port_handle, data, data_len);

    /* 恢复原有设置 */
    options.c_cc[VTIME] = 10; /* 恢复为1秒 */
    tcsetattr(g_port_handle, TCSANOW, &options);

    if (bytes_written < 0)
    {
        emit_error(0, errno, "Failed to write to port");
        return -1;
    }

    return bytes_written;
}

static int receive_data(uint8_t *buffer, uint16_t buffer_len, uint32_t timeout_ms)
{
    if (g_port_handle < 0)
    {
        emit_error(0, -1, "Port not open");
        return -1;
    }

    /* 设置超时 */
    struct termios options;
    if (tcgetattr(g_port_handle, &options) < 0)
    {
        emit_error(0, errno, "Failed to get port attributes");
        return -1;
    }

    /* 保存原有超时设置 */
    options.c_cc[VTIME] = timeout_ms / 100; /* 转换为1/10秒 */

    if (tcsetattr(g_port_handle, TCSANOW, &options) < 0)
    {
        emit_error(0, errno, "Failed to set port timeout");
        return -1;
    }

    /* 接收数据 */
    int bytes_read = read(g_port_handle, buffer, buffer_len);

    /* 恢复原有设置 */
    options.c_cc[VTIME] = 10; /* 恢复为1秒 */
    tcsetattr(g_port_handle, TCSANOW, &options);

    if (bytes_read < 0)
    {
        emit_error(0, errno, "Failed to read from port");
        return -1;
    }

    return bytes_read;
}

/* ===================== 消息发送函数 ===================== */
static void emit_msg(const Rs422Msg *msg)
{
    if (g_callback)
    {
        g_callback(g_callback_userdata, msg);
    }
}

static void emit_status(uint32_t task_id, int status_code, const char *status_msg)
{
    Rs422Msg m = {0};
    m.type = RS422_MSG_STATUS;
    m.u.status.task_id = task_id;
    m.u.status.status_code = status_code;
    snprintf(m.u.status.status_msg, sizeof(m.u.status.status_msg), "%s", status_msg ? status_msg : "");
    emit_msg(&m);
}

static void emit_error(uint32_t task_id, int error_code, const char *error_msg)
{
    Rs422Msg m = {0};
    m.type = RS422_MSG_ERROR;
    m.u.error.task_id = task_id;
    m.u.error.error_code = error_code;
    snprintf(m.u.error.error_msg, sizeof(m.u.error.error_msg), "%s", error_msg ? error_msg : "");
    emit_msg(&m);
}

static void emit_received_data(uint32_t task_id, const uint8_t *data, uint16_t data_len)
{
    Rs422Msg m = {0};
    m.type = RS422_MSG_RECEIVED;
    m.u.data.task_id = task_id;
    m.u.data.data = (uint8_t *)data;
    m.u.data.data_len = data_len;
    m.u.data.status = 0;
    emit_msg(&m);
}

/* 计算校验和（符合data_frame.h规范） */
static uint16_t nixyk_calculate_checksum(const uint8_t *data, size_t len)
{
    return nixyk_checksum16(data, len);
}

/* 组帧函数（封装标准数据帧构建） */
static uint16_t nixyk_assemble_frame(uint8_t *frame_buffer, size_t buffer_size, const uint8_t *data, uint16_t data_len)
{
    nixyk_data_frame_t frame = {
        .dev7 = 0x01,   /* 默认设备ID */
        .dtype4 = 0x0F, /* 文件型传输 */
        .seg_flag = NIXYK_SEG_SINGLE,
        .seq14 = 0,
        .seg_no = 0,
        .data = data,
        .data_len = data_len};

    size_t out_len = 0;
    if (nixyk_data_build(&frame, frame_buffer, buffer_size, &out_len) == 0)
    {
        return (uint16_t)out_len;
    }
    return 0;
}

/* ===================== 接收线程函数 ===================== */
static void *rs422_receive_thread(void *arg)
{
    (void)arg;
    emit_status(0, 0, "RS422 receive thread started");

    /* 接收缓冲区 */
    uint8_t recv_buffer[RS422_RECEIVE_BUFFER_SIZE];
    uint8_t frame_buffer[RS422_RECEIVE_BUFFER_SIZE];
    int frame_pos = 0;
    uint8_t state = 0;
    static uint32_t receive_task_id = 1; // 递增的接收任务ID

    while (!g_shutdown)
    {
        /* 持续接收数据 */
        int bytes_read = receive_data(recv_buffer, sizeof(recv_buffer), 1000); /* 1秒超时 */

        if (bytes_read < 0)
        {
            /* 发生错误，继续下一次接收 */
            continue;
        }

        if (bytes_read == 0)
        {
            /* 超时，继续下一次接收 */
            continue;
        }

        /* 处理接收到的数据 */
        for (int i = 0; i < bytes_read; i++)
        {
            uint8_t byte = recv_buffer[i];

            /* 状态机解析帧 */
            switch (state)
            {
            case 0: /* 等待帧头第一个字节 */
                if (byte == NIXYK_FRAME_HEAD_BYTE1)
                {
                    frame_buffer[frame_pos++] = byte;
                    state = 1;
                }
                break;

            case 1: /* 等待帧头第二个字节 */
                if (byte == NIXYK_FRAME_HEAD_BYTE2)
                {
                    frame_buffer[frame_pos++] = byte;
                    state = 2;
                }
                else
                {
                    /* 帧头不匹配，重新开始 */
                    frame_pos = 0;
                    state = 0;
                }
                break;

            case 2: /* 接收数据 */
                frame_buffer[frame_pos++] = byte;

                /* 检查是否接收到足够的数据来解析帧 */
                if (frame_pos >= 8) // 最小帧长度：ID(2) + 主导头(4) + 数据域长度(2)
                {
                    // 解析数据域长度
                    uint16_t data_len_field = (frame_buffer[6] << 8) | frame_buffer[7];

                    // 计算总帧长：ID(2) + 主导头(4) + 数据域长度(2) + 数据域长度 + 校验和(2)
                    uint16_t expected_frame_len = 2 + 4 + 2 + data_len_field + 2;

                    if (frame_pos >= expected_frame_len)
                    {
                        /* 完整的帧已接收 */
                        uint16_t frame_len = expected_frame_len;

                        /* 验证校验和 - 按照规范，对主导头(不含标识符)+数据域长度+数据域计算 */
                        uint16_t calculated_checksum = nixyk_checksum16(
                            &frame_buffer[2],      // 从主导头开始
                            4 + 2 + data_len_field // 主导头(4) + 数据域长度(2) + 数据域
                        );

                        // 提取接收到的校验和（位于帧末尾）
                        uint16_t received_checksum =
                            (frame_buffer[frame_len - 2] << 8) |
                            frame_buffer[frame_len - 1];

                        if (calculated_checksum == received_checksum)
                        {
                            /* 校验和正确，处理数据 */
                            uint8_t *data = &frame_buffer[8]; // 跳过ID、主导头和数据域长度
                            uint16_t actual_data_len = data_len_field;

                            /* 发送接收到的数据通知 */
                            emit_received_data(receive_task_id, data, actual_data_len);

                            /* 数据处理完成后，自动发送响应 */
                            uint8_t response_buffer[RS422_SEND_BUFFER_SIZE];
                            uint16_t response_len = nixyk_assemble_frame(
                                response_buffer,
                                sizeof(response_buffer),
                                data,
                                actual_data_len);

                            if (response_len > 0)
                            {
                                send_data(response_buffer, response_len, 1000);
                                emit_status(receive_task_id, 0, "Response sent successfully");
                            }
                            else
                            {
                                emit_error(receive_task_id, -1, "Failed to assemble response frame");
                            }
                        }
                        else
                        {
                            emit_error(receive_task_id, -1, "Checksum verification failed");
                        }

                        /* 重置状态机，准备接收下一帧 */
                        frame_pos = 0;
                        state = 0;
                        receive_task_id++;
                    }
                }
                break;
            }
        }
    }

    emit_status(0, 0, "RS422 receive thread exited");
    return NULL;
}

/* ===================== 服务线程函数 ===================== */
static void *rs422_thread(void *arg)
{
    (void)arg;
    emit_status(0, 0, "RS422 service thread started");

    while (!g_shutdown)
    {
        Rs422Task task;
        if (q_pop(&task) != 0)
        {
            break;
        }

        int rc = 0;

        switch (task.type)
        {
        case RS422_TASK_SEND_DATA:
        {
            /* 线程同步锁，保护共享资源 */
            pthread_mutex_lock(&g_mutex);

            /* 组帧并发送数据 */
            uint8_t frame_buffer[RS422_SEND_BUFFER_SIZE];
            uint16_t frame_len = nixyk_assemble_frame(
                frame_buffer, sizeof(frame_buffer),
                task.u.send.data, task.u.send.data_len);

            if (frame_len > 0)
            {
                rc = send_data(frame_buffer, frame_len, task.u.send.timeout_ms);
                if (rc < 0)
                {
                    emit_error(task.task_id, rc, "Failed to send data");
                }
                else
                {
                    emit_status(task.task_id, 0, "Data sent successfully");
                }
            }
            else
            {
                rc = -1;
                emit_error(task.task_id, rc, "Failed to assemble frame");
            }

            pthread_mutex_unlock(&g_mutex);
            break;
        }

        case RS422_TASK_CONFIG_PORT:
        {
            /* 线程同步锁，保护共享资源 */
            pthread_mutex_lock(&g_mutex);

            rc = configure_port(&task.u.config);
            if (rc == 0)
            {
                emit_status(task.task_id, 0, "Port configured successfully");
            }
            else
            {
                emit_error(task.task_id, rc, "Failed to configure port");
            }

            pthread_mutex_unlock(&g_mutex);
            break;
        }

        case RS422_TASK_RECEIVE_DATA:
        {
            /* 此任务类型在接收线程中持续处理 */
            emit_status(task.task_id, 0, "Receive task is running in receive thread");
            break;
        }

        default:
        {
            rc = -1;
            emit_error(task.task_id, rc, "Unknown task type");
            break;
        }
        }
    }

    emit_status(0, 0, "RS422 service thread exited");
    return NULL;
}

/* ===================== 对外接口函数实现 ===================== */
int rs422_init(Rs422Callback callback, void *userdata)
{
    if (g_inited)
    {
        return 0;
    }

    /* 初始化互斥锁和条件变量 */
    pthread_mutex_init(&g_mutex, NULL);
    pthread_cond_init(&g_cond, NULL);

    /* 初始化任务队列 */
    memset(&g_q, 0, sizeof(g_q));
    pthread_mutex_init(&g_q.mtx, NULL);
    pthread_cond_init(&g_q.cv_put, NULL);
    pthread_cond_init(&g_q.cv_get, NULL);

    /* 保存回调函数和用户数据 */
    g_callback = callback;
    g_callback_userdata = userdata;
    g_shutdown = 0;

    /* 打开端口 */
    if (open_port(g_current_config.port_name) != 0)
    {
        emit_error(0, -1, "Failed to open default port");
        return -1;
    }

    /* 配置端口 */
    if (configure_port(&g_current_config) != 0)
    {
        emit_error(0, -1, "Failed to configure default port");
        close_port();
        return -1;
    }

    /* 创建服务线程 */
    if (pthread_create(&g_thr, NULL, rs422_thread, NULL) != 0)
    {
        emit_error(0, errno, "Failed to create service thread");
        close_port();
        return -1;
    }

    /* 创建接收线程 */
    if (pthread_create(&g_recv_thr, NULL, rs422_receive_thread, NULL) != 0)
    {
        emit_error(0, errno, "Failed to create receive thread");
        close_port();
        pthread_cancel(g_thr);
        pthread_join(g_thr, NULL);
        return -1;
    }

    g_inited = 1;
    emit_status(0, 0, "RS422 initialized successfully");
    return 0;
}

void rs422_shutdown(void)
{
    if (!g_inited)
    {
        return;
    }

    /* 设置关闭标志 */
    g_shutdown = 1;

    /* 唤醒所有等待的线程 */
    pthread_mutex_lock(&g_q.mtx);
    pthread_cond_broadcast(&g_q.cv_get);
    pthread_mutex_unlock(&g_q.mtx);

    /* 等待线程退出 */
    pthread_join(g_thr, NULL);
    pthread_join(g_recv_thr, NULL);

    /* 关闭端口 */
    close_port();

    /* 销毁互斥锁和条件变量 */
    pthread_mutex_destroy(&g_mutex);
    pthread_cond_destroy(&g_cond);
    pthread_mutex_destroy(&g_q.mtx);
    pthread_cond_destroy(&g_q.cv_put);
    pthread_cond_destroy(&g_q.cv_get);

    /* 重置状态 */
    g_inited = 0;
    g_callback = NULL;
    g_callback_userdata = NULL;

    emit_status(0, 0, "RS422 shutdown successfully");
}

int rs422_configure_port(const Rs422PortConfig *config)
{
    if (!g_inited)
    {
        emit_error(0, -1, "RS422 not initialized");
        return -1;
    }

    if (!config)
    {
        emit_error(0, -1, "Invalid configuration");
        return -1;
    }

    /* 关闭当前端口 */
    close_port();

    /* 打开新端口 */
    if (open_port(config->port_name) != 0)
    {
        emit_error(0, -1, "Failed to open port");
        return -1;
    }

    /* 配置端口 */
    return configure_port(config);
}

int rs422_send_data(const uint8_t *data, uint16_t data_len, uint32_t timeout_ms)
{
    if (!g_inited)
    {
        emit_error(0, -1, "RS422 not initialized");
        return -1;
    }

    if (!data || data_len == 0)
    {
        emit_error(0, -1, "Invalid data");
        return -1;
    }

    /* 线程同步锁，保护共享资源 */
    pthread_mutex_lock(&g_mutex);

    /* 组帧并发送数据 */
    uint8_t frame_buffer[RS422_SEND_BUFFER_SIZE];
    uint16_t frame_len = nixyk_assemble_frame(
        frame_buffer, sizeof(frame_buffer),
        data, data_len);

    int result = -1;
    if (frame_len > 0)
    {
        result = send_data(frame_buffer, frame_len, timeout_ms);
    }
    else
    {
        emit_error(0, -1, "Failed to assemble frame");
    }

    pthread_mutex_unlock(&g_mutex);
    return result;
}

int rs422_receive_data(uint8_t *buffer, uint16_t buffer_len, uint32_t timeout_ms)
{
    if (!g_inited)
    {
        emit_error(0, -1, "RS422 not initialized");
        return -1;
    }

    if (!buffer || buffer_len == 0)
    {
        emit_error(0, -1, "Invalid buffer");
        return -1;
    }

    /* 直接调用接收函数 */
    return receive_data(buffer, buffer_len, timeout_ms);
}

int rs422_enqueue_task(const Rs422Task *task)
{
    if (!g_inited)
    {
        emit_error(0, -1, "RS422 not initialized");
        return -2;
    }

    if (!task)
    {
        emit_error(0, -1, "Invalid task");
        return -3;
    }

    return q_push(task);
}

int rs422_queue_size(void)
{
    if (!g_inited)
    {
        return 0;
    }

    pthread_mutex_lock(&g_q.mtx);
    int n = g_q.count;
    pthread_mutex_unlock(&g_q.mtx);

    return n;
}

bool rs422_is_initialized(void)
{
    return g_inited;
}