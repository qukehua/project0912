#include "../include/resp_frame.h"
static inline void put_be16(uint8_t *p, uint16_t v){ p[0]=(uint8_t)(v>>8); p[1]=(uint8_t)(v); }
static inline uint16_t get_be16(const uint8_t *p){ return (uint16_t)((p[0]<<8) | p[1]); }
static uint8_t sum8(const uint8_t *p, size_t n){ uint32_t s=0; for(size_t i=0;i<n;i++) s+=p[i]; return (uint8_t)s; }
int resp_encode(const resp_frame_t *in, uint8_t *out, size_t cap, size_t *olen){
    if(!in||!out||!olen) return -1;
    if(cap<9) return -1;
    put_be16(out+0, RESP_FRAME_IDENTIFIER);
    out[2]=RESP_TYPE; out[3]=RESP_LEN_FIXED;
    put_be16(out+4, in->resp1);
    put_be16(out+6, in->resp2);
    out[8]=sum8(out, 8);
    *olen=9; return 0;
}
int resp_decode(const uint8_t *buf, size_t len, resp_frame_t *out){
    if(!buf||!out||len!=9) return -1;
    uint16_t id = (uint16_t)((buf[0]<<8)|buf[1]);
    if(id != RESP_FRAME_IDENTIFIER) return -1;
    if(buf[2]!=RESP_TYPE || buf[3]!=RESP_LEN_FIXED) return -1;
    uint32_t s=0; for(size_t i=0;i<8;i++) s+=buf[i];
    if((uint8_t)s != buf[8]) return -1;
    out->resp1=(uint16_t)((buf[4]<<8)|buf[5]);
    out->resp2=(uint16_t)((buf[6]<<8)|buf[7]);
    return 0;
}
