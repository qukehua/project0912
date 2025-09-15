#include "../include/common.h"
#include "../include/rs422.h"
#include "../include/eth1.h"
#include <stdlib.h>  // 定义NULL
//#include "../src/eth0_1.c"
// ================== 全局变量定义 ==================
GlobalState global_state = {
    .shutdown_requested = ATOMIC_VAR_INIT(0),
    .config = {
        .eth0_ip = "192.168.1.100",
        .eth1_ip = "192.168.2.100",
        .can_interface = "can0",
        .rest_port = 443,
        .ftp_port = 21,
        .udp_port = 5000},
    .msg_queue_head = 0,
    .msg_queue_tail = 0,
    .eth1_task_available = 0 // eth1任务可用标志
};

int cnt_ruid;
software_info list_ruid[MAX_RUID_NUM]; // 存储多个网元的信息数组


// ================== 主函数 ==================
int main()
{
    // 初始化互斥锁和条件变量
    memset(&global_state, 0, sizeof(GlobalState));
    cnt_ruid = 0;                                            // 初始化 RUID 计数器
    pthread_mutex_init(&global_state.ruid_list_mutex, NULL); // 新增互斥锁
    pthread_mutex_init(&global_state.queue_mutex, NULL);
    pthread_cond_init(&global_state.queue_cond, NULL);
    pthread_mutex_init(&global_state.dl_task_mutex, NULL);
    pthread_cond_init(&global_state.dl_task_cond, NULL);
    pthread_mutex_init(&global_state.update_task_mutex, NULL);
    pthread_mutex_init(&global_state.timed_task_mutex, NULL);
    pthread_mutex_init(&global_state.blacklist_mutex, NULL);
    pthread_mutex_init(&global_state.tracked_task_mutex, NULL);
    global_state.tracked_task_count = 0;
    pthread_mutex_init(&global_state.can0_task_mutex, NULL);
    pthread_cond_init(&global_state.can0_task_cond, NULL);
    pthread_mutex_init(&global_state.can1_task_mutex, NULL);
    pthread_cond_init(&global_state.can1_task_cond, NULL);
    pthread_mutex_init(&global_state.eth1_task_mutex, NULL);
    pthread_cond_init(&global_state.eth1_task_cond, NULL);
    for(int i = 0; i < 5; i++) {
    // pthread_mutex_init(&global_state.rs422_task_mutex[i], NULL);
    // pthread_cond_init(&global_state.rs422_task_cond[i], NULL);
}


    // 初始化RS422模块
    // rs422_init(&global_state);

    // 启动eth0接口线程
    pthread_t eth0_tid;
    printf("[DEBUG] 准备启动eth0接口线程 (HTTPS服务器)...\n");
    pthread_create(&eth0_tid, NULL, eth0_service_thread, NULL);
    printf("[DEBUG] eth0接口线程已创建\n");

    // 启动FTP下载线程
    pthread_t ftp_dl_tid;
    printf("[DEBUG] 准备启动FTP下载线程...\n");
    pthread_create(&ftp_dl_tid, NULL, ftp_dl_worker, &global_state);
    printf("[DEBUG] FTP下载线程已创建\n");

    // 创建定时器服务线程
    pthread_t timer_tid;
    printf("[DEBUG] 准备创建定时器服务线程...\n");
    pthread_create(&timer_tid, NULL, timer_service_thread, &global_state);
    printf("[DEBUG] 定时器服务线程已创建\n");
    // 注册Token清理任务（每3600秒执行一次）
    register_timed_task(&global_state, cleanup_expired_tokens, NULL, 3600);
    printf("[DEBUG] 已注册Token清理任务（每3600秒执行一次）\n");
    // 示例：注册其他定时任务（每10秒执行一次）
    // register_timed_task(&global_state, other_task_func, some_arg, 10);

    // 创建模拟处理线程（专门处理 eth0 指令）
    pthread_t command_worker_tid;
    printf("[DEBUG] 准备创建模拟处理线程...\n");
    pthread_create(&command_worker_tid, NULL, command_worker_thread, NULL);
    printf("[DEBUG] 模拟处理线程已创建\n");


    //  ================== 创建CAN服务线程 ==================
    // 注意：can_service_thread函数当前没有实现，已注释掉
    // pthread_t can0_tid, can1_tid;
    // 创建CAN0 线程
    // pthread_create(&can0_tid, NULL, can_service_thread, NULL);
    // 创建CAN1 线程
    // pthread_create(&can1_tid, NULL, can_service_thread, NULL);



    //  ================== 创建ETH1服务线程 ==================
    pthread_t eth1_udp_tid, eth1_ftp_tid;
    // 创建ETH1 UDP线程
    pthread_create(&eth1_udp_tid, NULL, eth1_service_thread, NULL) ;
    // 创建ETH1 FTP线程
    pthread_create(&eth1_ftp_tid, NULL, eth1_service_thread, NULL) ;



    // //  ================== 创建RS422服务线程 ==================
    // pthread_t rs422_tid_0, rs422_tid_1, rs422_tid_2, rs422_tid_3, rs422_tid_4;
    // // 创建RS422_DEV_0线程
    // pthread_create(&rs422_tid_0, NULL, rs422_service_thread, NULL) ;
    // // 创建RS422_DEV_1线程
    // pthread_create(&rs422_tid_1, NULL, rs422_service_thread, NULL) ;
    // // 创建RS422_DEV_2线程
    // pthread_create(&rs422_tid_2, NULL, rs422_service_thread, NULL) ;
    // // 创建RS422_DEV_3线程
    // pthread_create(&rs422_tid_3, NULL, rs422_service_thread, NULL) ;
    // // 创建RS422_DEV_4线程
    // pthread_create(&rs422_tid_4, NULL, rs422_service_thread, NULL) ;

    // 设置信号处理
    signal(SIGINT, signal_handler);


    // 主循环
    while (!atomic_load(&global_state.shutdown_requested))
    {
        sleep(1);
    }

    // 等待线程结束
    pthread_join(eth0_tid, NULL);
    pthread_join(ftp_dl_tid, NULL);
    pthread_join(timer_tid, NULL);
    pthread_join(command_worker_tid, NULL);
    // 注意：can_service_thread函数当前没有实现，已注释掉
    // pthread_join(can0_tid, NULL);
    // pthread_join(can1_tid, NULL);
    pthread_join(eth1_udp_tid, NULL);
    pthread_join(eth1_ftp_tid, NULL);
    // pthread_join(rs422_tid_0, NULL);
    // pthread_join(rs422_tid_1, NULL);
    // pthread_join(rs422_tid_2, NULL);
    // pthread_join(rs422_tid_3, NULL);
    // pthread_join(rs422_tid_4, NULL);


    // 清理资源 - 已在eth0.c中实现
    // cleanup_resources();
    printf("System shutdown gracefully\n");
    return 0;
}




