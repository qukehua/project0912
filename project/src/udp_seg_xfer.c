/*
 * udp_seg_xfer.c —— UDP 分片直传（文件型传输）
 * 流程：FILE_START(等ACK) → [分片数据逐包ACK] → FILE_END(等ACK)
 * 组帧/状态机：完全复用 udp_sm.{h,c} 的 sm_file_* 系列
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
#include "data_frame.h"
#include "eth1_ops.h"

/* 计算整文件 CRC16-CCITT-FALSE（多用于 Start 参数的最后2B） */
static uint16_t crc16_ccitt_false_file(const char* path){
    FILE* fp = fopen(path, "rb");
    if(!fp) return 0;
    uint16_t crc = 0xFFFF; /* 初值 */
    int c;
    while((c=fgetc(fp))!=EOF){
        crc ^= (uint8_t)c << 8;
        for(int i=0;i<8;i++)
            crc = (crc & 0x8000) ? (uint16_t)((crc<<1) ^ 0x1021) : (uint16_t)(crc<<1);
    }
    fclose(fp);
    return crc;
}

/* 按“3-1 文件传输开始”打包参数（设备标识/文件类型/子类型/是否分段/段数/总长/尾长/CRC16） */
static int build_file_start_params(uint8_t* buf, size_t cap, size_t* out_len,
                                   uint8_t dev_id, uint8_t file_type, uint8_t sub_type,
                                   uint32_t total_len, uint32_t slice, uint16_t crc16)
{
    if(!buf||!out_len||slice==0) return -1;
    /* 计算段数/尾段长度 */
    uint32_t seg_cnt = (total_len + slice - 1) / slice;
    uint32_t tail    = total_len - (seg_cnt-1)*slice;
    if (cap < 1+1+1+1+2+4+4+2) return -2;

    size_t off=0;
    buf[off++] = dev_id;                 /* Param1: 重构设备标识（这里用 ruid） */
    buf[off++] = file_type;              /* Param2: 文件类型 */
    buf[off++] = sub_type;               /* Param3: 文件子类型 */
    buf[off++] = (seg_cnt>1)? 0x01:0x00; /* Param4: 是否分段（简化位域：>1段则置1） */
    buf[off++] = (uint8_t)((seg_cnt>>8)&0xFF); /* Param5: 段数(2B) */
    buf[off++] = (uint8_t)( seg_cnt     &0xFF);
    buf[off++] = (uint8_t)((total_len>>24)&0xFF); /* Param6: 文件总长(4B) */
    buf[off++] = (uint8_t)((total_len>>16)&0xFF);
    buf[off++] = (uint8_t)((total_len>> 8)&0xFF);
    buf[off++] = (uint8_t)( total_len     &0xFF);
    buf[off++] = (uint8_t)((tail>>24)&0xFF);      /* Param7: 尾段长度(4B) */
    buf[off++] = (uint8_t)((tail>>16)&0xFF);
    buf[off++] = (uint8_t)((tail>> 8)&0xFF);
    buf[off++] = (uint8_t)( tail     &0xFF);
    buf[off++] = (uint8_t)((crc16>>8)&0xFF);      /* Param8: CRC16-CCITT-FALSE(2B) */
    buf[off++] = (uint8_t)( crc16     &0xFF);

    *out_len = off;
    return 0;
}

/* 结束参数（如果表中有额外参数，这里扩展；现按0参数处理） */
static int build_file_end_params(uint8_t* buf, size_t cap, size_t* out_len){
    (void)buf; (void)cap; *out_len = 0; return 0;
}

/* UDP 发送回调 */
typedef struct { int fd; struct sockaddr_in6 peer; } link_t;
static int udp_send_cb(const uint8_t* buf, size_t len, void* user){
    link_t* l = (link_t*)user;
    ssize_t n = sendto(l->fd, buf, len, 0, (struct sockaddr*)&l->peer, sizeof(l->peer));
    return (n==(ssize_t)len) ? 0 : -1;
}
static void sm_log(const char* m, void* u){ (void)u; }

/* 读入整文件到内存（简化） */
static int read_file_all(const char* path, uint8_t** out, size_t* out_len){
    FILE* fp=fopen(path,"rb"); if(!fp) return -1;
    fseek(fp,0,SEEK_END); long sz=ftell(fp); fseek(fp,0,SEEK_SET);
    if(sz<=0){ fclose(fp); return -2; }
    uint8_t* p=(uint8_t*)malloc((size_t)sz);
    if(!p){ fclose(fp); return -3; }
    size_t n=fread(p,1,(size_t)sz,fp);
    fclose(fp);
    if(n!=(size_t)sz){ free(p); return -4; }
    *out=p; *out_len=(size_t)sz; return 0;
}

