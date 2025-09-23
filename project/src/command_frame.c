#include "../include/command_frame.h"
#include <stdlib.h>  // 定义NULL

// ��ʼ��ָ��֡
void cmd_frame_init(CommandFrame *frame, uint8_t type, uint16_t cmd_code)
{
    if (frame == (void *)0)
        return;
    frame->id = CMD_FRAME_ID;
    frame->type = type;
    frame->seq_count = 0x00; // ��ʼ����Ϊ0
    frame->cmd_code = cmd_code;
    frame->cmd_len = 0x02; // ��ʼ���ȣ���ָ���룩
    frame->checksum = 0x00;
}

// ����ָ��֡У���
uint8_t cmd_calc_checksum(const CommandFrame *frame)
{
    if (frame == (void *)0)
        return 0x00;
    uint8_t sum = 0x00;
    // �ۼӰ�ͷ��id���2�ֽڣ�type��seq_count��cmd_len��
    sum += (uint8_t)(frame->id >> 8);
    sum += (uint8_t)(frame->id & 0xFF);
    sum += frame->type;
    sum += frame->seq_count;
    sum += frame->cmd_len;
    // �ۼ�ָ����cmd_code���2�ֽڣ�������
    sum += (uint8_t)(frame->cmd_code >> 8);
    sum += (uint8_t)(frame->cmd_code & 0xFF);
    for (uint8_t i = 0; i < (frame->cmd_len - 2); i++)
    { // cmd_len����ָ����+��������
        sum += frame->params[i];
    }
    return sum & 0xFF; // ȡ��8λ
}

// ����ָ��֡����
void cmd_frame_set_param(CommandFrame *frame, const uint8_t *params, uint8_t param_len)
{
    if (frame == (void *)0 || params == (void *)0)
        return;
    // �������Ȳ��ܳ����������ֵ��254��
    uint8_t len = (param_len > 254) ? 254 : param_len;
    for (uint8_t i = 0; i < len; i++)
    {
        frame->params[i] = params[i];
    }
    frame->cmd_len = 0x02 + len; // ָ���򳤶�=ָ���루2�ֽڣ�+��������
    frame->checksum = cmd_calc_checksum(frame);
}

// ָ��֡���룺���ṹ�����л�Ϊ�ֽ���������ʵ�ʳ��ȣ���У��ͣ�
uint16_t cmd_frame_encode(const CommandFrame *frame, uint8_t *buf, uint16_t buf_len)
{
    if (frame == NULL || buf == NULL || buf_len < (frame->cmd_len + 5))
    {
        return 0; // ���������㣨��С���ȣ���ͷ4 + ָ����cmd_len + У���1��
    }

    uint16_t pos = 0;
    // ��ͷ����ʶ����16bit����ˣ�
    buf[pos++] = (frame->id >> 8) & 0xFF;
    buf[pos++] = frame->id & 0xFF;
    // ���ͱ�ʶ��8bit��
    buf[pos++] = frame->type;
    // ָ�����м�����8bit��
    buf[pos++] = frame->seq_count;
    // ָ��ȣ�8bit��
    buf[pos++] = frame->cmd_len;

    // ָ����ָ���루16bit����ˣ�
    buf[pos++] = (frame->cmd_code >> 8) & 0xFF;
    buf[pos++] = frame->cmd_code & 0xFF;
    // ָ�������cmd_len - 2�ֽڣ���ָ����ռ2�ֽڣ�
    for (uint8_t i = 0; i < (frame->cmd_len - 2); i++)
    {
        buf[pos++] = frame->params[i];
    }

    // ���㲢д��У��ͣ����ǽṹ���п��ܵľ�ֵ��
    uint8_t checksum = cmd_calc_checksum(frame);
    buf[pos++] = checksum;

    return pos; // �ܳ��� = 4����ͷ�� + cmd_len��ָ���� + 1��У��ͣ�
}

// ָ��֡���룺���ֽ����������ṹ�壬����0�ɹ���<0ʧ��
int8_t cmd_frame_decode(CommandFrame *frame, const uint8_t *buf, uint16_t buf_len)
{
    if (frame == NULL || buf == NULL || buf_len < 7)
    {              // ��С���ȣ�4+2+1��cmd_len=2ʱ��
        return -1; // ���Ȳ���
    }

    // ������ͷ
    frame->id = (buf[0] << 8) | buf[1];
    if (frame->id != CMD_FRAME_ID)
    {
        return -2; // ��ʶ������
    }

    frame->type = buf[2];
    frame->seq_count = buf[3];
    frame->cmd_len = buf[4];
    // У��ָ��ȺϷ���
    if (frame->cmd_len < 0x02 || frame->cmd_len > 0xFF)
    {
        return -3; // ָ��ȳ�����Χ
    }
    // У���ܳ����Ƿ�ƥ�䣨4 + cmd_len + 1��
    if (buf_len != (frame->cmd_len + 5))
    {
        return -4; // �ܳ��Ȳ�ƥ��
    }

    // ����ָ����
    frame->cmd_code = (buf[5] << 8) | buf[6];
    // ��������
    uint8_t param_len = frame->cmd_len - 2;
    for (uint8_t i = 0; i < param_len; i++)
    {
        frame->params[i] = buf[7 + i]; // 7 = 5����ͷ�� + 2��ָ���룩
    }

    // У�����֤
    frame->checksum = buf[buf_len - 1];
    uint8_t calc_checksum = cmd_calc_checksum(frame);
    if (frame->checksum != calc_checksum)
    {
        return -5; // У��ʹ���
    }

    return 0; // �ɹ�
}