//======================================================================




void *command_worker_thread(void *arg)
{
    while (!atomic_load(&global_state.shutdown_requested))
    {
        Message msg;
        if (dequeue_message(&global_state, &msg))
        {
            switch (msg.type)
            {
                // 软件更新任务
                case MSG_UPDATE_TASK:
                {
                    UpdateTask *task = (UpdateTask *)msg.data;
                    printf("[CMD] 收到更新任务: %s\n", task->requestId);


                    // 软件更新任务转发给eth1或RS422
                    // if (task->updateType == UPDATE_TYPE_SOFTWARE)
                    // {
 
                    //     for (int i = 0; i < cnt_ruid; i++) {
                    //         // 比较网元ruid                           
                    //         // 选择RSS接口进行任务分发
                    //         if (task->ruid[i] == rs422_tasks[task->dev_index]) { 
                    //             pthread_mutex_lock(&global_state.rs422_task_mutex[task->dev_index]);
                    //             global_state.rs422_current_task[task->dev_index] = task;
                    //             global_state.rs422_task_available[task->dev_index] = 1;
                    //             pthread_cond_signal(&global_state.rs422_task_cond[task->dev_index]);
                    //             pthread_mutex_unlock(&global_state.rs422_task_mutex[task->dev_index]);
                    //             printf("[CMD] 已转发任务到RS422-%d: %s\n", task->dev_index, task->filename);
                    //             strncpy(task->status, "sending", sizeof(task->status));
                    //         }
                    //         // 通过ETH1发送
                    //         else if (task->ruid[i] == eth1){
                    //             pthread_mutex_lock(&global_state.eth1_task_mutex);
                    //             global_state.eth1_current_task = task;
                    //             global_state.eth1_task_available = 1;
                    //             pthread_cond_signal(&global_state.eth1_task_cond);
                    //             pthread_mutex_unlock(&global_state.eth1_task_mutex);
                    //             printf("[CMD] 已转发任务到ETH1: %s\n", task->ruid);
                    //         }
                    //         else
                    //         {
                    //             printf("[CMD] 未知的RUID，无法转发任务: %s\n", task->ruid);
                    //             strncpy(task->status, "failed", sizeof(task->status));
                    //             return NULL;
                    //         }
                    //     }
                    //     break;
                    // }
                }    
                //软件版本查询请求
                case MSG_QUERY_SOFTWARE_VERSION:
                {
                    eth0_data_Query_SOFTWARE_VERSION* request = (eth0_data_Query_SOFTWARE_VERSION*)msg.data;
                    printf("[CMD] 收到软件版本查询请求: %s\n", request->requestId);
                    
                    //pthread_mutex_lock(request->response_mutex);

   
                    // for (int i = 0; i < cnt_ruid; i++) {
                    //     // 比较网元ruid
                    //     if (request->ruid[i] == eth1) {
                    //         // 发送ETH1命令并等待响应
                    //         pthread_mutex_lock(&global_state.eth1_task_mutex);
                    //         global_state.eth1_current_task = task;
                    //         global_state.eth1_task_available = 1;
                    //         pthread_cond_signal(&global_state.eth1_task_cond);
                    //         pthread_mutex_unlock(&global_state.eth1_task_mutex);
                    //         printf("[CMD] 已转发软件版本查询请求到ETH1: %s\n", task->ruid);
                    //     }
                    //     else{
                    //             printf("[CMD] 未知的RUID，无法转发任务: %s\n", task->ruid);
                    //             strncpy(task->status, "failed", sizeof(task->status));
                    //             return NULL;
                    //         }                       
                    //     break;
                    // }    
                    ctx.response_ready = true;  // 设置响应就绪
                    pthread_cond_signal(&response_cond);  // 通知等待线程
                    break;
                }        
                //软件版本查询响应
                case MSG_RESPONSE_SOFTWARE_VERSION:
                {   
                    //成功响应
                    QueryVersionResponse_to_manage* response = (QueryVersionResponse_to_manage*)msg.data;
                    printf("[CMD] 收到软件版本查询响应: %s\n", response->requestId);
   
                    
                    // 假设从ETH1响应中获取版本信息
                    // 当前版本
                    printf("当前版本: %s\n", response->currentVersion);

                    //上一版本
                    printf("当前版本: %s\n", response->lastVersion);

                    //版本数量
                    printf("版本数量: %d\n", response->VersionCount);
                    
                    // 构造JSON响应
                    json_t* resp_json = json_object();
                    json_object_set_new(resp_json, "requestId", json_string(response->requestId));
                    json_object_set_new(resp_json, "currentVersion", json_string(response->currentVersion));
                    json_object_set_new(resp_json, "lastVersion", json_string(response->lastVersion));
                    json_object_set_new(resp_json, "versionCount", json_integer(response->VersionCount));
                    
                    // 转换为字符串并存储
                    char* json_str = json_dumps(resp_json, JSON_COMPACT);
                    if (json_str) {
                        printf("[CMD] Version query response: %s\n", json_str);
                        free(json_str);
                    }
                    
                    // 释放JSON对象
                    json_decref(resp_json);

                    // 通知ETH0线程响应已准备好
                    pthread_cond_signal(request->response_cond);
                    break;
                }
 
                   
                    

                            
         
                // 软件回退状态查询
                case MSG_QUERY_ROLLBACK_STATUS:
                {
                    eth0_data_Query_ROLLBACK* request = (eth0_data_Query_ROLLBACK*)msg.data;
                    printf("[CMD] 收到软件回退状态查询请求: %s\n", request->requestId);



                }


                // 软件回退状态响应
                case MSG_RESPONSE_ROLLBACK_STATUS:
                {
                    //成功响应
                    QueryVersionResponse_to_manage* response = (QueryVersionResponse_to_manage*)msg.data;
                    printf("[CMD] 收到软件回退状态响应响应: %s\n", response->requestId);



                }
              

                // 恢复出厂配置状态查询
                case MSG_QUERY_REINITIATE_STATUS:{
                    eth0_data_Query_REINITIATE* request = (eth0_data_Query_REINITIATE*)msg.data;
                    printf("[CMD] 收到恢复出厂配置状态查询请求: %s\n", request->requestId);

                }


                // 恢复出厂配置状态响应
    
                case MSG_RESPONSE_REINITIATE_STATUS:{
                    QueryReinitiateResponse_to_manage* response = (QueryReinitiateResponse_to_manage*)msg.data;
                    printf("[CMD] 收到恢复出厂配置状态响应: %s\n", request->requestId);



                }
            }   
        }
    }
}     

