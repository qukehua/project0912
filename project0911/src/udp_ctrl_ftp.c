#define _GNU_SOURCE
#include "../include/udp_ctrl_ftp.h"

#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

/* ===== 回调相关全局变量 ===== */
static udp_ack_cb g_ack_cb = NULL;
static void* g_user_data = NULL;
static char g_request_id[64] = {0};

/* ===== 回调注册函数实现 ===== */
void udp_ctrl_ftp_register_cb(udp_ack_cb cb, void* user, const char* request_id){
    g_ack_cb = cb;
    g_user_data = user;
    if(request_id){
        strncpy(g_request_id, request_id, sizeof(g_request_id)-1);
    } else {
        g_request_id[0] = '\0';
    }
}

/* ===== 帧编解码 ===== */
#include "cmd_frame.h"   // cmd_encode()
#include "resp_frame.h"  // resp_decode()

/* ===== 指令码 ===== */
#ifndef CMD_CODE_FTP_NOTICE
#define CMD_CODE_FTP_NOTICE      0x01BB
#endif
#ifndef CMD_CODE_PROGRESS_QUERY
#define CMD_CODE_PROGRESS_QUERY  0x01CC
#endif

/* ===== 应答码2 位义（低8位）：bit7=1 错误；bit1:0=00进行中/01完成/11错误 ===== */
static inline int rsp2_is_error(uint16_t r2){ uint8_t b=(uint8_t)r2; return (b&0x80) || ((b&0x03)==0x03); }
static inline int rsp2_is_busy (uint16_t r2){ uint8_t b=(uint8_t)r2; return (b&0x03)==0x00; }
static inline int rsp2_is_done (uint16_t r2){ uint8_t b=(uint8_t)r2; return (b&0x03)==0x01; }

/* ===== 工具 ===== */
static uint64_t now_ms(void){ struct timespec ts; clock_gettime(CLOCK_MONOTONIC,&ts); return (uint64_t)ts.tv_sec*1000ull + ts.tv_nsec/1000000ull; }
static void msleep(int ms){ struct timespec ts={.tv_sec=ms/1000,.tv_nsec=(ms%1000)*1000000}; nanosleep(&ts,NULL); }

static int udp_sendto_all(int s, const struct sockaddr_in* to, const void* p, size_t n){
	return sendto(s, p, n, 0, (const struct sockaddr*)to, sizeof *to) == (ssize_t)n ? 0 : -1;
}
static int udp_recvfrom_tmo(int s, void* buf, size_t cap, int tmo_ms, struct sockaddr_in* from){
	fd_set rfds; FD_ZERO(&rfds); FD_SET(s, &rfds);
	struct timeval tv={.tv_sec=tmo_ms/1000, .tv_usec=(tmo_ms%1000)*1000};
	int r = select(s+1, &rfds, NULL, NULL, &tv);
	if(r<=0) return r;  // 0 超时；<0 出错
	socklen_t sl=sizeof *from; ssize_t n = recvfrom(s, buf, cap, 0, (struct sockaddr*)from, &sl);
	return n>0 ? (int)n : -1;
}

/* ====== 指令参数编码（通知 / 查询） ======
* 统一定长+短字符串(1字节长度) */
static size_t enc_notice_params(uint8_t* out, size_t cap, const udp_ftp_cfg_t* c){
	uint8_t *p=out, *e=out+cap;
#define PUT_U8(v)  do{ if(p>=e) return 0; *p++=(uint8_t)(v); }while(0)
#define PUT_U16(v) do{ if(p+2>e) return 0; *p++=(uint8_t)((v)>>8); *p++=(uint8_t)(v); }while(0)
#define PUT_STR(s) do{ size_t L=strlen(s); if(L>255) L=255; if(p+1+L>e) return 0; *p++=(uint8_t)L; memcpy(p,(s),L); p+=L; }while(0)
	PUT_U8(1);                         /* method=FTP */
	PUT_U8(c->op==1?1:2);              /* 1=PUT,2=GET（对端解释） */
	PUT_U16(c->ftp_port);
	PUT_U8(c->passive?1:0);
	PUT_STR(c->ftp_host);
	PUT_STR(c->user);
	PUT_STR(c->pass);
	PUT_STR(c->remote_path);
	PUT_STR(c->local_path);
	return (size_t)(p-out);
}
static size_t enc_query_params(uint8_t* out, size_t cap, const udp_ftp_cfg_t* c){
	uint8_t *p=out, *e=out+cap;
	size_t L=strlen(c->remote_path); if(L>255) L=255;
	if(p+1+L>e) return 0; *p++=(uint8_t)L; memcpy(p,c->remote_path,L); p+=L;
	return (size_t)(p-out);
}

