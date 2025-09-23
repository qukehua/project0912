/*
 * ftp_ctrl_runner.c —— UDP 控制 FTP（通知 + 周期查询）
 * 参考 ftp_demo.c 的口径，封装为可复用函数 eth1_run_udp_ctrl_ftp()
 * 收到“通知应答/进度应答”即刻通过 Eth1Emitter 回 main（ACK/PROGRESS/DONE）
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
#include "cmd_codes.h"
#include "eth1_ops.h"

/* 与 ftp_demo 一致的通知参数打包（1B类型/1B子类型/4B大小/1B操作/128B路径ASCII） */
static int build_ftp_notice_params(uint8_t* buf, size_t cap, size_t* out_len,
                                   uint8_t file_type, uint8_t sub_type,
                                   uint32_t file_size, uint8_t op_type,
                                   const char* file_path)
{
    if(!buf || !out_len || !file_path) return -1;
    if(cap < (size_t)(1+1+4+1+128))    return -2;
    if(file_size == 0)                 return -3;
    size_t n = strlen(file_path);
    for(size_t i=0;i<n;i++){ if((unsigned char)file_path[i] < 0x20 || (unsigned char)file_path[i] > 0x7E) return -4; }

    size_t off = 0;
    buf[off++] = file_type;
    buf[off++] = sub_type;
    buf[off++] = (uint8_t)((file_size >> 24) & 0xFF);
    buf[off++] = (uint8_t)((file_size >> 16) & 0xFF);
    buf[off++] = (uint8_t)((file_size >>  8) & 0xFF);
    buf[off++] = (uint8_t)( file_size        & 0xFF);
    buf[off++] = op_type;
    memset(&buf[off], 0, 128);
    if(n > 127) n = 127;
    memcpy(&buf[off], file_path, n);
    off += 128;
    *out_len = off;
    return 0;
}

/* UDP 发送回调 */
typedef struct { int fd; struct sockaddr_in6 peer; } link_t;
static int udp_send_cb(const uint8_t* buf, size_t len, void* user){
    link_t* l = (link_t*)user;
    ssize_t n = sendto(l->fd, buf, len, 0, (struct sockaddr*)&l->peer, sizeof(l->peer));
    return (n==(ssize_t)len) ? 0 : -1;
}
static void sm_log(const char* m, void* u){ (void)u; /* 可定向到日志 */ }