// ================== 消息队列操作 ==================
int enqueue_message(GlobalState* state,Message *msg)
{
    pthread_mutex_lock(&global_state.queue_mutex);

    int next = (global_state.msg_queue_tail + 1) % MESSAGE_QUEUE_SIZE;
    if (next == global_state.msg_queue_head)
    {
        pthread_mutex_unlock(&global_state.queue_mutex);
        return 0; // 队列满
    }

    // 复制数据
    if (msg->data_size > 0)
    {
        void *data_copy = malloc(msg->data_size);
        if (!data_copy)
        {
            pthread_mutex_unlock(&global_state.queue_mutex);
            return 0;
        }
        memcpy(data_copy, msg->data, msg->data_size);
        global_state.msg_queue[global_state.msg_queue_tail].data = data_copy;
    }
    else
    {
        global_state.msg_queue[global_state.msg_queue_tail].data = NULL;
    }

    // 设置消息属性
    global_state.msg_queue[global_state.msg_queue_tail].type = msg->type;
    global_state.msg_queue[global_state.msg_queue_tail].data_size = msg->data_size;
    global_state.msg_queue[global_state.msg_queue_tail].source = msg->source;

    global_state.msg_queue_tail = next;

    pthread_cond_signal(&global_state.queue_cond);
    pthread_mutex_unlock(&global_state.queue_mutex);
    return 1;
}