/* ====== 主流程 ====== */
int udp_ctrl_ftp_run(const udp_ftp_cfg_t* cfg){
	if(!cfg||!cfg->peer_ip[0]||!cfg->ftp_host[0]||!cfg->remote_path[0]||!cfg->local_path[0]) return -100;
	
	const int notice_tmo  = cfg->notice_timeout_ms  >0 ? cfg->notice_timeout_ms  : 5000;
	const int query_tmo   = cfg->query_timeout_ms   >0 ? cfg->query_timeout_ms   : 5000;
	const int query_itval = cfg->query_interval_ms  >0 ? cfg->query_interval_ms  : 3000;
	const int max_try     = cfg->notice_max_attempts>0 ? cfg->notice_max_attempts: 5;
	
	/* 建 UDP socket & 对端地址 */
	int s = socket(AF_INET, SOCK_DGRAM, 0);
	if(s<0){ perror("socket"); return -1; }
	struct sockaddr_in peer={0}; peer.sin_family=AF_INET; peer.sin_port=htons(cfg->peer_port);
	if(inet_pton(AF_INET, cfg->peer_ip, &peer.sin_addr)!=1){ fprintf(stderr,"bad peer ip\n"); close(s); return -2; }
	
	uint8_t snd[1500], pay[1024], rcv[1500];
	struct sockaddr_in from;
	
	/* 1) 发送 文件更新通知(0x01BB) + 等应答；失败重试 ≤5 */
	int attempts=0, ok=0; size_t L=0;
	while(attempts++ < max_try){
		cmd_frame_t cf={0};
		cf.type = CMD_TYPE_CUSTOM;          /*  cmd_frame.h 里定义的类型值（常见 0x55） */
		cf.seq  = (uint8_t)(attempts & 0xFF);
		cf.code = CMD_CODE_FTP_NOTICE;
		
		size_t plen = enc_notice_params(pay, sizeof pay, cfg);
		if(!plen){ fprintf(stderr,"notice params too long\n"); close(s); return -3; }
		cf.params = pay; cf.param_len = (uint8_t)plen;
		
		if(cmd_encode(&cf, snd, sizeof snd, &L)!=0){
			fprintf(stderr,"cmd_encode(notice) fail\n"); close(s); return -4;
		}
		
		/* 上报通知成功 */
		if(g_ack_cb){
			resp_frame_t ack_rsp;
			ack_rsp.resp1 = CMD_CODE_FTP_NOTICE;
			ack_rsp.resp2 = 0; /* 成功 */
			g_ack_cb(0, CMD_CODE_FTP_NOTICE, &ack_rsp, g_request_id, g_user_data);
		}
		if(udp_sendto_all(s, &peer, snd, L)!=0){ perror("sendto(notice)"); close(s); return -5; }
		//printf("[NOTICE] sent try=%d\n", attempts);
		
		int n = udp_recvfrom_tmo(s, rcv, sizeof rcv, notice_tmo, &from);
		if(n<=0){ /* timeout */ continue; }
		
		resp_frame_t rsp;
		if(resp_decode(rcv, (size_t)n, &rsp)!=0)      { /* bad frame */ continue; }
		if(rsp.resp1 != CMD_CODE_FTP_NOTICE)          { /* not mine  */ continue; }
		if(rsp2_is_error(rsp.resp2))                  { /* peer err  */ continue; }
		
		ok=1; break;
	}
	if(!ok){ /* 通知阶段失败 */ close(s); return -6; }
	
	/* 2) 周期（≥3s）发 进度查询(0x01CC) 直到 完成/错误 */
	uint64_t last=0;
	for(;;){
		if(now_ms() - last < (uint64_t)query_itval){ msleep(50); continue; }
		last = now_ms();
		
		cmd_frame_t q={0}; q.type=CMD_TYPE_CUSTOM; q.seq++; q.code=CMD_CODE_PROGRESS_QUERY;
		size_t qlen = enc_query_params(pay, sizeof pay, cfg);
		q.params=pay; q.param_len=(uint8_t)qlen;
		
		if(cmd_encode(&q, snd, sizeof snd, &L)!=0){ close(s); return -7; }
		if(udp_sendto_all(s, &peer, snd, L)!=0){ close(s); return -8; }
		
		int n = udp_recvfrom_tmo(s, rcv, sizeof rcv, query_tmo, &from);
		if(n<=0) continue; /* 超时就下轮查询 */
		
		resp_frame_t rsp;
		if(resp_decode(rcv, (size_t)n, &rsp)!=0)      { continue; }
		if(rsp.resp1 != CMD_CODE_PROGRESS_QUERY)      { continue; }
		
		if(rsp2_is_error(rsp.resp2)) {
            /* 上报错误 */
            if(g_ack_cb){
                g_ack_cb(0, CMD_CODE_PROGRESS_QUERY, &rsp, g_request_id, g_user_data);
            }
            close(s); return -9; 
        }
        if(rsp2_is_done (rsp.resp2)) {
            /* 上报完成 */
            if(g_ack_cb){
                g_ack_cb(0, CMD_CODE_PROGRESS_QUERY, &rsp, g_request_id, g_user_data);
            }
            break;           /* 完成 */
        }
		/* 进行中 -> 继续下一轮 */
	}
	
	close(s);
	return 0;
}

