#include "rs422.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* 消息回调函数 */
static void rs422_message_handler(void *user, const Rs422Msg *msg)
{
    (void)user; /* 未使用的参数 */

    switch (msg->type)
    {
    case RS422_MSG_RECEIVED:
    {
        printf("[RS422] Received data (task_id=%u, len=%u):\n",
               msg->u.data.task_id, msg->u.data.data_len);

        /* 打印接收到的数据（十六进制格式） */
        printf("[RS422] Data: ");
        for (uint16_t i = 0; i < msg->u.data.data_len; i++)
        {
            printf("0x%02X ", msg->u.data.data[i]);
        }
        printf("\n");

        /* 这里可以根据接收到的数据进行相应的处理 */
        /* 例如，可以发送响应数据 */
        uint8_t response[] = "Hello from RS422 test program!";
        rs422_send_data(response, sizeof(response), 1000);
        break;
    }

    case RS422_MSG_DONE:
    {
        printf("[RS422] Data sent (task_id=%u)\n", msg->u.data.task_id);
        break;
    }

    case RS422_MSG_ERROR:
    {
        printf("[RS422] Error (task_id=%u, code=%d): %s\n",
               msg->u.error.task_id, msg->u.error.error_code, msg->u.error.error_msg);
        break;
    }

    case RS422_MSG_STATUS:
    {
        printf("[RS422] Status (task_id=%u, code=%d): %s\n",
               msg->u.status.task_id, msg->u.status.status_code, msg->u.status.status_msg);
        break;
    }

    default:
    {
        printf("[RS422] Unknown message type\n");
        break;
    }
    }
}

/* 主函数 */
int main(int argc, char *argv[])
{
    printf("=== RS422 Test Program ===\n");

    /* 初始化RS422模块 */
    printf("Initializing RS422 module...\n");
    int ret = rs422_init(rs422_message_handler, NULL);
    if (ret != 0)
    {
        printf("Failed to initialize RS422 module. Error code: %d\n", ret);
        return -1;
    }
    printf("RS422 module initialized successfully.\n");

    /* 配置RS422端口（如果需要自定义配置） */
    Rs422PortConfig config = {
        RS422_PORT_DEFAULT,
        RS422_BAUD_RATE_DEFAULT,
        8, /* 数据位 */
        1, /* 停止位 */
        0  /* 无校验 */
    };

    /* 如果命令行参数提供了端口名和波特率，则使用它们 */
    if (argc > 1)
    {
        config.port_name = argv[1];
    }

    if (argc > 2)
    {
        config.baud_rate = atoi(argv[2]);
    }

    printf("Configuring RS422 port: %s, Baud rate: %u\n",
           config.port_name, config.baud_rate);

    ret = rs422_configure_port(&config);
    if (ret != 0)
    {
        printf("Failed to configure RS422 port. Error code: %d\n", ret);
        rs422_shutdown();
        return -1;
    }
    printf("RS422 port configured successfully.\n");

    /* 发送测试数据 */
    uint8_t test_data[] = "Hello RS422! This is a test message.";
    printf("Sending test data...\n");
    ret = rs422_send_data(test_data, sizeof(test_data), 1000);
    if (ret < 0)
    {
        printf("Failed to send data. Error code: %d\n", ret);
    }
    else
    {
        printf("Sent %d bytes of data.\n", ret);
    }

    /* 演示使用任务队列发送数据 */
    Rs422Task task;
    task.task_id = 1001;
    task.type = RS422_TASK_SEND_DATA;
    task.u.send.data = test_data;
    task.u.send.data_len = sizeof(test_data);
    task.u.send.timeout_ms = 1000;

    printf("Enqueuing send task...\n");
    ret = rs422_enqueue_task(&task);
    if (ret != 0)
    {
        printf("Failed to enqueue task. Error code: %d\n", ret);
    }
    else
    {
        printf("Task enqueued successfully.\n");
        printf("Current queue size: %d\n", rs422_queue_size());
    }

    /* 等待一段时间，让接收线程有机会接收数据 */
    printf("Waiting for data reception...\n");
    printf("(Press Ctrl+C to exit)\n");

    /* 持续运行，直到用户中断 */
    while (1)
    {
        sleep(1);

        /* 可以在这里添加定期发送数据的逻辑 */
        /* 例如，每10秒发送一次心跳包 */
        static int counter = 0;
        counter++;
        if (counter % 10 == 0)
        {
            uint8_t heartbeat[] = "RS422 Heartbeat";
            rs422_send_data(heartbeat, sizeof(heartbeat), 1000);
        }
    }

    /* 关闭RS422模块（实际上不会执行到这里，因为上面是无限循环） */
    printf("Shutting down RS422 module...\n");
    rs422_shutdown();
    printf("RS422 module shut down successfully.\n");

    return 0;
}