int eth1_run_udp_segmented(const EthTarget* target, const CtrlPolicy* policy,
                           const FileTransferSpec* spec,
                           eth1_raw_resp_t* o_last_resp, const Eth1Emitter* em)
{
    (void)policy;
    if(!target || !spec || !spec->file_path || !o_last_resp) return -1;
    if(spec->seg.slice_size==0 || spec->seg.slice_size>1000) return -2;

    /* 1) 解析端点 */
    char ip6[40]={0}; uint16_t port=0, apid_low_data=0;
    if (endpoint_query_by_ruid(target->ruid, ip6, &port, &apid_low_data) != 0) return -ENODEV;

    /* 2) 读文件 + 计算 CRC16（可选） */
    uint8_t* data=NULL; size_t data_len=0;
    int rc = read_file_all(spec->file_path, &data, &data_len);
    if(rc){ return -3; }
    uint16_t crc = spec->seg.enable_crc ? crc16_ccitt_false_file(spec->file_path) : 0;

    /* 3) 准备 Start/End 参数 */
    uint8_t p_start[32]; size_t n_start=0;
    rc = build_file_start_params(p_start, sizeof(p_start), &n_start,
                                 target->ruid, spec->seg.file_type, spec->seg.sub_type,
                                 (uint32_t)data_len, spec->seg.slice_size, crc);
    if(rc){ free(data); return -4; }
    uint8_t p_end[4]; size_t n_end=0; build_file_end_params(p_end, sizeof(p_end), &n_end);

    /* 4) 建 UDP 非阻塞 socket */
    link_t L={0}; L.fd=socket(AF_INET6, SOCK_DGRAM, 0);
    if(L.fd<0){ free(data); return -5; }
    int fl=fcntl(L.fd, F_GETFL, 0); fcntl(L.fd, F_SETFL, fl|O_NONBLOCK);
    memset(&L.peer,0,sizeof(L.peer));
    L.peer.sin6_family=AF_INET6; L.peer.sin6_port=htons(port);
    if(inet_pton(AF_INET6, ip6, &L.peer.sin6_addr)!=1){ close(L.fd); free(data); return -6; }

    /* 5) 状态机：Start→分片→End */
    sm_file_t fs; sm_file_init(&fs, target->ruid, udp_send_cb, &L, sm_log, NULL);
    if (sm_file_start(&fs,
                      CMD_CODE_FILE_START, CMD_CODE_FILE_START_ACK,
                      CMD_CODE_FILE_UDP_ACK,
                      CMD_CODE_FILE_END, CMD_CODE_FILE_END_ACK,
                      p_start, (uint16_t)n_start,
                      p_end,   (uint16_t)n_end,
                      data, data_len,
                      (uint64_t)(time(NULL)*1000ull)) != 0){
        close(L.fd); free(data); return -7;
    }

    uint8_t rx[1600];
    eth1_raw_resp_t last={0};

    while(!sm_file_idle(&fs)){
        /* 推进（内部按 5s/≤3 的口径处理） */
        sm_file_poll(&fs, (uint64_t)(time(NULL)*1000ull));

        /* 200ms select */
        fd_set rfds; FD_ZERO(&rfds); FD_SET(L.fd,&rfds);
        struct timeval tv={.tv_sec=0,.tv_usec=200*1000};
        int r=select(L.fd+1,&rfds,NULL,NULL,&tv);
        if(r<0){ close(L.fd); free(data); return -8; }
        if(r==0) continue;

        ssize_t n=recv(L.fd, rx, sizeof(rx), 0);
        if(n<=0) continue;

        /* 解析 ACK，立即上报（包括逐包 0x018A 与 START/END ACK） */
        nixyk_cmd_ack_view_t v;
        if (nixyk_cmd_parse_ack(rx,(size_t)n,&v)==0 && (v.apid11 & 0x0F)==0x7){
            if (em && em->sink){
                Eth1Msg m={0}; m.type=ETH1_MSG_ACK;
                m.u.ack.task_id=em->task_id; m.u.ack.ruid=em->ruid; m.u.ack.opcode=v.cmd_code;
                m.u.ack.payload.len = (v.param_len<=sizeof(m.u.ack.payload.buf))? v.param_len : sizeof(m.u.ack.payload.buf);
                memcpy(m.u.ack.payload.buf, v.params, m.u.ack.payload.len);
                em->sink(em->user, &m);

                /* 逐包应答也做简单进度上报（无百分比，给出“分片已确认”文案） */
                if (v.cmd_code == CMD_CODE_FILE_UDP_ACK){
                    Eth1Msg pm={0}; pm.type=ETH1_MSG_PROGRESS;
                    pm.u.progress.task_id=em->task_id; pm.u.progress.ruid=em->ruid;
                    pm.u.progress.pct = 0; pm.u.progress.stage=2; /* 约定：2=UDP分片直传 */
                    snprintf(pm.u.progress.note, sizeof(pm.u.progress.note), "收到分片ACK(0x018A): 0x%02X", (v.param_len>=1)?v.params[0]:0xFF);
                    em->sink(em->user, &pm);
                }
            }
            last.len = (v.param_len<=sizeof(last.buf))? v.param_len : sizeof(last.buf);
            memcpy(last.buf, v.params, last.len);
        }

        int done = sm_file_on_udp(&fs, rx, (size_t)n, (uint64_t)(time(NULL)*1000ull));
        if (done == 1 || sm_file_done(&fs)) break;
        if (done < 0 || sm_file_fail(&fs)){ close(L.fd); free(data); return -9; }
    }

    close(L.fd);
    free(data);
    *o_last_resp = last;
    return sm_file_done(&fs) ? 0 : -10;
}
