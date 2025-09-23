#include "../include/protocol_tx.h"
#include "../include/cmd_frame.h"
#include "../include/data_frame_eth1.h"
#include "../include/resp_frame.h"
#include "../include/cmd_codes.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

static uint64_t now_ms(void){ struct timespec ts; clock_gettime(CLOCK_MONOTONIC,&ts); return (uint64_t)ts.tv_sec*1000ull + ts.tv_nsec/1000000ull; }

static uint16_t crc16_ccitt(const uint8_t* data, size_t len, uint16_t crc){
    for(size_t i=0;i<len;i++){
        crc ^= (uint16_t)data[i] << 8;
        for(int j=0;j<8;j++){
            if(crc & 0x8000) crc = (crc<<1) ^ 0x1021;
            else crc <<= 1;
        }
    }
    return crc;
}

typedef enum {
    ST_IDLE=0,
    ST_S_CMD_START, ST_W_ACK_START,
    ST_S_DATA,      ST_W_ACK_DATA,
    ST_S_CMD_END,   ST_W_ACK_END,
    ST_FINISHED,
    ST_ERROR
} tx_state_t;

typedef struct {
    int sock;
    tx_state_t st;
    uint64_t last_tx;
    uint8_t cmd_retry;
    uint8_t data_retry;
    uint8_t file_restart;
    FILE *fp;
    file_meta_t meta;
    uint32_t bytes_sent;
    uint32_t chunks_total;
    uint32_t chunk_idx;
    uint16_t seq_cmd;
    uint16_t seq_nul;
    proto_cfg_t cfg;
    proto_err_t last_err;
    uint8_t  txbuf[DATA_PAYLOAD_MAX + 64];
    uint8_t  scratch[DATA_PAYLOAD_MAX];
} tx_ctx_t;

static tx_ctx_t g;

static uint32_t current_timeout_ms(void){
    if(g.st == ST_W_ACK_START){
        if(g.cfg.select_start_timeout_ms) return g.cfg.select_start_timeout_ms(g.meta.device_id7);
        return g.cfg.cmd_timeout_ms;
    }else if(g.st == ST_W_ACK_DATA){
        return g.cfg.data_timeout_ms;
    }else if(g.st == ST_W_ACK_END){
        return g.cfg.cmd_timeout_ms;
    }
    return g.cfg.cmd_timeout_ms;
}

static int  compute_crc16_if_needed(void){
    if(g.meta.file_crc16!=0) return 0;
    long pos = ftell(g.fp);
    if(pos<0) return -1;
    fseek(g.fp, 0, SEEK_SET);
    uint8_t buf[4096];
    uint16_t crc = 0xFFFF;
    size_t n;
    while((n=fread(buf,1,sizeof buf,g.fp))>0){
        crc = crc16_ccitt(buf, n, crc);
    }
    g.meta.file_crc16 = crc;
    fseek(g.fp, pos, SEEK_SET);
    return 0;
}

static void send_cmd_start(void);
static void send_cmd_end(void);
static void send_next_data(void);
static void handle_ack(const resp_frame_t *r);
static void restart_transfer(void);

void proto_init(int sock_fd, const proto_cfg_t *cfg){
    memset(&g, 0, sizeof(g));
    g.sock = sock_fd;
    g.st   = ST_IDLE;
    g.cfg.cmd_timeout_ms  = 5000;
    g.cfg.data_timeout_ms = 5000;
    g.cfg.cmd_max_retry   = 3;
    g.cfg.data_max_retry  = 3;
    g.cfg.file_max_restart= 2;
    g.cfg.data_chunk_bytes= 1000;
    g.cfg.select_start_timeout_ms = NULL;
    if(cfg){
        g.cfg = *cfg;
        if(g.cfg.data_chunk_bytes==0 || g.cfg.data_chunk_bytes>1400) g.cfg.data_chunk_bytes=1000;
    }
}

