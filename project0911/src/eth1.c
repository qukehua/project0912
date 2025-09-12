#define _GNU_SOURCE
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "common.h"
#include "../include/eth1.h"
#include "../include/udp_ctrl_ftp.h"  /* udp_ctrl_ftp_run() */
#include "../include/protocol_tx.h"
#include "../include/update_runner.h"
#include "../include/udp_report.h"
#include "../include/udp_cmd.h"

/* 如果工程没有 app_transfer.h，就用 extern 声明 */
extern transfer_result_t transfer_file_udp(const char* remote_ip, uint16_t remote_port,
	uint16_t local_port, const file_meta_t* meta,
	const proto_cfg_t* cfg);

#include "ftp_server.h"

/* ===== 本机 FTP Server 的默认配置 ===== */
#ifndef ETH1_FTP_BIND_IP
#define ETH1_FTP_BIND_IP   "192.168.1.100"   /*  ETH1 IP */
#endif
#ifndef ETH1_FTP_ROOT
#define ETH1_FTP_ROOT      "/data/ftp"
#endif
#ifndef ETH1_FTP_USER
#define ETH1_FTP_USER      "ftpuser"
#endif

static ftp_srv_cfg_t g_ftp_srv_cfg = {
	.bind_ip     = ETH1_FTP_BIND_IP,
	.port        = 21,
	.root_dir    = ETH1_FTP_ROOT,
	.user        = ETH1_FTP_USER,
	.pasv_min    = 50010,
	.pasv_max    = 50030,
	.allow_write = 1
};

/* 把直传进度打印到控制台） */
static void* progress_printer(void* arg){
	(void)arg;
	while(!atomic_load(&global_state.shutdown_requested)){
		if (proto_is_finished && proto_is_finished()) break;
		if (proto_is_error    && proto_is_error())    break;
		if (proto_progress){
			float p = proto_progress();
			printf("[ETH1][UDP-SEND] progress: %.2f%%\n", p*100.0f);
		}
		struct timespec ts={.tv_sec=1,.tv_nsec=0}; nanosleep(&ts,NULL);
	}
	return NULL;
}

/* ===== 服务线程：条件变量唤醒 -> 分发两类任务 ===== */
void* eth1_service_thread(void* arg){
	(void)arg;
	/* 保证本机 FTP Server 已启动 */
	if (ftp_srv_ensure(&g_ftp_srv_cfg) != 0) {
		fprintf(stderr, "[ETH1] WARNING: failed to start local FTP server\n");
	}
	printf("[ETH1] service thread started\n");
	
	while (!atomic_load(&global_state.shutdown_requested)) {
		/* 1) 等待任务 */
		pthread_mutex_lock(&global_state.eth1_task_mutex);
		while (!global_state.eth1_task_available &&
			!atomic_load(&global_state.shutdown_requested)) {
			pthread_cond_wait(&global_state.eth1_task_cond, &global_state.eth1_task_mutex);
		}
		if (atomic_load(&global_state.shutdown_requested)) {
			pthread_mutex_unlock(&global_state.eth1_task_mutex);
			break;
		}
		eth1_task_t* task = (eth1_task_t*)global_state.eth1_current_task;
		global_state.eth1_task_available = 0;
		pthread_mutex_unlock(&global_state.eth1_task_mutex);
		
		if (!task) { fprintf(stderr,"[ETH1] null task, skip\n"); continue; }
		
		/* 2) 分发执行 */
		if (task->type == ETH1_TASK_UDP_FTP) {
			/* 强制让通知里的 FTP 目标指向自己的 FTP Server” */
		strncpy(task->u.ftp.ftp_host, g_ftp_srv_cfg.bind_ip, sizeof(task->u.ftp.ftp_host)-1);
		task->u.ftp.ftp_port = g_ftp_srv_cfg.port;
		task->u.ftp.passive  = 1;
		
		/* 通过update_runner执行FTP更新，提供回调函数 */
		static udp_ack_cb eth1_ftp_ack_cb = NULL;
		int rc = run_update_ftp(task->u.ftp.peer_ip, task->u.ftp.peer_port, 0x0001, 0x0002, &task->u.ftp, eth1_ftp_ack_cb, NULL, NULL);
		printf("[ETH1] UDP-FTP control finished, rc=%d\n", rc);
		}
		else if (task->type == ETH1_TASK_UDP_SEND) {
			/* 通过update_runner执行UDP直传，提供回调函数 */
	static udp_ack_cb eth1_udp_ack_cb = NULL;
	int rc = run_update_udp(task->u.send.remote_ip, 0, 0x0001, 0x0002, task->u.send.remote_port, task->u.send.local_port, &task->u.send.meta, &task->u.send.cfg, eth1_udp_ack_cb, NULL, NULL, NULL);
		printf("[ETH1] UDP-SEND finished, rc=%d\n", rc);
		} else if (task->type == ETH1_TASK_UDP_CMD) {
			/* 发送指令并等待应答 */
		int rc = udp_send_cmd_wait_ack(task->u.cmd.remote_ip, task->u.cmd.remote_port,
		                             &task->u.cmd.cmd, task->u.cmd.resp_out,
		                             task->u.cmd.timeout_ms, task->u.cmd.max_retry,
		                             task->u.cmd.callback, task->u.cmd.cb_user,
		                             task->u.cmd.flow_tag, task->u.cmd.request_id);
		printf("[ETH1] UDP-CMD finished, rc=%d\n", rc);
			}
		else {
			fprintf(stderr,"[ETH1] unknown task type=%d\n", task->type);
		}
		
		/* 3) 一次任务结束；可在此处回写结果到 global_state 供 main 查询 */
		// 释放任务内存
		free(task);
	}
	
	printf("[ETH1] service thread exit\n");
	return NULL;
}