int eth1_run_udp_ctrl_ftp(const EthTarget* target, const CtrlPolicy* policy,
                          const FileTransferSpec* spec,
                          eth1_raw_resp_t* o_last_resp, const Eth1Emitter* em)
{
    (void)policy;
    if(!target || !spec || !spec->file_path || !o_last_resp) return -1;

    /* 1) 解析端点 */
    char ip6[40]={0}; uint16_t port=0, apid_low_data=0;
    if (endpoint_query_by_ruid(target->ruid, ip6, &port, &apid_low_data) != 0) return -ENODEV;

    /* 2) 统计文件大小（用于通知参数） */
    FILE* fp = fopen(spec->file_path, "rb");
    if(!fp) return -2;
    fseek(fp, 0, SEEK_END); long sz = ftell(fp); fseek(fp, 0, SEEK_SET);
    fclose(fp);
    if (sz <= 0 || sz > 0x7FFFFFFF) return -3;

    /* 3) 通知参数 */
    uint8_t params[1+1+4+1+128]; size_t plen=0;
    int rc = build_ftp_notice_params(params, sizeof(params), &plen,
                                     spec->ftp.file_type, spec->ftp.sub_type,
                                     (uint32_t)sz, spec->ftp.op_type,
                                     spec->file_path);
    if (rc) return -4;

    /* 4) 建 UDP/IPv6 非阻塞 socket */
    link_t L = {0}; L.fd = socket(AF_INET6, SOCK_DGRAM, 0);
    if (L.fd < 0) return -5;
    int fl = fcntl(L.fd, F_GETFL, 0); fcntl(L.fd, F_SETFL, fl | O_NONBLOCK);
    memset(&L.peer, 0, sizeof(L.peer));
    L.peer.sin6_family = AF_INET6; L.peer.sin6_port = htons(port);
    if (inet_pton(AF_INET6, ip6, &L.peer.sin6_addr) != 1){ close(L.fd); return -6; }

    /* 5) 状态机：通知 + 周期查询 */
    sm_ftp_t ftp; sm_ftp_init(&ftp, target->ruid, udp_send_cb, &L, sm_log, NULL);
    if (sm_ftp_start(&ftp,
                     CMD_CODE_FTP_NOTICE, CMD_CODE_FTP_NOTICE_ACK,
                     CMD_CODE_FTP_PROGRESS_QUERY, CMD_CODE_FTP_PROGRESS_RESP,
                     params, (uint16_t)plen, (uint64_t) (time(NULL)*1000ull)) != 0){
        close(L.fd); return -7;
    }

    uint8_t rx[1600];
    eth1_raw_resp_t last = {0};

    while(!sm_ftp_idle(&ftp)){
        /* poll 推进（内部会按 3s 周期发送查询） */
        sm_ftp_poll(&ftp, (uint64_t)(time(NULL)*1000ull));

        /* 200ms select */
        fd_set rfds; FD_ZERO(&rfds); FD_SET(L.fd,&rfds);
        struct timeval tv={.tv_sec=0,.tv_usec=200*1000};
        int r = select(L.fd+1, &rfds, NULL, NULL, &tv);
        if (r < 0){ close(L.fd); return -8; }
        if (r == 0) continue;

        ssize_t n = recv(L.fd, rx, sizeof(rx), 0);
        if (n <= 0) continue;

        /* 尝试解析 ACK 帧，立刻上报 */
        nixyk_cmd_ack_view_t v;
        if (nixyk_cmd_parse_ack(rx, (size_t)n, &v)==0 && (v.apid11 & 0x0F)==0x7){
            if (em && em->sink){
                Eth1Msg m = {0};
                m.type = ETH1_MSG_ACK;
                m.u.ack.task_id = em->task_id;
                m.u.ack.ruid    = em->ruid;
                m.u.ack.opcode  = v.cmd_code;
                m.u.ack.payload.len = (v.param_len<=sizeof(m.u.ack.payload.buf))? v.param_len : sizeof(m.u.ack.payload.buf);
                memcpy(m.u.ack.payload.buf, v.params, m.u.ack.payload.len);
                em->sink(em->user, &m);
            }
            last.len = (v.param_len<=sizeof(last.buf))? v.param_len : sizeof(last.buf);
            memcpy(last.buf, v.params, last.len);

            /* 如果是进度应答（0x01DB），发一条 PROGRESS */
            if (v.cmd_code == CMD_CODE_FTP_PROGRESS_RESP && em && em->sink){
                Eth1Msg pm = {0}; pm.type = ETH1_MSG_PROGRESS;
                pm.u.progress.task_id = em->task_id;
                pm.u.progress.ruid    = em->ruid;
                pm.u.progress.pct     = (v.param_len>=1 && v.params[0]==0x11)?100: ((v.param_len>=1 && v.params[0]==0x00)?50:0);
                pm.u.progress.stage   = 1; /* 约定：1=FTP */
                snprintf(pm.u.progress.note, sizeof(pm.u.progress.note),
                         "FTP进度: 0x%02X (0x11=完成,0x00=进行,0xFF=异常)", (v.param_len>=1)?v.params[0]:0xFF);
                em->sink(em->user, &pm);
            }
        }

        int done = sm_ftp_on_udp(&ftp, rx, (size_t)n, (uint64_t)(time(NULL)*1000ull));
        if (done == 1 || ftp.st == FTP_DONE) break;
        if (done < 0 || ftp.st == FTP_FAIL){ close(L.fd); return -9; }
    }

    close(L.fd);
    *o_last_resp = last;
    return (ftp.st == FTP_DONE) ? 0 : -10;
}
