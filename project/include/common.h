#ifndef COMMON_H
#define COMMON_H

#include <ulfius.h>
#include <jansson.h>
#include <jwt.h>
#include <gnutls/gnutls.h>
#include <pthread.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <curl/curl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <stdatomic.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <linux/can.h>
#include <sys/ioctl.h>
#include <net/if.h>

// ================== ȫ�ֶ��� ==================
#define PORT 443
#define KEY_PEM "server.key"
#define CERT_PEM "server.crt"
#define TOKEN_EXPIRES 1800
#define MAX_DL_TASK_QUEUE 100
#define MAX_DL_HISTORY_TASKS 100
#define MAX_UPDATE_TASK_QUEUE 100
#define MAX_BLACKLIST_TOKENS 1000
#define MAX_EVENTS 10
#define THREAD_POOL_SIZE 5
#define MESSAGE_QUEUE_SIZE 100
#define MAX_UPDATE_HISTORY_TASKS 100
#define MAX_TIMED_TASKS 10
#define MAX_RUID_NUM 16

typedef struct {
    void (*task_func)(void*);  // ������ָ��
    void* arg;                  // �������
    int interval;               // ִ�м��(��)
    time_t last_executed;       // �ϴ�ִ��ʱ��
} TimedTask;

// ================== ���Ͷ��� ==================
typedef enum {
    MSG_REST_COMMAND,
    MSG_CAN_COMMAND,
    MSG_FILE_RECEIVED,
    MSG_UDP_DATA,
    MSG_FILE_TRANSFER,
    MSG_SYSTEM_CTRL,
    MSG_UPDATE_TASK
} MessageType;

// �������������ģʽ
typedef enum {
    UPDATE_TYPE_SOFTWARE = 0,
    UPDATE_TYPE_REINITIATE = 1,
    UPDATE_TYPE_ROLLBACK = 2
} UpdateType;

// ================== ���������� ==================
typedef struct {
    char requestId[64];       // ����ID
    char ruid[64];            // ��ԪRUID
    char download_url[512];   // ����URL
    char save_path[256];       // ���ر���·��
    char status[16];           // ����״̬��"success", "updating", "fail"��
    char error_info[256];      // ������Ϣ
} DownloadTask;

////////////////////////�忨�Ѵ洢�ĸ���ģ��ʹ�õ������Ϣ
typedef struct {
    char ruid[64];            // ��ԪRUID
    char currentVersion[64];      ////��ǰ�汾
    char current_save_path[256];  ////��ǰ�汾�洢λ��
    char status[16];             /// ��ǰ�汾����״̬
    char error_info[256];      // ������Ϣ
    char additional_info[256];

    char lastVersion[64];      ////��һ���汾
    char last_save_path[256];  ////��һ���汾�洢λ��
    int versionCount;         // ���ݰ汾��ʵ������
    char backupVersionList[10][64];// ���ݰ汾�б�������һ���汾��
    char backup_save_path[10][256];  ////���ݰ汾�洢λ��
    char dl_Version[64];
    char dl_status[16];
} software_info;


//�������������
typedef struct {
    char requestId[64];
    char ruid[64];
    char updateVersion[64];
    UpdateType updateType;
    char status[16];
    char error_info[256];
    char additional_info[256];
    char save_path[256];
} UpdateTask;

typedef struct {
    char* token;
    time_t expire_time;
} BlacklistEntry;

typedef struct {
    char* eth0_ip;
    char* eth1_ip;
    char* can_interface;
    int rest_port;
    int ftp_port;
    int udp_port;
} SystemConfig;

typedef struct {
    MessageType type;
    void* data;
    size_t data_size;
    int source;
} Message;

// ================== ȫ��״̬�ṹ ==================
typedef struct GlobalState {
    atomic_int shutdown_requested;
    SystemConfig config;

    // ����eth1����������ֶ�
    UpdateTask* eth1_current_task;    // ETH1��ǰ����ָ��
    pthread_mutex_t eth1_task_mutex;  // ETH1���񻥳���
    pthread_cond_t eth1_task_cond;   // ETH1������������
    int eth1_task_available;          // ETH1������ñ�־

    // �̼߳�ͨ��
    Message msg_queue[MESSAGE_QUEUE_SIZE];
    int msg_queue_head;
    int msg_queue_tail;
    pthread_mutex_t queue_mutex;
    pthread_cond_t queue_cond;

    TimedTask timed_tasks[MAX_TIMED_TASKS];
    int timed_task_count;
    pthread_mutex_t timed_task_mutex;

    // FTP�����������С��̶߳���
    DownloadTask dl_task_queue[MAX_DL_TASK_QUEUE];
    int dl_task_count;
    int active_downloads;
    pthread_mutex_t dl_task_mutex;
    pthread_cond_t dl_task_cond;
 
    // ��������
    UpdateTask update_task_queue[MAX_UPDATE_TASK_QUEUE];
    int update_task_count;
    pthread_mutex_t update_task_mutex;

    // Token������
    BlacklistEntry* token_blacklist[MAX_BLACKLIST_TOKENS];
    pthread_mutex_t blacklist_mutex;
} GlobalState;

// ================== ȫ�ֱ������� ==================
extern GlobalState global_state;
extern int cnt_ruid;
extern software_info  list_ruid[MAX_RUID_NUM];//���16��ruid

// ================== �������� ==================

void* eth0_service_thread(void* arg);
void* eth1_service_thread(void* arg);
void* can_service_thread(void* arg);
void* ftp_dl_worker(void* arg);
void* command_worker_thread(void* arg);
void* timer_service_thread(void* arg);
void register_timed_task(GlobalState* state, void (*task_func)(void*),
    void* arg, int interval);

int enqueue_message(GlobalState* state,Message* msg);
int dequeue_message(GlobalState* state,Message* msg);

void handle_can_command(struct can_frame* frame);
void handle_udp_data(void* data, size_t data_size);

int login_callback(const struct _u_request* request, struct _u_response* response, void* user_data);
int handle_handshake_request(const struct _u_request* request, struct _u_response* response, void* user_data);
int logout_callback(const struct _u_request* request, struct _u_response* response, void* user_data);
int software_version_callback(const struct _u_request* request, struct _u_response* response, void* user_data);
int software_download_callback(const struct _u_request* request, struct _u_response* response, void* user_data);
int software_download_status_callback(const struct _u_request* request, struct _u_response* response, void* user_data);
int software_update_callback(const struct _u_request* request, struct _u_response* response, void* user_data);
int software_update_status_callback(const struct _u_request* request, struct _u_response* response, void* user_data);
int reinitiate_callback(const struct _u_request* request, struct _u_response* response, void* user_data);
int reinitiate_status_callback(const struct _u_request* request, struct _u_response* response, void* user_data);
int software_rollback_callback(const struct _u_request* request, struct _u_response* response, void* user_data);
int software_rollback_status_callback(const struct _u_request* request, struct _u_response* response, void* user_data);
char* generate_jwt(const char* username);
int verify_jwt(const char* access_token);
void blacklist_token(const char* token, time_t expire_time);
int is_token_blacklisted(const char* token);
int add_dl_task(DownloadTask task);
int add_update_task(UpdateTask task);

void signal_handler(int sig);
void cleanup_resources();

#endif // COMMON_H