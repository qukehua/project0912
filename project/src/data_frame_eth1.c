#include "../include/data_frame_eth1.h"
static inline void put_be16(uint8_t *p, uint16_t v){ p[0]=(uint8_t)(v>>8); p[1]=(uint8_t)(v); }
static inline uint16_t get_be16(const uint8_t *p){ return (uint16_t)((p[0]<<8) | p[1]); }
static uint16_t sum16(const uint8_t *p, size_t n){ uint32_t s=0; for(size_t i=0;i<n;i++) s+=p[i]; return (uint16_t)s; }

int data_encode(const data_frame_t *in, uint8_t *out, size_t cap, size_t *olen){
    if(!in||!out||!olen) return -1;
    if(!in->payload || in->payload_len==0 || in->payload_len>DATA_PAYLOAD_MAX) return -1;
    size_t total = 2+2+2+2 + in->payload_len + 2;
    if(cap<total) return -2;
    put_be16(out+0, DATA_FRAME_IDENTIFIER);
    put_be16(out+2, (uint16_t)(in->apid & 0x07FFu));
    uint16_t nul14 = (uint16_t)(in->nul_id & 0x3FFFu);
    out[4] = (uint8_t)(((in->grid & 0x3)<<6) | ((nul14>>8)&0x3F));
    out[5] = (uint8_t)(nul14 & 0xFF);
    put_be16(out+6, (uint16_t)(in->payload_len - 1));
    for(uint16_t i=0;i<in->payload_len;i++) out[8+i]=in->payload[i];
    put_be16(out+8+in->payload_len, sum16(out, 8+in->payload_len));
    *olen=total; return 0;
}

int data_decode(const uint8_t *buf, size_t len, data_frame_t *out, uint8_t *payload, size_t cap){
    if(!buf||!out||len<10) return -1;
    if(get_be16(buf)!=DATA_FRAME_IDENTIFIER) return -1;
    uint16_t apid = get_be16(buf+2) & 0x07FFu;
    uint8_t b4 = buf[4], b5 = buf[5];
    grid_t grid = (grid_t)((b4>>6)&0x3);
    uint16_t nul = (uint16_t)(((b4&0x3F)<<8)|b5);
    uint16_t n = (uint16_t)(get_be16(buf+6)+1);
    if(len != (size_t)(2+2+2+2 + n + 2)) return -1;
    uint16_t expect = sum16(buf, 8+n);
    if(expect != get_be16(buf+8+n)) return -1;
    if(payload){
        if(cap < n) return -1;
        for(uint16_t i=0;i<n;i++) payload[i]=buf[8+i];
        out->payload = payload;
    }else out->payload=NULL;
    out->apid=apid; out->grid=grid; out->nul_id=nul; out->payload_len=n;
    return 0;
}
