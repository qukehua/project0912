#include "../include/app_report.h"

/* 回调函数和用户数据的全局存储 */
udp_ack_cb g_ack_cb = NULL;
udp_prog_cb g_prog_cb = NULL;
void* g_user_data = NULL;
const char* g_request_id = NULL;

/* 实现回调注册函数 */
void app_transfer_register_cbs(udp_ack_cb ack_cb, udp_prog_cb prog_cb, void* user, const char* request_id)
{
    g_ack_cb = ack_cb;
    g_prog_cb = prog_cb;
    g_user_data = user;
    g_request_id = request_id;
}