int dequeue_message(GlobalState* state,Message *msg)
{
    pthread_mutex_lock(&global_state.queue_mutex);

    while (global_state.msg_queue_head == global_state.msg_queue_tail)
    {
        pthread_cond_wait(&global_state.queue_cond, &global_state.queue_mutex);

        if (atomic_load(&global_state.shutdown_requested))
        {
            pthread_mutex_unlock(&global_state.queue_mutex);
            return 0;
        }
    }

    *msg = global_state.msg_queue[global_state.msg_queue_head];
    global_state.msg_queue_head = (global_state.msg_queue_head + 1) % MESSAGE_QUEUE_SIZE;

    pthread_mutex_unlock(&global_state.queue_mutex);
    return 1;
}

// ================== 信号处理 ==================
// 注意：signal_handler函数已在eth0.c中实现
// void signal_handler(int sig)
// {
//     atomic_store(&global_state.shutdown_requested, 1);
// }

// ================== 资源清理 ==================
// 注意：cleanup_resources函数已在eth0.c中实现
// void cleanup_resources()
// {
//     // 清理黑名单
//     pthread_mutex_lock(&global_state.blacklist_mutex);
//     for (int i = 0; i < MAX_BLACKLIST_TOKENS; i++)
//     {
//         if (global_state.token_blacklist[i] != NULL)
//         {
//             free(global_state.token_blacklist[i]->token);
//             free(global_state.token_blacklist[i]);
//             global_state.token_blacklist[i] = NULL;
//         }
//     }
//     pthread_mutex_unlock(&global_state.blacklist_mutex);

