#include "../include/response_frame.h"
#include <stdlib.h>  // 定义NULL

// ��ʼ��Ӧ��֡
void resp_frame_init(ResponseFrame *frame, uint16_t code1, uint16_t code2)
{
    if (frame == (void *)0)
        return;
    frame->id = RESP_FRAME_ID;
    frame->type = RESP_TYPE;
    frame->len = RESP_LEN;
    frame->resp_code1 = code1;
    frame->resp_code2 = code2;
    frame->checksum = resp_calc_checksum(frame);
}

// ����Ӧ��֡У���
uint8_t resp_calc_checksum(const ResponseFrame *frame)
{
    if (frame == (void *)0)
        return 0x00;
    uint8_t sum = 0x00;
    // �ۼӰ�ͷ��id���2�ֽڣ�type��len��
    sum += (uint8_t)(frame->id >> 8);
    sum += (uint8_t)(frame->id & 0xFF);
    sum += frame->type;
    sum += frame->len;
    // �ۼ�������resp_code1���2�ֽڣ�resp_code2���2�ֽڣ�
    sum += (uint8_t)(frame->resp_code1 >> 8);
    sum += (uint8_t)(frame->resp_code1 & 0xFF);
    sum += (uint8_t)(frame->resp_code2 >> 8);
    sum += (uint8_t)(frame->resp_code2 & 0xFF);
    return sum & 0xFF; // ȡ��8λ
}

// Ӧ��֡���룺���ṹ�����л�Ϊ�ֽ���������ʵ�ʳ��ȣ���У��ͣ�
uint16_t resp_frame_encode(const ResponseFrame *frame, uint8_t *buf, uint16_t buf_len)
{
    if (frame == NULL || buf == NULL || buf_len < 9)
    { // �̶�����9�ֽ�
        return 0;
    }

    uint16_t pos = 0;
    // ��ͷ����ʶ����16bit����ˣ�
    buf[pos++] = (frame->id >> 8) & 0xFF;
    buf[pos++] = frame->id & 0xFF;
    // ���ͱ�ʶ��8bit��
    buf[pos++] = frame->type;
    // Ӧ�𳤶ȣ�8bit��
    buf[pos++] = frame->len;

    // ������Ӧ����1��16bit����ˣ�
    buf[pos++] = (frame->resp_code1 >> 8) & 0xFF;
    buf[pos++] = frame->resp_code1 & 0xFF;
    // Ӧ����2��16bit����ˣ�
    buf[pos++] = (frame->resp_code2 >> 8) & 0xFF;
    buf[pos++] = frame->resp_code2 & 0xFF;

    // ���㲢д��У��ͣ����ǽṹ���п��ܵľ�ֵ��
    uint8_t checksum = resp_calc_checksum(frame);
    buf[pos++] = checksum;

    return pos; // �̶�����9�ֽ�
}

// Ӧ��֡���룺���ֽ����������ṹ�壬����0�ɹ���<0ʧ��
int8_t resp_frame_decode(ResponseFrame *frame, const uint8_t *buf, uint16_t buf_len)
{
    if (frame == NULL || buf == NULL || buf_len != 9)
    {              // �̶�����9�ֽ�
        return -1; // ���ȴ���
    }

    // ������ʶ��
    frame->id = (buf[0] << 8) | buf[1];
    if (frame->id != RESP_FRAME_ID)
    {
        return -2; // ��ʶ������
    }

    // �������ͺͳ���
    frame->type = buf[2];
    frame->len = buf[3];
    if (frame->len != RESP_LEN)
    {
        return -3; // Ӧ�𳤶ȴ���
    }

    // ����Ӧ����
    frame->resp_code1 = (buf[4] << 8) | buf[5];
    frame->resp_code2 = (buf[6] << 8) | buf[7];

    // У�����֤
    frame->checksum = buf[8];
    uint8_t calc_checksum = resp_calc_checksum(frame);
    if (frame->checksum != calc_checksum)
    {
        return -4; // У��ʹ���
    }

    return 0; // �ɹ�
}
