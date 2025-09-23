/*
 * eth1_ops_ctrl.c —— 控制类5项：版本查询/恢复出厂（通知/查询）/回退（设置/查询）
 * 统一策略：
 *   - 解析 ruid→ip6/port（endpoint_map）
 *   - 建 IPv6 UDP socket（非阻塞）
 *   - 用 sm_ctrl_* 状态机：发 REQ → 等 ACK（内部完整解析/校验）
 *   - 每次收到 ACK 即刻通过 Eth1Emitter 回 main（ETH1_MSG_ACK）
 *   - 成功时把“应答参数区原始字节”拷到 o_resp->buf/len
 */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <time.h>

#include "endpoint_map.h"
#include "udp_sm.h"
#include "cmd_frame.h"
#include "cmd_codes.h"
#include "eth1_ops.h"

#ifndef CMD_CODE_ROLLBACK_SET
/* 若 cmd_codes.h 未定义回退相关宏，可先用兜底值，后续再在 cmd_codes.h 补正 */
#define CMD_CODE_ROLLBACK_SET         0x350Au
#define CMD_CODE_ROLLBACK_SET_ACK     0x340Bu
#define CMD_CODE_ROLLBACK_QUERY       0x350Cu
#define CMD_CODE_ROLLBACK_QUERY_ACK   0x340Du
#endif

/* ===== 工具：时间戳(ms) & UDP 发送回调 ===== */
static uint64_t now_ms(void){
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec*1000ull + ts.tv_nsec/1000000ull;
}
typedef struct { int fd; struct sockaddr_in6 peer; } link_t;

static int udp_send_cb(const uint8_t* buf, size_t len, void* user){
    link_t* l = (link_t*)user;
    ssize_t n = sendto(l->fd, buf, len, 0, (struct sockaddr*)&l->peer, sizeof(l->peer));
    return (n==(ssize_t)len) ? 0 : -1;
}
static void sm_log(const char* m, void* u){ (void)u; /* 可加fprintf(stderr,...) */ }

/* ===== 共通一次性控制指令 ===== */
static int do_ctrl_once(uint16_t req_code, uint16_t ack_code,
                        const EthTarget* target, eth1_raw_resp_t* o_resp,
                        const Eth1Emitter* em)
{
    if (!target || !o_resp) return -1;

    /* 1) 解析 ruid → ip/port/apid_low_data */
    char ip6[40] = {0}; uint16_t port = 0, apid_low_data = 0;
    if (endpoint_query_by_ruid(target->ruid, ip6, &port, &apid_low_data) != 0) return -ENODEV;

    /* 2) 建 UDP/IPv6 socket，非阻塞 */
    link_t L = {0}; L.fd = socket(AF_INET6, SOCK_DGRAM, 0);
    if (L.fd < 0) return -2;
    int fl = fcntl(L.fd, F_GETFL, 0); fcntl(L.fd, F_SETFL, fl | O_NONBLOCK);
    memset(&L.peer, 0, sizeof(L.peer));
    L.peer.sin6_family = AF_INET6; L.peer.sin6_port = htons(port);
    if (inet_pton(AF_INET6, ip6, &L.peer.sin6_addr) != 1){ close(L.fd); return -3; }

    /* 3) 初始化控制状态机并首发 */
    eth1_raw_resp_t last = {0};
    sm_ctrl_t sm; sm_ctrl_init(&sm, target->ruid, udp_send_cb, &L, sm_log, NULL);
    if (sm_ctrl_start(&sm, req_code, ack_code, NULL, 0, now_ms()) != 0){ close(L.fd); return -4; }

    /* 4) 事件轮询：select(200ms) + poll 状态机；收ACK即回 main */
    uint8_t rx[1600];
    while(!sm_ctrl_idle(&sm)){
        sm_ctrl_poll(&sm, now_ms());

        fd_set rfds; FD_ZERO(&rfds); FD_SET(L.fd, &rfds);
        struct timeval tv = { .tv_sec=0, .tv_usec=200*1000 };
        int r = select(L.fd+1, &rfds, NULL, NULL, &tv);
        if (r < 0) { close(L.fd); return -5; }
        if (r == 0) continue;

        ssize_t n = recv(L.fd, rx, sizeof(rx), 0);
        if (n <= 0) continue;

        /* 4.1 解析是否为 ACK 帧；若是，先把“原始参数区”抛回 main */
        uint16_t code=0; const uint8_t* p=NULL; uint16_t m=0;
        if (0 == nixyk_cmd_parse_ack(rx, (size_t)n, &(nixyk_cmd_ack_view_t){0})){
            nixyk_cmd_ack_view_t v;
            if (nixyk_cmd_parse_ack(rx, (size_t)n, &v)==0 && (v.apid11 & 0x0F)==0x7){
                /* 即时报 ACK （ETH1_MSG_ACK） */
                if (em && em->sink){
                    Eth1Msg msg = {0};
                    msg.type = ETH1_MSG_ACK;
                    msg.u.ack.task_id = em->task_id;
                    msg.u.ack.ruid    = em->ruid;
                    msg.u.ack.opcode  = v.cmd_code;
                    msg.u.ack.payload.len = (v.param_len<=sizeof(msg.u.ack.payload.buf))? v.param_len : sizeof(msg.u.ack.payload.buf);
                    memcpy(msg.u.ack.payload.buf, v.params, msg.u.ack.payload.len);
                    em->sink(em->user, &msg);
                }
                /* 同时保留“最后一次应答”的原始字节，以便函数返回 */
                last.len = (v.param_len<=sizeof(last.buf))? v.param_len : sizeof(last.buf);
                memcpy(last.buf, v.params, last.len);
            }
        }
        int done = sm_ctrl_on_udp(&sm, rx, (size_t)n, now_ms());
        if (done == 1) break;
        if (done < 0){ close(L.fd); return -6; }
    }
    close(L.fd);
    *o_resp = last;
    return 0;
}

/* ===== 五个操作的薄封装 ===== */
int eth1_sw_version_query(const EthTarget* t, const CtrlPolicy* p, eth1_raw_resp_t* o, const Eth1Emitter* em){
    (void)p; return do_ctrl_once(CMD_CODE_SW_VERSION_QUERY,       CMD_CODE_SW_VERSION_RESP,       t, o, em);
}
int eth1_factory_reset_notify(const EthTarget* t, const CtrlPolicy* p, eth1_raw_resp_t* o, const Eth1Emitter* em){
    (void)p; return do_ctrl_once(CMD_CODE_FACTORY_RESET,          CMD_CODE_FACTORY_RESET_ACK,     t, o, em);
}
int eth1_factory_reset_query(const EthTarget* t, const CtrlPolicy* p, eth1_raw_resp_t* o, const Eth1Emitter* em){
    (void)p; return do_ctrl_once(CMD_CODE_FACTORY_RESET_QUERY,    CMD_CODE_FACTORY_RESET_QUERY_ACK, t, o, em);
}
int eth1_rollback_set(const EthTarget* t, const CtrlPolicy* p, eth1_raw_resp_t* o, const Eth1Emitter* em){
    (void)p; return do_ctrl_once(CMD_CODE_ROLLBACK_SET,           CMD_CODE_ROLLBACK_SET_ACK,      t, o, em);
}
int eth1_rollback_query(const EthTarget* t, const CtrlPolicy* p, eth1_raw_resp_t* o, const Eth1Emitter* em){
    (void)p; return do_ctrl_once(CMD_CODE_ROLLBACK_QUERY,         CMD_CODE_ROLLBACK_QUERY_ACK,    t, o, em);
}
