#include "../include/cmd_frame.h"
static inline void put_be16(uint8_t *p, uint16_t v){ p[0]=(uint8_t)(v>>8); p[1]=(uint8_t)(v); }
static inline uint16_t get_be16(const uint8_t *p){ return (uint16_t)((p[0]<<8) | p[1]); }
static uint8_t sum8(const uint8_t *p, size_t n){ uint32_t s=0; for(size_t i=0;i<n;i++) s+=p[i]; return (uint8_t)s; }

int cmd_encode(const cmd_frame_t *in, uint8_t *out, size_t cap, size_t *olen){
    if(!in||!out||!olen) return -1;
    if(in->param_len>254) return -3;
    size_t pay = 2 + in->param_len;
    size_t total = 2+1+1+1 + pay + 1;
    if(cap<total) return -2;
    out[0] = (uint8_t)(CMD_FRAME_IDENTIFIER>>8);
    out[1] = (uint8_t)(CMD_FRAME_IDENTIFIER);
    out[2]=in->type; out[3]=in->seq; out[4]=(uint8_t)pay;
    out[5] = (uint8_t)(in->code>>8);
    out[6] = (uint8_t)(in->code);
    for(uint8_t i=0;i<in->param_len;i++) out[7+i] = in->params? in->params[i] : 0;
    out[7+in->param_len] = sum8(out, 7+in->param_len);
    *olen = total;
    return 0;
}

int cmd_decode(const uint8_t *buf, size_t len, cmd_frame_t *out, uint8_t *param_store, size_t cap){
    if(!buf||!out||len<2+1+1+1+2+1) return -1;
    uint16_t id = (uint16_t)((buf[0]<<8)|buf[1]);
    if(id != CMD_FRAME_IDENTIFIER) return -2;
    uint8_t pay = buf[4];
    if(len < (size_t)(2+1+1+1 + pay + 1)) return -1;
    uint32_t s=0; for(size_t i=0;i<2+1+1+1+pay;i++) s+=buf[i];
    if((uint8_t)s != buf[2+1+1+1+pay]) return -3;
    out->type=buf[2]; out->seq=buf[3];
    out->code = (uint16_t)((buf[5]<<8)|buf[6]);
    out->param_len = (uint8_t)((pay>=2)? pay-2 : 0);
    if(out->param_len){
        if(!param_store || cap<out->param_len) return -4;
        for(uint8_t i=0;i<out->param_len;i++) param_store[i] = buf[7+i];
        out->params = param_store;
    }else out->params = NULL;
    return 0;
}