int proto_start_file(const file_meta_t *meta){
    if(!meta||!meta->filepath) return -1;
    if(g.st!=ST_IDLE) return -2;

    g.meta = *meta;
    g.fp = fopen(meta->filepath, "rb");
    if(!g.fp){ g.last_err=PERR_IO_OPEN_FAIL; g.st=ST_ERROR; return -3; }

    if(compute_crc16_if_needed()!=0){ g.last_err=PERR_IO_READ_FAIL; g.st=ST_ERROR; return -4; }

    uint32_t sz = g.meta.file_size;
    uint32_t chunk = g.cfg.data_chunk_bytes;
    g.chunks_total = (sz + chunk - 1) / chunk;

    g.chunk_idx = 0; g.bytes_sent = 0;
    g.seq_cmd = 0; g.seq_nul = 0;
    g.cmd_retry = 0; g.data_retry = 0; g.file_restart = 0;
    g.last_err = PERR_NONE;

    g.st = ST_S_CMD_START;
    send_cmd_start();
    return 0;
}

void proto_on_rx(const uint8_t *buf, size_t len){
    resp_frame_t r;
    if(resp_decode(buf, len, &r)==0){
        handle_ack(&r);
    }
}

void proto_tick(void){
    if(g.st==ST_IDLE || g.st==ST_FINISHED || g.st==ST_ERROR) return;
    uint32_t tmo = current_timeout_ms();
    if(now_ms() - g.last_tx < tmo) return;

    switch(g.st){
    case ST_W_ACK_START:
        if(++g.cmd_retry <= g.cfg.cmd_max_retry){
            send_cmd_start();
        }else{
            if(g.file_restart < g.cfg.file_max_restart){
                g.file_restart++;
                restart_transfer();
            }else{
                g.last_err = PERR_CMD_START_RETRY_EXCEEDED;
                g.st = ST_ERROR;
            }
        }
        break;
    case ST_W_ACK_DATA: {
        if(++g.data_retry <= g.cfg.data_max_retry){
            /* 重发当前片：回退文件指针 */
            uint32_t offset = g.chunk_idx * g.cfg.data_chunk_bytes;
            fseek(g.fp, offset, SEEK_SET);
            send_next_data();
        }else{
            if(g.file_restart < g.cfg.file_max_restart){
                g.file_restart++;
                restart_transfer();
            }else{
                g.last_err = PERR_DATA_RETRY_EXCEEDED;
                g.st = ST_ERROR;
            }
        }
    } break;
    case ST_W_ACK_END:
        if(++g.cmd_retry <= g.cfg.cmd_max_retry){
            send_cmd_end();
        }else{
            if(g.file_restart < g.cfg.file_max_restart){
                g.file_restart++;
                restart_transfer();
            }else{
                g.last_err = PERR_CMD_END_RETRY_EXCEEDED;
                g.st = ST_ERROR;
            }
        }
        break;
    default: break;
    }
}

float proto_progress(void){
    return g.meta.file_size? (float)g.bytes_sent / (float)g.meta.file_size : 0.f;
}
int proto_is_finished(void){ return g.st==ST_FINISHED; }
int proto_is_error(void){ return g.st==ST_ERROR; }
proto_err_t proto_last_error(void){ return g.last_err; }

static void restart_transfer(void){
    if(g.fp) fseek(g.fp, 0, SEEK_SET);
    g.chunk_idx = 0; g.bytes_sent = 0;
    g.seq_cmd = 0; g.seq_nul = 0;
    g.cmd_retry = 0; g.data_retry = 0;
    g.st = ST_S_CMD_START;
    send_cmd_start();
}

static void send_cmd_start(void){
    cmd_frame_t cf={0};
    cf.type=CMD_TYPE_CUSTOM;
    cf.seq=(uint8_t)(g.seq_cmd++);
    cf.code=CMD_CODE_FILE_START;

    uint8_t p[16]={0};
    uint32_t sz = g.meta.file_size;
    uint32_t chunk = g.cfg.data_chunk_bytes;
    uint32_t segments = (sz + chunk - 1) / chunk;
    if(segments>0x3FFF) segments=0x3FFF;

    p[0]  = g.meta.device_id7 & 0x7F;
    p[1]  = (uint8_t)(((g.meta.file_type & 0x0F)<<4) | (g.meta.version & 0x0F));
    p[2]  = g.meta.storage_area;
    p[3]  = (segments>1)? 1:0;
    p[4]  = (uint8_t)((segments>>8)&0x3F);
    p[5]  = (uint8_t)(segments & 0xFF);
    p[6]  = (uint8_t)(sz>>24); p[7]=(uint8_t)(sz>>16); p[8]=(uint8_t)(sz>>8); p[9]=(uint8_t)sz;
    p[10] = p[11] = p[12] = p[13] = 0x00;
    p[14] = (uint8_t)(g.meta.file_crc16>>8); p[15]=(uint8_t)(g.meta.file_crc16);

    cf.params=p; cf.param_len=sizeof p;

    size_t L=0;
    if(cmd_encode(&cf, g.txbuf, sizeof g.txbuf, &L)==0){
        send(g.sock, g.txbuf, L, 0);
        g.last_tx=now_ms();
        g.st = ST_W_ACK_START;
    }else{
        g.last_err=PERR_INTERNAL_BUG; g.st=ST_ERROR;
    }
}

