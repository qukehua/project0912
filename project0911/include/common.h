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
#include <errno.h>

// ================== 全局定义 ==================
#define PORT 443
#define KEY_PEM "/opt/server.key"
#define CERT_PEM "/opt/server.crt"
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

// 添加FTP响应代码
#define FTP_RESP_ACCEPTED 150
#define FTP_RESP_SUCCESS 200
#define FTP_RESP_QUEUE_FULL 450
#define FTP_RESP_ERROR 500


//接口ruid
#define can0 0000000
#define eth1 0011000
#define RSS422_1  0b0101000   // 40 (十六进制: 0x28)
#define RSS422_2  0b0101011   // 43 (十六进制: 0x2B)
#define RSS422_3  0b0101101   // 45 (十六进制: 0x2D)
#define RSS422_4  0b0101110   // 46 (十六进制: 0x2E)
#define RSS422_5  0b0110010   // 50 (十六进制: 0x32)

typedef enum {
    TRANSPORT_RS422,
    TRANSPORT_CAN,
    TRANSPORT_UDP
} TransportType;



typedef struct {
    void (*task_func)(void*);  // 任务函数指针
    void* arg;                  // 任务参数
    int interval;               // 执行间隔(秒)
    time_t last_executed;       // 上次执行时间
} TimedTask;

// ================== 中控接收消息定义 ==================
typedef enum {
    MSG_REST_COMMAND,
    MSG_CAN_COMMAND,
    MSG_FILE_RECEIVED,
    MSG_UDP_DATA,
    MSG_FILE_TRANSFER,
    MSG_SYSTEM_CTRL,
    MSG_FTP_COMMAND,

    MSG_QUERY_SOFTWARE_VERSION,      //接收软件版本查询(eth0->自主管理)//////11111111
    MSG_RESPONSE_SOFTWARE_VERSION,    //软件版本查询端口反馈(自主管理<-端口)

    MSG_UPDATE_TASK,                  //接收软件更新指令(eth0->自主管理)
    MSG_RESPONSE_UPDATE_TASK，       // 软件版本更新指令端口反馈(自主管理<-端口)

    MSG_QUERY_UPDATE_STATUS,          //软件更新状态查询(eth0->自主管理) //不需要转发了，直接反馈

    MSG_QUERY_ROLLBACK_TASK,        //接收回退指令(eth0->自主管理)
    MSG_RESPONSE_ROLLBACK_TASK,      //回退指令反馈(自主管理<-端口)

    MSG_QUERY_ROLLBACK_STATUS,        //接收回退查询结果状态查询(eth0->自主管理)
    MSG_RESPONSE_ROLLBACK_STATUS,      //回退查询结果查询反馈(自主管理<-端口)

    MSG_QUERY_REINITIATE_TASK,      //接收恢复出厂设置指令(eth0->自主管理)
    MSG_RESPONSE_REINITIATE_TASK,    //恢复出厂设置结果指令反馈(自主管理<-端口)

    MSG_QUERY_REINITIATE_STATUS,      //接收恢复出厂设置结果状态查询(eth0->自主管理)
    MSG_RESPONSE_REINITIATE_STATUS,    //恢复出厂设置结果状态查询(自主管理<-端口)
} MessageType;  //定义 主控接收的消息类型，作为 message的一个属性

// 消息来源类型
enum {
    ETH0_SOURCE = 1,
    ETH1_SOURCE = 2,
    CAN_SOURCE = 3
};

typedef struct {
    MessageType type; //自定义的主控接收的消息类型
    void* data;       //数据，传递的具体信息内容
    size_t data_size;  //数据大小
    int source;        //消息来源 //
} Message; //定义消息

// ================== 中控接收传递的data的定义 ==================
/////////////eth0给中控的  MSG_QUERY_SOFTWARE_VERSION 对应的 data 结构
/////软件版本查询数据结构 
typedef struct {
    int requestId;       // 请求ID
    char ruid[64];            // 网元RUID
} eth0_data_Query_SOFTWARE_VERSION; // eth0->中控


typedef struct {
    int requestId;       // 请求ID
    char ruid[64];       //uint8_t ruid;            // 网元RUID
    char currentVersion[64]；
    char backupVersionList[10][64];
    char errorInfo[256];    // 错误信息（若接口状态为失败，填入失败原因）
    //pthread_mutex_t*  response_mutex; // 用于同步的互斥锁
    //pthread_cond_t*  response_cond;   // 用于同步的条件变量
    bool response_ready;               // 响应就绪标志
} eth0_Query_SOFTWARE_VERSION_Context;  //中控-》etho0



// 软件更新请求数据结构
typedef struct {
    int requestId;       // 请求ID
    char ruid[64];            // 网元RUID
    char updateVersion[64];   // 目标更新版本
} eth0_data_Request_UPDATE_SOFTWARE; // eth0->中控

typedef struct {
    int requestId;       // 请求ID
    char errorInfo[256];    // 错误信息（若接口状态为失败，填入失败原因）
    bool response_ready;               // 响应就绪标志
} eth0_Request_UPDATE_SOFTWARE_Context;    // 中控-》etho0


