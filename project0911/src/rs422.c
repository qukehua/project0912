#include "../include/rs422.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

// ȫ�ֱ���
RS422Task rs422_tasks[5] = {0};
GlobalState *rs422_global_state = NULL;

// ��ʼ��RS422�豸
static int rs422_open_device(const char *dev_path)
{
    int fd = open(dev_path, O_RDWR | O_NOCTTY | O_NDELAY);
    if (fd < 0)
    {
        printf("Failed to open %s: %s\n", dev_path, strerror(errno));
        return -1;
    }

    // ���ô��ڲ���
    struct termios options;
    tcgetattr(fd, &options);
    options.c_cflag = B115200 | CS8 | CLOCAL | CREAD;
    options.c_iflag = IGNPAR;
    options.c_oflag = 0;
    options.c_lflag = 0;
    tcflush(fd, TCIFLUSH);
    tcsetattr(fd, TCSANOW, &options);

    return fd;
}

// �ر�RS422�豸
static void rs422_close_device(int fd)
{
    if (fd > 0)
    {
        close(fd);
    }
}

// ����ָ��֡���ȴ�Ӧ��
static int rs422_send_command(int fd, uint16_t cmd_code, const uint8_t *params, uint8_t param_len)
{
    CommandFrame cmd_frame;
    ResponseFrame resp_frame;
    uint8_t cmd_buf[256];
    uint8_t resp_buf[256];
    int ret;

    // ��ʼ��ָ��֡
    cmd_frame_init(&cmd_frame, CMD_TYPE_STANDARD, cmd_code);
    if (params && param_len > 0)
    {
        cmd_frame_set_param(&cmd_frame, params, param_len);
    }

    // ����ָ��֡
    uint16_t cmd_len = cmd_frame_encode(&cmd_frame, cmd_buf, sizeof(cmd_buf));
    if (cmd_len == 0)
    {
        printf("Failed to encode command frame\n");
        return -1;
    }

    // ����ָ��֡
    ret = write(fd, cmd_buf, cmd_len);
    if (ret != cmd_len)
    {
        printf("Failed to send command frame\n");
        return -2;
    }

    // �ȴ�Ӧ�𣨳�ʱ5�룩
    fd_set read_fds;
    struct timeval timeout = {5, 0};
    int bytes_read;

    FD_ZERO(&read_fds);
    FD_SET(fd, &read_fds);

    ret = select(fd + 1, &read_fds, NULL, NULL, &timeout);
    if (ret <= 0)
    {
        printf("Timeout waiting for command response\n");
        return -3;
    }

    // ��ȡӦ��֡
    bytes_read = read(fd, resp_buf, sizeof(resp_buf));
    if (bytes_read <= 0)
    {
        printf("Failed to read response frame\n");
        return -4;
    }

    // ����Ӧ��֡
    ret = resp_frame_decode(&resp_frame, resp_buf, bytes_read);
    if (ret != 0)
    {
        printf("Failed to decode response frame: %d\n", ret);
        return -5;
    }

    // ���Ӧ���Ƿ�����
    if ((resp_frame.resp_code2 & RESP_ERROR) != 0)
    {
        printf("Command execution error: 0x%04X\n", resp_frame.resp_code2);
        return -6;
    }

    return 0;
}