/* 发送指令并等待应答的实现 */
int eth1_send_cmd_wait_ack(const char* ip, uint16_t port, 
                           const cmd_frame_t* req, resp_frame_t* ack_out,
                           int timeout_ms, int max_retry,
                           udp_ack_cb cb, void* cb_user,
                           int flow_tag, const char* request_id)
{
    if (!ip || !req) {
        fprintf(stderr, "[ETH1] Invalid parameters for cmd task\n");
        return -1;
    }
    
    /* 分配任务内存 */
    eth1_task_t* task = (eth1_task_t*)malloc(sizeof(eth1_task_t));
    if (!task) {
        fprintf(stderr, "[ETH1] Failed to allocate memory for cmd task\n");
        return -2;
    }
    
    /* 填充任务参数 */
    memset(task, 0, sizeof(eth1_task_t));
    task->type = ETH1_TASK_UDP_CMD;
    strncpy(task->u.cmd.remote_ip, ip, sizeof(task->u.cmd.remote_ip) - 1);
    task->u.cmd.remote_port = port;
    task->u.cmd.cmd = *req;
    task->u.cmd.resp_out = ack_out;
    task->u.cmd.timeout_ms = (timeout_ms <= 0) ? 5000 : timeout_ms; // 默认5秒超时
    task->u.cmd.max_retry = (max_retry <= 0) ? 3 : max_retry;       // 默认重试3次
    task->u.cmd.callback = cb;
    task->u.cmd.cb_user = cb_user;
    task->u.cmd.flow_tag = flow_tag;
    if (request_id) {
        strncpy(task->u.cmd.request_id, request_id, sizeof(task->u.cmd.request_id) - 1);
    }
    
    /* 提交任务到eth1线程 */
    pthread_mutex_lock(&global_state.eth1_task_mutex);
    // 使用memcpy转换eth1_task_t到UpdateTask
    UpdateTask* update_task = (UpdateTask*)malloc(sizeof(UpdateTask));
    if (update_task) {
        memset(update_task, 0, sizeof(UpdateTask));
        global_state.eth1_current_task = update_task;
    }
    global_state.eth1_task_available = 1;
    pthread_cond_signal(&global_state.eth1_task_cond);
    pthread_mutex_unlock(&global_state.eth1_task_mutex);
    
    /* 等待任务完成的逻辑需要根据实际需求实现 */
    /* 当前实现是非阻塞的，直接返回成功提交 */
    return 0;
}