static void send_next_data(void){
    if(g.chunk_idx >= g.chunks_total){
        g.st = ST_S_CMD_END;
        send_cmd_end();
        return;
    }

    uint32_t chunk = g.cfg.data_chunk_bytes;
    uint32_t remain = g.meta.file_size - g.bytes_sent;
    uint32_t want = remain < chunk ? remain : chunk;

    size_t n = fread(g.scratch, 1, want, g.fp);
    if(n != want){ g.last_err=PERR_IO_READ_FAIL; g.st=ST_ERROR; return; }

    data_frame_t df={0};
    df.apid = (uint16_t)(g.meta.apid & 0x07FF);
    if(g.chunks_total==1) df.grid=GRID_SINGLE;
    else if(g.chunk_idx==0) df.grid=GRID_FIRST;
    else if(g.chunk_idx+1==g.chunks_total) df.grid=GRID_LAST;
    else df.grid=GRID_MIDDLE;
    df.nul_id = g.seq_nul++ & 0x3FFF;
    df.payload = g.scratch;
    df.payload_len = (uint16_t)n;

    size_t L=0;
    if(data_encode(&df, g.txbuf, sizeof g.txbuf, &L)==0){
        send(g.sock, g.txbuf, L, 0);
        g.last_tx=now_ms();
        g.st = ST_W_ACK_DATA;
    }else{
        g.last_err=PERR_INTERNAL_BUG; g.st=ST_ERROR;
    }
}

static void send_cmd_end(void){
    cmd_frame_t cf={0};
    cf.type=CMD_TYPE_CUSTOM;
    cf.seq=(uint8_t)(g.seq_cmd++);
    cf.code=CMD_CODE_FILE_END;
    uint8_t p2[2];
    p2[0] = g.meta.device_id7 & 0x7F;
    p2[1] = (uint8_t)(((g.meta.file_type & 0x0F)<<4) | (g.meta.version & 0x0F));
    cf.params=p2; cf.param_len=sizeof p2;
    size_t L=0;
    if(cmd_encode(&cf, g.txbuf, sizeof g.txbuf, &L)==0){
        send(g.sock, g.txbuf, L, 0);
        g.last_tx=now_ms();
        g.st = ST_W_ACK_END;
    }else{
        g.last_err=PERR_INTERNAL_BUG; g.st=ST_ERROR;
    }
}

static void handle_ack(const resp_frame_t *r){
    if((r->resp2 & 0x80) != 0){
        if(g.file_restart < g.cfg.file_max_restart){
            g.file_restart++;
            restart_transfer();
        }else{
            g.last_err = PERR_CMD_START_RETRY_EXCEEDED;
            g.st = ST_ERROR;
        }
        return;
    }

    switch(g.st){
    case ST_W_ACK_START:
        if(r->resp1 != CMD_CODE_FILE_START) return;
        g.cmd_retry = 0;
        g.st = ST_S_DATA;
        send_next_data();
        break;
    case ST_W_ACK_DATA:
        g.data_retry=0;
        g.bytes_sent += (g.cfg.data_chunk_bytes < (g.meta.file_size - g.bytes_sent)) ?
                        g.cfg.data_chunk_bytes : (g.meta.file_size - g.bytes_sent);
        g.chunk_idx++;
        g.st = ST_S_DATA;
        send_next_data();
        break;
    case ST_W_ACK_END:
        if(r->resp1 != CMD_CODE_FILE_END) return;
        g.cmd_retry=0;
        g.st = ST_FINISHED;
        if(g.fp){ fclose(g.fp); g.fp=NULL; }
        break;
    default: break;
    }
}