// ��������֡���ȴ�Ӧ��
static int rs422_send_data_frame(int fd, uint8_t *data, uint16_t data_len, uint8_t grid, uint16_t nuid, uint8_t apid)
{
    DataFrame data_frame;
    ResponseFrame resp_frame;
    uint8_t data_buf[65536 + 10]; // ����֡��󳤶�+ͷ��
    uint8_t resp_buf[256];
    int ret;
    int retry_count = 0;

retry:
    // ��ʼ������֡
    data_frame_init(&data_frame, apid, grid);
    data_frame.header.nuid = nuid;
    data_frame_set_data(&data_frame, data, data_len);

    // 编码数据帧 - 限制buf_len不超过uint16_t最大值
    uint32_t frame_len = data_frame_encode(&data_frame, data_buf, 65535);
    if (frame_len == 0)
    {
        printf("Failed to encode data frame\n");
        data_frame_free(&data_frame);
        return -1;
    }

    // ��������֡
    ret = write(fd, data_buf, frame_len);
    if (ret != frame_len)
    {
        printf("Failed to send data frame\n");
        data_frame_free(&data_frame);
        if (retry_count < 3)
        {
            retry_count++;
            goto retry;
        }
        return -2;
    }

    // �ȴ�Ӧ�𣨳�ʱ5�룩
    fd_set read_fds;
    struct timeval timeout = {5, 0};
    int bytes_read;

    FD_ZERO(&read_fds);
    FD_SET(fd, &read_fds);

    ret = select(fd + 1, &read_fds, NULL, NULL, &timeout);
    if (ret <= 0)
    {
        printf("Timeout waiting for data response\n");
        data_frame_free(&data_frame);
        if (retry_count < 3)
        {
            retry_count++;
            goto retry;
        }
        return -3;
    }

    // ��ȡӦ��֡
    bytes_read = read(fd, resp_buf, sizeof(resp_buf));
    if (bytes_read <= 0)
    {
        printf("Failed to read data response\n");
        data_frame_free(&data_frame);
        if (retry_count < 3)
        {
            retry_count++;
            goto retry;
        }
        return -4;
    }

    // ����Ӧ��֡
    ret = resp_frame_decode(&resp_frame, resp_buf, bytes_read);
    if (ret != 0)
    {
        printf("Failed to decode data response: %d\n", ret);
        data_frame_free(&data_frame);
        if (retry_count < 3)
        {
            retry_count++;
            goto retry;
        }
        return -5;
    }

    // �ͷ�����֡�ڴ�
    data_frame_free(&data_frame);

    // ���Ӧ���Ƿ�����
    if ((resp_frame.resp_code2 & RESP_ERROR) != 0)
    {
        printf("Data frame error: 0x%04X\n", resp_frame.resp_code2);
        if (retry_count < 3)
        {
            retry_count++;
            goto retry;
        }
        return -6;
    }

    return 0;
}

// ��ȡ�ļ����ݵ�������
static uint8_t *read_file(const char *filename, uint32_t *file_size)
{
    FILE *file = fopen(filename, "rb");
    if (!file)
    {
        printf("Failed to open file: %s\n", filename);
        return NULL;
    }

    // ��ȡ�ļ���С
    fseek(file, 0, SEEK_END);
    *file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    // �����ڴ�
    uint8_t *buffer = (uint8_t *)malloc(*file_size);
    if (!buffer)
    {
        printf("Failed to allocate memory for file\n");
        fclose(file);
        return NULL;
    }

    // ��ȡ�ļ�����
    size_t bytes_read = fread(buffer, 1, *file_size, file);
    if (bytes_read != *file_size)
    {
        printf("Failed to read file content\n");
        free(buffer);
        fclose(file);
        return NULL;
    }

    fclose(file);
    return buffer;
}

