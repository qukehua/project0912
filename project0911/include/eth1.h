#pragma once
#include <stdint.h>
#include "../include/udp_ctrl_ftp.h"   /* UDP 控制 FTP 的配置结构 udp_ftp_cfg_t */
#include "../include/protocol_tx.h"    /* 直传协议结构：file_meta_t / proto_cfg_t */
#include "../include/cmd_frame.h"      /* 命令帧结构 */
#include "../include/resp_frame.h"     /* 响应帧结构 */
#include "../include/udp_report.h"     /* UDP 回调函数类型 */

#ifdef __cplusplus
extern "C" {
#endif
	
	typedef enum {
		ETH1_TASK_UDP_FTP  = 1,   /* UDP 控制 FTP（通知 + 周期查询） */
		ETH1_TASK_UDP_SEND = 2,   /* 单纯 UDP 直传文件 */
		ETH1_TASK_UDP_CMD  = 3    /* UDP 发送指令并等待应答 */
		ETH1_TASK_VERSION_QUERY = 4  /* 软件版本查询 */
	} eth1_task_type_t;
	
	/* 直传任务参数 */
	typedef struct {
		char       remote_ip[32];
		uint16_t   remote_port;
		uint16_t   local_port;
		file_meta_t meta;
		proto_cfg_t cfg;          /* 可用默认；需要时在 main 里填 */
	} eth1_udp_send_t;
	
	/* 指令发送任务参数 */
	typedef struct {
		char       remote_ip[32];
		uint16_t   remote_port;
		cmd_frame_t cmd;          /* 要发送的命令帧 */
		resp_frame_t* resp_out;
		int        timeout_ms;    /* 超时时间（ms） */
		int        max_retry;     /* 最大重试次数 */
		udp_ack_cb callback;      /* 回调函数 */
		void*      cb_user;       /* 回调用户数据 */
		int        flow_tag;      /* 流标记 */
		char       request_id[64]; /* 请求ID */
	} eth1_udp_cmd_t;

	/* 统一任务对象：把这个指针塞给 global_state.eth1_current_task 即可 */
	typedef struct {
		eth1_task_type_t type;
		union {
			udp_ftp_cfg_t   ftp;   /* UDP 控制 FTP */
			eth1_udp_send_t send;  /* UDP 直传 */
			eth1_udp_cmd_t  cmd;   /* UDP 发送指令 */
		} u;
	} eth1_task_t;
	
	/* 发送指令并等待应答 */
	int eth1_send_cmd_wait_ack(const char* ip, uint16_t port, 
	                          const cmd_frame_t* req, resp_frame_t* ack_out,
	                          int timeout_ms, int max_retry,
	                          udp_ack_cb cb, void* cb_user,
	                          int flow_tag, const char* request_id);

	/* 服务线程（main: pthread_create(&eth1_tid, NULL, eth1_service_thread, NULL)） */
	void* eth1_service_thread(void* arg);
	
#ifdef __cplusplus
}
#endif

