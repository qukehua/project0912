#ifndef RS422_H
#define RS422_H

#include "../include/common.h"
#include "../include/data_frame_rs422.h"
#include "../include/response_frame.h"
#include "../include/command_frame.h"

// RS422设备路径
#define RS422_DEV_0 "/dev/ttyRS422-0"
#define RS422_DEV_1 "/dev/ttyRS422-1"
#define RS422_DEV_2 "/dev/ttyRS422-2"
#define RS422_DEV_3 "/dev/ttyRS422-3"
#define RS422_DEV_4 "/dev/ttyRS422-4"

// RS422任务结构体
typedef struct
{
    int dev_fd;          // 设备文件描述符
    char dev_path[32];   // 设备路径
    //pthread_t thread_id; // 线程ID//
    int dev_index;       // 设备索引 (0-4)
    int running;         // 运行标志
    char filename[256];  // 待发送文件名
    uint8_t apid;        // 应用过程标识
    int task_completed;  // 任务完成标志
    int task_result;     // 任务结果 0-成功 <0-失败
} RS422Task;

// 全局RS422任务数组
extern RS422Task rs422_tasks[5];

// 函数声明
int rs422_init();
void rs422_cleanup();
int rs422_create_task(int dev_index, const char *filename, uint8_t apid);
void *rs422_service_thread(void *arg);

#endif
