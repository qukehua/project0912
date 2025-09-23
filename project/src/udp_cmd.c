
#include "../include/udp_cmd.h"

#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdio.h>

int udp_send_cmd_wait_ack(const char* ip, uint16_t port, const cmd_frame_t* req, resp_frame_t* ack_out, int timeout_ms, int max_retry, udp_ack_cb cb, void* cb_user, int flow_tag, const char* request_id)
{
    if(!ip || !req) return -1;
    if(timeout_ms<=0) timeout_ms = 5000;
    if(max_retry<=0) max_retry = 3;

    int s = socket(AF_INET, SOCK_DGRAM, 0);
    if(s<0) return -2;

    struct sockaddr_in to = {0};
    to.sin_family = AF_INET;
    to.sin_port   = htons(port);
    if(inet_pton(AF_INET, ip, &to.sin_addr) != 1){ close(s); return -3; }

    uint8_t buf[256];
    size_t blen=0;
    if(cmd_encode(req, buf, sizeof buf, &blen) != 0){ close(s); return -4; }

    int attempt = 0;
    for(; attempt<=max_retry; ++attempt){
        /* 发送 */
        ssize_t wn = sendto(s, buf, blen, 0, (struct sockaddr*)&to, sizeof to);
        if(wn<0){ close(s); return -5; }
        
        /* 命令发送成功（暂时不上报，等待实际的应答） */

        /* 等待 */
        fd_set rf; FD_ZERO(&rf); FD_SET(s, &rf);
        struct timeval tv = { .tv_sec = timeout_ms/1000, .tv_usec = (timeout_ms%1000)*1000 };
        int r = select(s+1, &rf, NULL, NULL, &tv);
        if(r<=0){
            continue; /* timeout -> retry */
        }

        struct sockaddr_in from; socklen_t flen = sizeof from;
        uint8_t rcv[256]; int n = recvfrom(s, rcv, sizeof rcv, 0, (struct sockaddr*)&from, &flen);
        if(n<=0) continue;

        resp_frame_t rsp;
        if(resp_decode(rcv, (size_t)n, &rsp) != 0) continue;
        /* 成功收到了应答：上报并返回 */
        if(cb) cb(flow_tag, req->code, &rsp, request_id, cb_user);
        if(ack_out) *ack_out = rsp;

        close(s);
        return 0;
    }

    /* 超出重试次数：上报一次失败态（以resp2最高位当错误标记由上层去判） */
    if(cb){
        resp_frame_t er = {0};
        er.resp1 = req->code;
        er.resp2 = 0x8000u; /* 自定义：最高位=1视为失败 */
        cb(flow_tag, req->code, &er, request_id, cb_user);
    }
    close(s);
    return -6; /* 超时无应答 */
}
