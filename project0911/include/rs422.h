#ifndef RS422_H
#define RS422_H

#include "../include/common.h"
#include "../include/data_frame_rs422.h"
#include "../include/response_frame.h"
#include "../include/command_frame.h"

// RS422�豸·��
#define RS422_DEV_0 "/dev/ttyRS422-0"
#define RS422_DEV_1 "/dev/ttyRS422-1"
#define RS422_DEV_2 "/dev/ttyRS422-2"
#define RS422_DEV_3 "/dev/ttyRS422-3"
#define RS422_DEV_4 "/dev/ttyRS422-4"

// RS422����ṹ��
typedef struct
{
    int dev_fd;          // �豸�ļ�������
    char dev_path[32];   // �豸·��
    //pthread_t thread_id; // �߳�ID//
    int dev_index;       // �豸���� (0-4)
    int running;         // ���б�־
    char filename[256];  // �������ļ���
    uint8_t apid;        // Ӧ�ù��̱�ʶ
    int task_completed;  // ������ɱ�־
    int task_result;     // ������ 0-�ɹ� <0-ʧ��
} RS422Task;

// ȫ��RS422��������
extern RS422Task rs422_tasks[5];

// ��������
int rs422_init();
void rs422_cleanup();
int rs422_create_task(int dev_index, const char *filename, uint8_t apid);
void *rs422_service_thread(void *arg);

#endif