//软件更新状态查询数据结构
typedef struct{
    int requestId;       // 请求ID
    char ruid[64];            // 网元RUID
} eth0_data_Query_UPDATE_SOFTWARE; // eth0->中控


typedef struct {
    int requestId;       // 请求ID
    char errorInfo[256];    // 错误信息（若接口状态为失败，填入失败原因）
    bool response_ready;               // 响应就绪标志
} eth0_Query_UPDATE_SOFTWARE_Context;    // 中控-》etho0


// 恢复出厂配置请求数据结构 
typedef struct {
    int requestId;       // 请求ID
    char ruid[64];            // 网元RUID
} eth0_data_Request_REINITIATE; // eth0->中控


typedef struct {
    int requestId;       // 请求ID
    char errorInfo[256];    // 错误信息（若接口状态为失败，填入失败原因）
    bool response_ready;               // 响应就绪标志
} eth0_Request_REINITIATE_Context;    // 中控-》etho0


//	恢复出厂配置状态查询数据结构 
typedef struct {
    int requestId;       // 请求ID
    char ruid[64];            // 网元RUID
} eth0_data_Query_REINITIATE; // eth0->中控


typedef struct {
    int requestId;       // 请求ID
    char errorInfo[256];    // 错误信息（若接口状态为失败，填入失败原因）
    char status[16];          // 状态（恢复成功success、恢复中updating、恢复失败fail）
    char additionalInfo[8];   //若重启状态为失败，填入失败原因
    bool response_ready;               // 响应就绪标志
} eth0_Query_REINITIATE_Context;   //  中控-》etho0


//回退请求数据结构
typedef struct {
    int requestId;       // 请求ID
    char ruid[64];            // 网元RUID
} eth0_data_Request_ROLLBACK; // eth0->中控

typedef struct {
    int requestId;       // 请求ID
    char errorInfo[256];    // 错误信息（若接口状态为失败，填入失败原因）
    bool response_ready;               // 响应就绪标志
} eth0_Request_ROLLBACK_Context;   //  中控-》etho0



//回退状态查询数据结构
typedef struct {
    int requestId;       // 请求ID
    char ruid[64];            // 网元RUID
} eth0_data_Query_ROLLBACK; // eth0->中控


typedef struct {
    int requestId;       // 请求ID
    char errorInfo[256];    // 错误信息（若接口状态为失败，填入失败原因）
    char status[16];          // 状态（恢复成功success、恢复中updating、恢复失败fail）
    char additionalInfo[8];   //若重启状态为失败，填入失败原因
    bool response_ready;               // 响应就绪标志
} eth0_Query_ROLLBACK_Context;   //  中控-》etho0



// ================== (自主管理->端口)指令请求数据结构 ==================


// 软件版本查询请求数据结构(自主管理->端口)
typedef struct {
    char requestId[64];       // 请求ID
    uint8_t ruid;            // 网元RUID
    char additionalInfo[8];  // 自定义信息 
    void* response;      // 指向中控响应数据的指针
    void* eth0_response;   // 指向eth0响应数据的指针       
    pthread_mutex_t* response_mutex;
    pthread_cond_t* response_cond;
} QueryVersionRequest_to_device;


// 软件回退设置请求数据结构(自主管理->端口)
typedef struct {
    char requestId[64];       // 请求ID
    uint8_t ruid;            // 网元RUID
    char additionalInfo[8];  // 自定义信息     
    void* response;      // 指向中控响应数据的指针
    void* eth0_response;   // 指向eth0响应数据的指针 
    pthread_mutex_t* response_mutex;
    pthread_cond_t* response_cond;    
} RollbackRequest_to_device;

// 软件回退结果查询请求数据结构(自主管理->端口)
typedef struct {
    char requestId[64];       // 请求ID
    uint8_t ruid;            // 网元RUID
    char additionalInfo[8];  // 自定义信息  
    void* response;      // 指向中控响应数据的指针
    void* eth0_response;   // 指向eth0响应数据的指针 
    pthread_mutex_t* response_mutex;
    pthread_cond_t* response_cond;       
} QueryRollbackRequest_to_device;




// 恢复出厂设置响应数据结构(端口->自主管理)
typedef struct {
    char requestId[64];       // 请求ID
    uint8_t ruid;            //网元RUID
    char additionalInfo[8];  // 自定义信息
    void* response;      // 指向中控响应数据的指针
    void* eth0_response;   // 指向eth0响应数据的指针 
    pthread_mutex_t* response_mutex;
    pthread_cond_t* response_cond;           
} ReinitiateRequest_to_device;

// 恢复出厂设置结果查询响应数据结构(端口->自主管理)
typedef struct {
    char requestId[64];       // 请求ID
    uint8_t ruid;             //网元RUID  
    char additionalInfo[8];  // 自定义信息
    void* response;      // 指向中控响应数据的指针
    void* eth0_response;   // 指向eth0响应数据的指针 
    pthread_mutex_t* response_mutex;
    pthread_cond_t* response_cond;              
} QueryReinitiateRequset_to_deivice;









