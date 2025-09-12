#include "../include/app_transfer.h"
#include "../include/protocol_tx.h"
#include "../include/resp_frame.h"
#include "../include/udp_report.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>

/* 全局回调变量 - 这些变量将通过app_transfer_register_cbs设置 */
static udp_ack_cb g_ack_cb = NULL;
static udp_prog_cb g_prog_cb = NULL;
static void* g_user_data = NULL;
static const char* g_request_id = NULL;

/* 声明外部回调注册函数，实现在app_report.c中 */
extern void app_transfer_register_cbs(udp_ack_cb ack_cb, udp_prog_cb prog_cb, void* user, const char* request_id);

static int g_sock = -1;
static pthread_t g_th;
static volatile int g_run = 0;

/* 进度回调函数 */
static void progress_callback(float progress, uint32_t done, uint32_t total){
	if(g_prog_cb){
		g_prog_cb(1, progress, done, total, g_request_id, g_user_data);
	}
}

static void *rx_thread(void *arg){
    (void)arg;
    uint8_t buf[65536];
    while(g_run){
        ssize_t n = recv(g_sock, buf, sizeof buf, 0);
        if(n>0) proto_on_rx(buf, (size_t)n);
    }
    return NULL;
}

transfer_result_t transfer_file_udp(const char *remote_ip, uint16_t remote_port, uint16_t local_port,
                                    const file_meta_t *meta, const proto_cfg_t *cfg)
{
    transfer_result_t ret = TR_OK;
    g_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if(g_sock < 0) return TR_ERR_SOCKET;

    struct sockaddr_in local={0};
    local.sin_family=AF_INET; local.sin_port=htons(local_port); local.sin_addr.s_addr=htonl(INADDR_ANY);
    if(bind(g_sock, (struct sockaddr*)&local, sizeof local) < 0){ close(g_sock); return TR_ERR_SOCKET; }

    struct sockaddr_in remote={0};
    remote.sin_family=AF_INET; remote.sin_port=htons(remote_port); inet_pton(AF_INET, remote_ip, &remote.sin_addr);
    if(connect(g_sock, (struct sockaddr*)&remote, sizeof remote) < 0){ close(g_sock); return TR_ERR_SOCKET; }

    g_run = 1;
    if(pthread_create(&g_th, NULL, rx_thread, NULL)!=0){ close(g_sock); return TR_ERR_SOCKET; }

    proto_cfg_t cfg_tmp = *cfg;
    cfg_tmp.progress_cb = progress_callback;
    proto_init(g_sock, &cfg_tmp);
    if(proto_start_file(meta) != 0){
        ret = TR_ERR_PROTOCOL_START;
        g_run = 0; shutdown(g_sock, SHUT_RDWR); close(g_sock);
        return ret;
    }

    while(!proto_is_finished() && !proto_is_error()){
        usleep(10000);
        proto_tick();
    }

    if(proto_is_finished()){
        ret = TR_OK;
    }else{
        switch(proto_last_error()){
            case PERR_CMD_START_RETRY_EXCEEDED: ret = TR_ERR_START_RETRY_EXCEEDED; break;
            case PERR_DATA_RETRY_EXCEEDED:      ret = TR_ERR_DATA_RETRY_EXCEEDED; break;
            case PERR_CMD_END_RETRY_EXCEEDED:   ret = TR_ERR_END_RETRY_EXCEEDED; break;
            case PERR_FILE_RESTART_EXCEEDED:    ret = TR_ERR_FILE_RESTART_EXCEEDED; break;
            case PERR_IO_OPEN_FAIL:             ret = TR_ERR_OPEN_FILE; break;
            case PERR_INTERNAL_BUG:             ret = TR_ERR_PROTOCOL_INTERNAL; break;
            default:                            ret = TR_ERR_PROTOCOL_INTERNAL; break;
        }
    }

    g_run = 0;
    shutdown(g_sock, SHUT_RDWR); close(g_sock);
    pthread_join(g_th, NULL);
    return ret;
}
