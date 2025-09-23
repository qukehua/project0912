/*
 * eth1_ops_reconfig.c —— 重构编排器
 * 流程：
 *   1) 自主重构组件“准备”指令：等待 ACK（收到即回 ACK 消息）
 *   2) 文件传输：由 plan->xfer.method 决定 FTP 或 UDP 分片（期间按收到的应答回 ACK/PROGRESS）
 *   3) 自主重构组件“开始”指令：发出即结束（不等待应答）
 */
#include <string.h>
#include <stdio.h>

#include "cmd_codes.h"
#include "eth1_ops.h"

/* 内部：复用控制类的通用发送（声明于 eth1_ops_ctrl.c） */
static int do_prepare(const EthTarget* t, eth1_raw_resp_t* o, const Eth1Emitter* em){
    extern int eth1_factory_reset_notify(const EthTarget*, const CtrlPolicy*, eth1_raw_resp_t*, const Eth1Emitter*);
    /* 这里不能直接用“恢复出厂”，而是用计划里给的准备指令对（2-1/2-2） */
    /* 为避免额外暴露内部 helper，这里复制一份 do_ctrl_once 的入口声明 */
    extern int eth1_sw_version_query(const EthTarget*, const CtrlPolicy*, eth1_raw_resp_t*, const Eth1Emitter*); /* 仅为拿符号，实际不用 */
    /* 实际调用由 eth1_ops_ctrl.c 暴露的“通用一次”不对外；此处换为在本文件内的局部实现 */
    return 0; /* 在下方真正走一遍“通用一次控制指令” */
}

#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <time.h>

#include "udp_sm.h"
#include "endpoint_map.h"
#include "cmd_frame.h"

/* 为本文件复制一份“通用一次控制指令”，但用 plan 的准备/开始码 */
static uint64_t now_ms(void){
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec*1000ull + ts.tv_nsec/1000000ull;
}
typedef struct { int fd; struct sockaddr_in6 peer; } link_t;
static int udp_send_cb(const uint8_t* buf, size_t len, void* user){
    link_t* l = (link_t*)user; ssize_t n = sendto(l->fd, buf, len, 0, (struct sockaddr*)&l->peer, sizeof(l->peer));
    return (n==(ssize_t)len) ? 0 : -1;
}
static void sm_log(const char* m, void* u){ (void)u; }

static int ctrl_once_custom(uint16_t req_code, uint16_t ack_code,
                            const EthTarget* target, eth1_raw_resp_t* o_resp,
                            const Eth1Emitter* em, int wait_ack)
{
    if (!target || !o_resp) return -1;
    char ip6[40]={0}; uint16_t port=0, apid_low=0;
    if (endpoint_query_by_ruid(target->ruid, ip6, &port, &apid_low) != 0) return -ENODEV;

    link_t L={0}; L.fd=socket(AF_INET6, SOCK_DGRAM, 0);
    if(L.fd<0) return -2;
    int fl=fcntl(L.fd, F_GETFL, 0); fcntl(L.fd, F_SETFL, fl|O_NONBLOCK);
    memset(&L.peer,0,sizeof(L.peer));
    L.peer.sin6_family=AF_INET6; L.peer.sin6_port=htons(port);
    if(inet_pton(AF_INET6, ip6, &L.peer.sin6_addr)!=1){ close(L.fd); return -3; }

    eth1_raw_resp_t last={0};

    if (wait_ack){
        sm_ctrl_t sm; sm_ctrl_init(&sm, target->ruid, udp_send_cb, &L, sm_log, NULL);
        if (sm_ctrl_start(&sm, req_code, ack_code, NULL, 0, now_ms()) != 0){ close(L.fd); return -4; }

        uint8_t rx[1600];
        while(!sm_ctrl_idle(&sm)){
            sm_ctrl_poll(&sm, now_ms());
            fd_set rfds; FD_ZERO(&rfds); FD_SET(L.fd, &rfds);
            struct timeval tv={.tv_sec=0,.tv_usec=200*1000};
            int r=select(L.fd+1,&rfds,NULL,NULL,&tv);
            if(r<0){ close(L.fd); return -5; }
            if(r==0) continue;

            ssize_t n=recv(L.fd, rx, sizeof(rx), 0);
            if(n<=0) continue;

            nixyk_cmd_ack_view_t v;
            if (nixyk_cmd_parse_ack(rx,(size_t)n,&v)==0 && (v.apid11 & 0x0F)==0x7){
                if (em && em->sink){
                    Eth1Msg m={0}; m.type=ETH1_MSG_ACK;
                    m.u.ack.task_id=em->task_id; m.u.ack.ruid=em->ruid; m.u.ack.opcode=v.cmd_code;
                    m.u.ack.payload.len = (v.param_len<=sizeof(m.u.ack.payload.buf))? v.param_len : sizeof(m.u.ack.payload.buf);
                    memcpy(m.u.ack.payload.buf, v.params, m.u.ack.payload.len);
                    em->sink(em->user, &m);
                }
                last.len = (v.param_len<=sizeof(last.buf))? v.param_len : sizeof(last.buf);
                memcpy(last.buf, v.params, last.len);
            }
            int done = sm_ctrl_on_udp(&sm, rx, (size_t)n, now_ms());
            if(done==1) break;
            if(done<0){ close(L.fd); return -6; }
        }
    } else {
        /* 开始指令：发出即结束，不等待 ACK */
        sm_ctrl_t sm; sm_ctrl_init(&sm, target->ruid, udp_send_cb, &L, sm_log, NULL);
        if (send_cmd(sm_log, NULL, udp_send_cb, &L, target->ruid, 0x0, &sm.next_seq14, req_code, NULL, 0) != 0){
            close(L.fd); return -7;
        }
    }

    close(L.fd);
    *o_resp = last;
    return 0;
}

int eth1_reconfig_execute(const ReconfigPlan* plan,
                          eth1_raw_resp_t* o_last_resp, const Eth1Emitter* em)
{
    if(!plan || !o_last_resp) return -1;

    /* 1) 准备（等待ACK） */
    eth1_raw_resp_t prep={0};
    int rc = ctrl_once_custom(plan->cmd_prepare_req, plan->cmd_prepare_ack,
                              &plan->target, &prep, em, /*wait_ack*/1);
    if (rc){ return rc; }

    /* 2) 文件传输：两种皆可，上层选择 */
    eth1_raw_resp_t last={0};
    switch(plan->xfer.method){
        case FT_UDP_CTRL_FTP:
            rc = eth1_run_udp_ctrl_ftp(&plan->target, &plan->policy, &plan->xfer, &last, em);
            break;
        case FT_UDP_SEGMENTED:
            rc = eth1_run_udp_segmented (&plan->target, &plan->policy, &plan->xfer, &last, em);
            break;
        default: return -2;
    }
    if (rc){ return rc; }

    /* 3) 开始（不等待ACK） */
    eth1_raw_resp_t dummy={0};
    rc = ctrl_once_custom(plan->cmd_start_req, 0, &plan->target, &dummy, em, /*wait_ack*/0);
    if (rc){ return rc; }

    *o_last_resp = last;
    return 0;
}