//     // 销毁互斥锁和条件变量
//     pthread_mutex_destroy(&global_state.queue_mutex);
//     pthread_cond_destroy(&global_state.queue_cond);
//     pthread_mutex_destroy(&global_state.file_mutex);
//     pthread_mutex_destroy(&global_state.dl_task_mutex);
//     pthread_cond_destroy(&global_state.dl_task_cond);
//     pthread_mutex_destroy(&global_state.dl_history_mutex);
//     pthread_mutex_destroy(&global_state.update_task_mutex);
//     pthread_mutex_destroy(&global_state.update_history_mutex);
//     pthread_mutex_destroy(&global_state.blacklist_mutex);
// }

// ================== 通用定时器线程 ==================
void *timer_service_thread(void *arg)
{
    GlobalState *state = (GlobalState *)arg;

    while (!atomic_load(&state->shutdown_requested))
    {
        // 1秒定时
        sleep(1);

        pthread_mutex_lock(&state->timed_task_mutex);
        time_t current_time = time(NULL);

        // 遍历所有定时任务
        for (int i = 0; i < state->timed_task_count; i++)
        {
            TimedTask *task = &state->timed_tasks[i];

            // 检查是否到达执行时间
            if (difftime(current_time, task->last_executed) >= task->interval)
            {
                // 执行任务
                task->task_func(task->arg);
                task->last_executed = current_time;
            }
        }
        pthread_mutex_unlock(&state->timed_task_mutex);
    }
    return NULL;
}

// ================== 添加定时任务注册函数 ==================
void register_timed_task(GlobalState *state, void (*task_func)(void *),
                         void *arg, int interval)
{
    pthread_mutex_lock(&state->timed_task_mutex);

    if (state->timed_task_count < MAX_TIMED_TASKS)
    {
        TimedTask *task = &state->timed_tasks[state->timed_task_count++];
        task->task_func = task_func;
        task->arg = arg;
        task->interval = interval;
        task->last_executed = time(NULL);
    }

    pthread_mutex_unlock(&state->timed_task_mutex);
}


// ================== 辅助函数 ==================
static int save_system_state(void)
{
    // 保存系统状态的实现
    return 0;
}

static void stop_all_active_tasks(void)
{
    // 停止所有活动任务的实现
    pthread_mutex_lock(&global_state.eth1_task_mutex);
    global_state.eth1_task_available = 0;
    pthread_mutex_unlock(&global_state.eth1_task_mutex);
    
    // 停止其他任务...
}

static int reset_all_devices(void)
{
    // 重置所有设备连接的实现
    return 0;
}

static int reinitialize_system_config(void)
{
    // 重新初始化系统配置的实现
    return 0;
}

static int check_rollback_version(void)
{
    // 检查是否存在可回滚版本的实现
    return 1;
}

static void stop_current_services(void)
{
    // 停止当前运行服务的实现
}

static void backup_current_config(void)
{
    // 备份当前配置的实现
}

static int perform_system_rollback(void)
{
    // 执行系统回滚的实现
    return 0;
}

static void restart_system_services(void)
{
    // 重启系统服务的实现
}

static void restore_backup_config(void)
{
    // 恢复备份配置的实现
}
