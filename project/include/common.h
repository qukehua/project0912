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

// ================== 全局定义 ==================
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
    void (*task_func)(void*);  // 任务函数指针
    void* arg;                  // 任务参数
    int interval;               // 执行间隔(秒)
    time_t last_executed;       // 上次执行时间
} TimedTask;

// ================== 类型定义 ==================
typedef enum {
    MSG_REST_COMMAND,
    MSG_CAN_COMMAND,
    MSG_FILE_RECEIVED,
    MSG_UDP_DATA,
    MSG_FILE_TRANSFER,
    MSG_SYSTEM_CTRL,
    MSG_UPDATE_TASK
} MessageType;

// 更新软件的三种模式
typedef enum {
    UPDATE_TYPE_SOFTWARE = 0,
    UPDATE_TYPE_REINITIATE = 1,
    UPDATE_TYPE_ROLLBACK = 2
} UpdateType;

// ================== 下载任务定义 ==================
typedef struct {
    char requestId[64];       // 请求ID
    char ruid[64];            // 网元RUID
    char download_url[512];   // 下载URL
    char save_path[256];       // 本地保存路径
    char status[16];           // 任务状态（"success", "updating", "fail"）
    char error_info[256];      // 错误信息
} DownloadTask;

////////////////////////板卡已存储的各个模块使用的软件信息
typedef struct {
    char ruid[64];            // 网元RUID
    char currentVersion[64];      ////当前版本
    char current_save_path[256];  ////当前版本存储位置
    char status[16];             /// 当前版本更新状态
    char error_info[256];      // 错误信息
    char additional_info[256];

    char lastVersion[64];      ////上一个版本
    char last_save_path[256];  ////上一个版本存储位置
    int versionCount;         // 备份版本的实际数量
    char backupVersionList[10][64];// 备份版本列表（包含上一个版本）
    char backup_save_path[10][256];  ////备份版本存储位置
    char dl_Version[64];
    char dl_status[16];
} software_info;


//软件更新任务定义
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

// ================== 全局状态结构 ==================
typedef struct GlobalState {
    atomic_int shutdown_requested;
    SystemConfig config;

    // 新增eth1任务处理相关字段
    UpdateTask* eth1_current_task;    // ETH1当前任务指针
    pthread_mutex_t eth1_task_mutex;  // ETH1任务互斥锁
    pthread_cond_t eth1_task_cond;   // ETH1任务条件变量
    int eth1_task_available;          // ETH1任务可用标志

    // 线程间通信
    Message msg_queue[MESSAGE_QUEUE_SIZE];
    int msg_queue_head;
    int msg_queue_tail;
    pthread_mutex_t queue_mutex;
    pthread_cond_t queue_cond;

    TimedTask timed_tasks[MAX_TIMED_TASKS];
    int timed_task_count;
    pthread_mutex_t timed_task_mutex;

    // FTP下载任务序列、线程定义
    DownloadTask dl_task_queue[MAX_DL_TASK_QUEUE];
    int dl_task_count;
    int active_downloads;
    pthread_mutex_t dl_task_mutex;
    pthread_cond_t dl_task_cond;
 
    // 更新任务
    UpdateTask update_task_queue[MAX_UPDATE_TASK_QUEUE];
    int update_task_count;
    pthread_mutex_t update_task_mutex;

    // Token黑名单
    BlacklistEntry* token_blacklist[MAX_BLACKLIST_TOKENS];
    pthread_mutex_t blacklist_mutex;
} GlobalState;

// ================== 全局变量声明 ==================
extern GlobalState global_state;
extern int cnt_ruid;
extern software_info  list_ruid[MAX_RUID_NUM];//最多16个ruid

// ================== 函数声明 ==================

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