// RS422�����߳�
void *rs422_service_thread(void *arg)
{
    RS422Task *task = (RS422Task *)arg;
    uint32_t file_size;
    uint8_t *file_data;
    int ret = 0;
    int total_retrans = 0;

    printf("Starting RS422 task on %s for file: %s\n", task->dev_path, task->filename);

    // ���豸
    task->dev_fd = rs422_open_device(task->dev_path);
    if (task->dev_fd < 0)
    {
        task->task_result = -1;
        task->task_completed = 1;
        return NULL;
    }

    // ��ȡ�ļ�����
    file_data = read_file(task->filename, &file_size);
    if (!file_data)
    {
        task->task_result = -2;
        task->task_completed = 1;
        rs422_close_device(task->dev_fd);
        return NULL;
    }

    // �ļ�������ѭ����֧���ش���
    while (total_retrans <= 2 && !atomic_load(&rs422_global_state->shutdown_requested))
    {     
        // 1) �ȴ����� //
        pthread_mutex_lock(&global_state.rs422_task_mutex[task->dev_index]);
		while (!global_state.eth1_task_available &&
			!atomic_load(&global_state.shutdown_requested)) {
			pthread_cond_wait(&global_state.eth1_task_cond, &global_state.eth1_task_mutex);
		}
		if (atomic_load(&global_state.shutdown_requested)) {
			pthread_mutex_unlock(&global_state.eth1_task_mutex);
			break;
		}
        // �����ļ����俪��ָ��
        ret = rs422_send_command(task->dev_fd, CMD_FILE_START,
                                 (uint8_t *)&file_size, sizeof(file_size));
        if (ret != 0)
        {
            printf("Failed to send file start command, retry %d\n", total_retrans);
            total_retrans++;
            continue;
        }

        // ��Ƭ�����ļ�����
        const uint16_t frame_size = 1024; // ÿ֡���ݴ�С
        uint32_t offset = 0;
        uint16_t nuid = 0;
        int frame_count = (file_size + frame_size - 1) / frame_size;
        int frame_index = 0;

        while (offset < file_size)
        {
            uint16_t send_size = (file_size - offset) > frame_size ? frame_size : (file_size - offset);
            uint8_t grid;

            // ����֡���ͣ���֡���м�֡��β֡��֡��
            if (frame_count == 1)
            {
                grid = GRID_SINGLE;
            }
            else if (frame_index == 0)
            {
                grid = GRID_FIRST;
            }
            else if (frame_index == frame_count - 1)
            {
                grid = GRID_LAST;
            }
            else
            {
                grid = GRID_MIDDLE;
            }

            // ��������֡
            ret = rs422_send_data_frame(task->dev_fd, &file_data[offset],
                                        send_size, grid, nuid, task->apid);
            if (ret != 0)
            {
                printf("Failed to send data frame %d, retry\n", frame_index);
                // ��֡�ش�����rs422_send_data_frame�д���
                // �����Ȼʧ�ܣ��������ļ��ش�
                goto file_retry;
            }

            offset += send_size;
            frame_index++;
            nuid++;
        }

        // �����ļ��������ָ��
        ret = rs422_send_command(task->dev_fd, CMD_FILE_END, NULL, 0);
        if (ret != 0)
        {
            printf("Failed to send file end command, retry %d\n", total_retrans);
            total_retrans++;
            continue;
        }

        // ����֡���ͳɹ�
        printf("File %s sent successfully via %s\n", task->filename, task->dev_path);
        break;

    file_retry:
        total_retrans++;
    }

    // ������Դ
    free(file_data);
    rs422_close_device(task->dev_fd);

    // ����������
    if (total_retrans > 2)
    {
        printf("File %s failed to send after 3 retries\n", task->filename);
        task->task_result = -3;
    }
    else
    {
        task->task_result = 0;
    }

    task->task_completed = 1;
    return NULL;
}

// ��ʼ��RS422ģ��
int rs422_init(GlobalState *state)
{
    rs422_global_state = state;
    const char *dev_paths[5] = {
        RS422_DEV_0,
        RS422_DEV_1,
        RS422_DEV_2,
        RS422_DEV_3,
        RS422_DEV_4};

    // ��ʼ������ṹ
    for (int i = 0; i < 5; i++)
    {   
        rs422_tasks[i].dev_index = i;
        strncpy(rs422_tasks[i].dev_path, dev_paths[i], sizeof(rs422_tasks[i].dev_path) - 1);
        rs422_tasks[i].dev_fd = -1;
        rs422_tasks[i].running = 0;
        rs422_tasks[i].task_completed = 0;
        rs422_tasks[i].task_result = 0;
    }

    printf("RS422 module initialized\n");
    return 0;
}

// // ����RS422ģ��
// void rs422_cleanup()
// {
//     // �ȴ������������
//     for (int i = 0; i < 5; i++)
//     {
//         if (rs422_tasks[i].running)
//         {
//             pthread_join(rs422_tasks[i].thread_id, NULL);
//         }
//     }
// }

// // ����RS422��������
// int rs422_create_task(int dev_index, const char *filename, uint8_t apid)
// {
//     if (dev_index < 0 || dev_index >= 5)
//     {
//         printf("Invalid RS422 device index: %d\n", dev_index);
//         return -1;
//     }

//     RS422Task *task = &rs422_tasks[dev_index];
//     if (task->running)
//     {
//         printf("RS422 device %d is busy\n", dev_index);
//         return -2;
//     }

//     // �����������
//     strncpy(task->filename, filename, sizeof(task->filename) - 1);
//     task->apid = apid;
//     task->running = 1;
//     task->task_completed = 0;
//     task->task_result = 0;

//     // �����߳�
//     int ret = pthread_create(&task->thread_id, NULL, rs422_service_thread, task);
//     if (ret != 0)
//     {
//         printf("Failed to create RS422 thread: %d\n", ret);
//         task->running = 0;
//         return -3;
//     }

//     return 0;
// }
