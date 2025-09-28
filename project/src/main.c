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



static void on_eth1_msg(void* user, const Eth1Msg* msg) {
  switch (msg->type) {
    case ETH1_MSG_ACK:
      // 这里把 msg->u.ack.payload.buf/len 转给 REST/上层
      break;
    case ETH1_MSG_PROGRESS:
      // 进度上报（FTP=stage1，分片直传=stage2）
      break;
    case ETH1_MSG_DONE:
      // status==0 表示成功；last_resp 带最后一次应答的原始参数字节
      break;
    default: break;
  }
}


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



int app_start(void) {
     //  ================== 创建ETH1服务线程 ==================
    printf("Initializing ETH1 module...\n");
    if (eth1_init(on_eth1_msg, NULL) != 0) {
        printf("ETH1 init failed\n");
        return -1;
    }
    printf("ETH1 module initialized successfully.\n");

    //  ================== 创建RS422服务线程 ==================
    printf("Initializing RS422 module...\n");
    if (rs422_init(rs422_message_handler, NULL) != 0) {
        printf("RS422 init failed\n");
        return -1;
    }
    printf("RS422 module initialized successfully.\n");
    
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

    return 0;
}


void app_stop(void){
    //关闭线程
    
    pthread_join(eth0_tid, NULL);
    pthread_join(ftp_dl_tid, NULL);
    pthread_join(timer_tid, NULL);
    pthread_join(command_worker_tid, NULL);
    // 注意：can_service_thread函数当前没有实现，已注释掉
    // pthread_join(can0_tid, NULL);
    // pthread_join(can1_tid, NULL);

    //关闭eth1线程
    eth1_shutdown();
    //关闭rs422线程
    rs422_shutdown();


    // 清理资源 - 已在eth0.c中实现
    // cleanup_resources();
}
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

    app_start();
    printf("System initialized successfully\n");

    // 设置信号处理
    signal(SIGINT, signal_handler);


    // 主循环
    while (!atomic_load(&global_state.shutdown_requested))
    {
        sleep(1);
    }

    app_stop();
    printf("System shutdown gracefully\n");
    return 0;
    // 等待线程结束

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
                     for (int i = 0; i < cnt_ruid; i++) {
                        if (request->ruid[i] == NIXYK_DEVID_BASEBAND) { //eth1设备标识符
                            // 创建ETH1任务
                            Eth1Task* eth1_task = malloc(sizeof(Eth1Task));
                            eth1_task->type = ETH1_TASK_RECONFIG_EXECUTE;
                            eth1_task->task_id = strdup(request->requestId);
                            eth1_task.u.reconfig.target.ruid = NIXYK_DEVID_BASEBAND;      // 目标设备
                            eth1_task.u.reconfig.policy = CTRL_POLICY_DEFAULT();
                            eth1_task.u.reconfig.xfer.method = FT_UDP_CTRL_FTP;
                            eth1_task.u.reconfig.xfer.file_path = "/opt/pkg/update.bin";
                            eth1_task.u.reconfig.xfer.ftp.user = "u";     // 按需填写
                            eth1_task.u.reconfig.xfer.ftp.pass = "p";
                            eth1_task.u.reconfig.xfer.ftp.url  = "ftp://server/path/update.bin";
                            eth1_task.u.reconfig.xfer.ftp.file_type = 0xFE;  // 
                            eth1_task.u.reconfig.xfer.ftp.sub_type  = 0x01;
                            eth1_task.u.reconfig.xfer.ftp.op_type   = 0x11;  // 写入
                            eth1_task.u.reconfig.cmd_prepare_req = /* 2-1 指令码 */;
                            eth1_task.u.reconfig.cmd_prepare_ack = /* 2-2 应答码 */;
                            eth1_task.u.reconfig.cmd_start_req   = /* 2-3 指令码 (无需等待ACK) */;

                            
                            
                            // 将任务入队给ETH1线程处理
                            int result = eth1_enqueue_task(eth1_task);
                            if (result != 0) {
                                printf("[ERROR] Failed to enqueue version query task: %d\n", result);
                                free(eth1_task->requestId);
                                free(eth1_task);
                                return NULL;
                            }
                            
                            printf("[CMD] 已将软件版本查询任务发送给ETH1: %s\n", request->requestId);
                            break;
                        }
                         if (request->ruid[i] == NIXYK_DEVID_ISL_1_MGMT) { //rs422设备标识符
                            // 创建RS422任务
                            Rs422Task* rs422_task = malloc(sizeof(Rs422Task));
                            rs422_task->type = RS422_TASK_SEND_DATA;
                            rs422_task->task_id = strdup(request->requestId);

                            
                            // 将任务入队给RS422线程处理
                            int result = rs422_enqueue_task(rs422_task);
                            if (result != 0) {
                                printf("[ERROR] Failed to enqueue RS422 send task: %d\n", result);
                                free(rs422_task->task_id);
                                free(rs422_task);
                                return NULL;
                            }
                            
                            printf("[CMD] 已将软件版本查询任务发送给RS422: %s\n", request->requestId);
                            break;
                        }
                    }
                    break;
                }    
                //软件版本查询请求
                case MSG_QUERY_SOFTWARE_VERSION:
                {
                    QueryVersionRequest_to_device* request = (QueryVersionRequest_to_device*)msg.data;
                    printf("[CMD] 收到软件版本查询请求: %s\n", request->requestId);
                    
                    for (int i = 0; i < cnt_ruid; i++) {
                        if (request->ruid[i] == NIXYK_DEVID_BASEBAND) { //eth1设备标识符
                            // 创建ETH1任务
                            Eth1Task* eth1_task = malloc(sizeof(Eth1Task));
                            eth1_task->type = ETH1_TASK_SWVER_QUERY;
                            eth1_task->task_id = strdup(request->requestId);
                            eth1_task->target.ruid = NIXYK_DEVID_BASEBAND;
                            eth1_task->policy = CTRL_POLICY_DEFAULT(); 
                            
                            // 将任务入队给ETH1线程处理
                            int result = eth1_enqueue_task(eth1_task);
                            if (result != 0) {
                                printf("[ERROR] Failed to enqueue version query task: %d\n", result);
                                free(eth1_task->requestId);
                                free(eth1_task);
                                return NULL;
                            }
                            
                            printf("[CMD] 已将软件版本查询任务发送给ETH1: %s\n", request->requestId);
                            break;
                        }
                        if (request->ruid[i] == NIXYK_DEVID_ISL_1_MGMT) { //rs422设备标识符
                            // 创建RS422任务
                            Rs422Task* rs422_task = malloc(sizeof(Rs422Task));
                            rs422_task->type = RS422_TASK_SEND_DATA;
                            rs422_task->task_id = strdup(request->requestId);

                            
                            // 将任务入队给RS422线程处理
                            int result = rs422_enqueue_task(rs422_task);
                            if (result != 0) {
                                printf("[ERROR] Failed to enqueue RS422 send task: %d\n", result);
                                free(rs422_task->task_id);
                                free(rs422_task);
                                return NULL;
                            }
                            
                            printf("[CMD] 已将软件版本查询任务发送给RS422: %s\n", request->requestId);
                            break;
                        }
                    break;
                    }
                }    
                  
                //软件版本查询响应
                case MSG_RESPONSE_SOFTWARE_VERSION:
                {   
                    //成功响应
                    g_version_ctx* response = (g_version_ctx*)msg.data;
                    printf("[CMD] 收到软件版本查询响应: %s\n", response->requestId);


                    // 通知ETH0线程响应已准备好
                    response->response_ready = true;  // 设置响应就绪
                    pthread_cond_signal(response->original_request->response_cond);  // 通知等待线程
                    printf("[Main] 已设置响应就绪，通知eth0线程\n");
                    break;
                }
 
                //  接收回退指令
                case MSG_QUERY_ROLLBACK_TASK: {
                    eth0_data_Request_ROLLBACK* request = (eth0_data_Request_ROLLBACK*)msg.data;
                    printf("[CMD] 收到软件回退指令: %s\n", request->requestId);
                    for (int i = 0; i < cnt_ruid; i++) {
                        if (request->ruid[i] == NIXYK_DEVID_BASEBAND) { //eth1设备标识符
                            // 创建ETH1任务
                            Eth1Task* eth1_task = malloc(sizeof(Eth1Task));
                            eth1_task->type = ETH1_TASK_ROLLBACK_SET;
                            eth1_task->task_id = strdup(request->requestId); 
                            eth1_task->target.ruid = NIXYK_DEVID_BASEBAND;
                            eth1_task->policy = CTRL_POLICY_DEFAULT();       

                            
                            // 将任务入队给ETH1线程处理
                            int result = eth1_enqueue_task(eth1_task);
                            if (result != 0) {
                                printf("[ERROR] Failed to enqueue ROLLBACK task: %d\n", result);
                                free(eth1_task->requestId);
                                free(eth1_task);
                                return NULL;
                            }
                            
                            printf("[CMD] 已将软件回退指令发送给ETH1: %s\n", request->requestId);
                            break;
                        }
                        if (request->ruid[i] == NIXYK_DEVID_ISL_1_MGMT) { //rs422设备标识符
                            // 创建RS422任务
                            Rs422Task* rs422_task = malloc(sizeof(Rs422Task));
                            rs422_task->type = RS422_TASK_SEND_DATA;
                            rs422_task->task_id = strdup(request->requestId);

                            
                            // 将任务入队给RS422线程处理
                            int result = rs422_enqueue_task(rs422_task);
                            if (result != 0) {
                                printf("[ERROR] Failed to enqueue RS422 send task: %d\n", result);
                                free(rs422_task->task_id);
                                free(rs422_task);
                                return NULL;
                            }
                            
                            printf("[CMD] 已将软件版本查询任务发送给RS422: %s\n", request->requestId);
                            break;
                        }
                    }
                }

                
                //  回退指令反馈
                case MSG_RESPONSE_ROLLBACK_TASK: {
                    eth0_Request_ROLLBACK_Context* response = (eth0_Request_ROLLBACK_Context*)msg.data;
                    printf("[CMD] 收到软件回退指令反馈: %s\n", response->requestId);
                    response->response_ready = true;  // 设置响应就绪
                    pthread_cond_signal(response->original_request->response_cond);  // 通知等待线程
                    printf("[Main] 已设置响应就绪，通知eth0线程\n");
                    break;
                }



         
                // 软件回退状态查询请求
                case MSG_QUERY_ROLLBACK_STATUS:
                {
                    eth0_data_Query_ROLLBACK* request = (eth0_data_Query_ROLLBACK*)msg.data;

                    printf("[CMD] 收到软件回退状态查询请求: %s\n", request->requestId);
                    for (int i = 0; i < cnt_ruid; i++) {
                        if (request->ruid[i] == NIXYK_DEVID_BASEBAND) { //eth1设备标识符
                            // 创建ETH1任务
                            Eth1Task* eth1_task = malloc(sizeof(Eth1Task));
                            eth1_task->type = ETH1_TASK_ROLLBACK_QUERY;
                            eth1_task->task_id = strdup(request->requestId);
                            eth1_task->target.ruid = NIXYK_DEVID_BASEBAND;
                            eth1_task->policy = CTRL_POLICY_DEFAULT(); 
                            
                            // 将任务入队给ETH1线程处理
                            int result = eth1_enqueue_task(eth1_task);
                            if (result != 0) {
                                printf("[ERROR] Failed to enqueue ROLLBACK STATUS task: %d\n", result);
                                free(eth1_task->requestId);
                                free(eth1_task);
                                return NULL;
                            }
                            
                            printf("[CMD] 已将软件回退状态查询请求发送给ETH1: %s\n", request->requestId);
                            break;
                        }
                        if (request->ruid[i] == NIXYK_DEVID_ISL_1_MGMT) { //rs422设备标识符
                                // 创建RS422任务
                            Rs422Task* rs422_task = malloc(sizeof(Rs422Task));
                            rs422_task->type = RS422_TASK_SEND_DATA;
                            rs422_task->task_id = strdup(request->requestId);

                            
                            // 将任务入队给RS422线程处理
                            int result = rs422_enqueue_task(rs422_task);
                            if (result != 0) {
                                printf("[ERROR] Failed to enqueue RS422 send task: %d\n", result);
                                free(rs422_task->task_id);
                                free(rs422_task);
                                return NULL;
                            }
                            
                            printf("[CMD] 已将软件版本查询任务发送给RS422: %s\n", request->requestId);
                            break;
                        }
                    }
                     break;
                }    

                // 软件回退状态查询响应
                case MSG_RESPONSE_ROLLBACK_STATUS:
                {
                    //成功响应
                    eth0_Query_ROLLBACK_Context* response = (eth0_Query_ROLLBACK_Context*)msg.data;
                    printf("[CMD] 收到软件回退状态响应响应: %s\n", response->requestId);
                     // 假设从ETH1响应中获取版本信息                    
                    response->response_ready = true;  // 设置响应就绪
                    pthread_cond_signal(response->original_request->response_cond);  // 通知等待线程
                    printf("[Main] 已设置响应就绪，通知eth0线程\n");
                    break;       


                }
                
               // 接收恢复出厂设置指令
                case MSG_QUERY_REINITIATE_TASK:{
                    eth0_data_Request_REINITIATE* request = (eth0_data_Request_REINITIATE*)msg.data;
                    printf("[CMD] 收到恢复出厂配置指令: %s\n", request->requestId);
                    for (int i = 0; i < cnt_ruid; i++) {
                        if (request->ruid[i] == NIXYK_DEVID_BASEBAND) { //eth1设备标识符
                            // 创建ETH1任务
                            Eth1Task* eth1_task = malloc(sizeof(Eth1Task));
                            eth1_task->type = ETH1_TASK_FACTORY_RESET_NOTIFY;
                            eth1_task->task_id = strdup(request->requestId);
                            eth1_task->target.ruid = NIXYK_DEVID_BASEBAND;
                            eth1_task->policy = CTRL_POLICY_DEFAULT(); 
                            
                            // 将任务入队给ETH1线程处理
                            int result = eth1_enqueue_task(eth1_task);
                            if (result != 0) {
                                printf("[ERROR] Failed to enqueue REINITIATE task: %d\n", result);
                                free(eth1_task->requestId);
                                free(eth1_task);
                                return NULL;
                            }
                            
                            printf("[CMD] 已将恢复出厂配置指令发送给ETH1: %s\n", request->requestId);
                            break;
                        }
                        if (request->ruid[i] == NIXYK_DEVID_ISL_1_MGMT) { //rs422设备标识符
                            // 创建RS422任务
                            Rs422Task* rs422_task = malloc(sizeof(Rs422Task));
                            rs422_task->type = RS422_TASK_SEND_DATA;
                            rs422_task->task_id = strdup(request->requestId);

                            
                            // 将任务入队给RS422线程处理
                            int result = rs422_enqueue_task(rs422_task);
                            if (result != 0) {
                                printf("[ERROR] Failed to enqueue RS422 send task: %d\n", result);
                                free(rs422_task->task_id);
                                free(rs422_task);
                                return NULL;
                            }
                            
                            printf("[CMD] 已将软件版本查询任务发送给RS422: %s\n", request->requestId);
                            break;
                        }
                    }
                        break;
                }   
                
                // 恢复出厂配置指令反馈
                case MSG_RESPONSE_REINITIATE_TASK:{
                    eth0_Request_REINITIATE_Context* response = (eth0_Request_REINITIATE_Context*)msg.data;
                    printf("[CMD] 收到恢复出厂配置指令反馈: %s\n", response->requestId);
                    response->response_ready = true;  // 设置响应就绪
                    pthread_cond_signal(response->original_request->response_cond);  // 通知等待线程
                    printf("[Main] 已设置响应就绪，通知eth0线程\n");
                    break;     
                }


                // 恢复出厂配置状态查询请求
                case MSG_QUERY_REINITIATE_STATUS:{
                    eth0_data_Query_REINITIATE* request = (eth0_data_Query_REINITIATE*)msg.data;
                    printf("[CMD] 收到恢复出厂配置状态查询请求: %s\n", request->requestId);
                    for (int i = 0; i < cnt_ruid; i++) {
                        if (request->ruid[i] == NIXYK_DEVID_BASEBAND) { //eth1设备标识符
                            // 创建ETH1任务
                            Eth1Task* eth1_task = malloc(sizeof(Eth1Task));
                            eth1_task->type = ETH1_TASK_FACTORY_RESET_QUERY;
                            eth1_task->task_id = strdup(request->requestId);
                            eth1_task->target.ruid = NIXYK_DEVID_BASEBAND;
                            eth1_task->policy = CTRL_POLICY_DEFAULT(); 
                            
                            // 将任务入队给ETH1线程处理
                            int result = eth1_enqueue_task(eth1_task);
                            if (result != 0) {
                                printf("[ERROR] Failed to enqueue REINITIATE STATUS task: %d\n", result);
                                free(eth1_task->requestId);
                                free(eth1_task);
                                return NULL;
                            }
                            
                            printf("[CMD] 已将恢复出厂配置状态查询请求发送给ETH1: %s\n", request->requestId);
                            break;
                        }
                        if (request->ruid[i] == NIXYK_DEVID_ISL_1_MGMT) { //rs422设备标识符
                            // 创建RS422任务
                            Rs422Task* rs422_task = malloc(sizeof(Rs422Task));
                            rs422_task->type = RS422_TASK_SEND_DATA;
                            rs422_task->task_id = strdup(request->requestId);

                            
                            // 将任务入队给RS422线程处理
                            int result = rs422_enqueue_task(rs422_task);
                            if (result != 0) {
                                printf("[ERROR] Failed to enqueue RS422 send task: %d\n", result);
                                free(rs422_task->task_id);
                                free(rs422_task);
                                return NULL;
                            }
                            
                            printf("[CMD] 已将软件版本查询任务发送给RS422: %s\n", request->requestId);
                            break;
                        }

                    }
                     break;
                }   


                // 恢复出厂配置状态查询响应
    
                case MSG_RESPONSE_REINITIATE_STATUS:{
                    eth0_Query_REINITIATE_Context* response = (eth0_Query_REINITIATE_Context*)msg.data;
                    printf("[CMD] 收到恢复出厂配置状态响应: %s\n", request->requestId);
                    response->response_ready = true;  // 设置响应就绪
                    pthread_cond_signal(response->original_request->response_cond);  // 通知等待线程
                    printf("[Main] 已设置响应就绪，通知eth0线程\n");
                    break;     


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