// ================== (端口->自主管理)应答数据结构 ==================



// 软件版本查询响应数据结构(端口->自主管理)
typedef struct {
    char requestId[64];       // 请求指令ID
    char currentVersion[32];    // 当前软件版本号
    char lastVersion[32];  // 上一软件版本号
    int VersionCount; // 总共软件版本数量
} QueryVersionResponse_to_manage;

// 软件更新状态查询响应数据结构(端口->自主管理)
typedef struct {
    char requestId[64];       // 请求ID
    int status;          // 状态（）
} QueryUpdateRequset_to_manage;

// 软件回退设置响应数据结构(端口->自主管理)
typedef struct {
    char requestId[64];       // 请求ID
    int status;          // 状态（00H开始执行，11H不具备执行状态，FFH不支持该操作）
} RollbackResponse_to_manage;


// 软件回退结果查询响应数据结构(端口->自主管理)
typedef struct {
    char requestId[64];       // 请求ID
    int status;          // 状态（00H执行正常，11H执行错误，22H执行中，FFH不支持该操作）
} QueryRollbackResponse_to_manage;


// 恢复出厂设置响应数据结构(端口->自主管理)
typedef struct {
    char requestId[64];       // 请求ID
    int status;          // 状态（00H开始执行，11H不具备执行状态，FFH不支持该操作）
} ReinitiateResponse_to_manage;

// 恢复出厂设置结果查询响应数据结构(端口->自主管理)
typedef struct {
    char requestId[64];       // 请求ID
    int status;          // 状态（00H执行正常，11H执行错误，22H执行中，FFH不支持该操作）
} QueryReinitiateResponse_to_manage;










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

//////////////////////// 板卡已存储的各个模块使用的软件信息
typedef struct {
    char ruid[64];            // 网元RUID
    char currentVersion[64];      //// 当前版本
    char current_save_path[256];  //// 当前版本存储位置
    char status[16];             /// 当前版本更新状态
    char error_info[256];      // 错误信息
    char additional_info[256];

    char lastVersion[64];      //// 上一个版本
    char last_save_path[256];  //// 上一个版本存储位置
    int versionCount;         // 备份版本的实际数量
    char backupVersionList[10][64];  // 备份版本列表（包含上一个版本）
    char backup_save_path[10][256];  //// 备份版本存储位置
    char dl_Version[64];
    char dl_status[16];
} software_info;


// 软件更新任务定义
typedef struct {
    int transport_type;
    int dev_index;
    char filename[256];
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


// ================== 全局状态结构 ==================
typedef struct GlobalState {
    pthread_mutex_t ruid_list_mutex;
    pthread_mutex_t can0_task_mutex;
    pthread_cond_t can0_task_cond;
    pthread_mutex_t can1_task_mutex;
    pthread_cond_t can1_task_cond;
    pthread_mutex_t file_mutex;
    pthread_mutex_t dl_history_mutex;
    pthread_mutex_t update_history_mutex;




    atomic_int shutdown_requested;
    SystemConfig config;
    
    //RSS任务处理
    UpdateTask *rs422_current_task[5];    // RS422当前更新任务指针数组
    pthread_mutex_t rs422_task_mutex[5];  // RS422互斥锁数组
    pthread_cond_t rs422_task_cond[5];    // RS422条件变量数组
    int rs422_task_available[5];       // RS422任务可用标志数组

    // 新增eth1任务处理相关字段
    UpdateTask* eth1_current_task;    // ETH1当前更新任务指针
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

    // 任务状态跟踪表 - 用于记录所有任务的完整生命周期
    #define MAX_TRACKED_TASKS 100  // 最大跟踪任务数
    DownloadTask tracked_tasks[MAX_TRACKED_TASKS];  // 跟踪任务表
    int tracked_task_count;  // 当前跟踪任务数量
    pthread_mutex_t tracked_task_mutex;  // 跟踪任务表互斥锁
} GlobalState;

// ================== 全局变量声明 ==================
extern GlobalState global_state;
extern int cnt_ruid;
extern software_info  list_ruid[MAX_RUID_NUM];  // 最多16个ruid

// ================== 函数声明 ==================

void* eth0_service_thread(void* arg);
void* ftp_dl_worker(void* arg);

void* eth1_udp_thread(void* arg);
void* eth1_ftp_server_thread(void* arg);

void* can_service_thread(void* arg);

void* command_worker_thread(void* arg);
void* timer_service_thread(void* arg);
void register_timed_task(GlobalState* state, void (*task_func)(void*),
    void* arg, int interval);

int enqueue_message(GlobalState* state, Message* msg);////消息序列处理函数
int dequeue_message(GlobalState* state, Message* msg);////消息序列处理函数

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

void cleanup_expired_tokens(void* arg);
void signal_handler(int sig);
void cleanup_resources();




#endif  // COMMON_H
