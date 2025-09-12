#include "../include/data_frame_rs422.h"
#include <stdlib.h> // ����malloc��free

// ��ʼ������֡��ָ���ʼ��ΪNULL��
void data_frame_init(DataFrame *frame, uint16_t apid, uint8_t grid)
{
    if (frame == (void *)0)
        return;
    frame->header.id = DATA_FRAME_ID;
    frame->header.version = 0b000;     // �̶��汾
    frame->header.type = 0b0;          // �̶�����
    frame->header.sub_header = 0b0;    // �޸���ͷ
    frame->header.apid = apid & 0x7FF; // ��������11λ
    frame->header.grid = grid & 0x03;  // ��������2λ
    frame->header.nuid = 0x0000;       // ��ʼ���м���
    frame->header.data_len = 0x0000;   // ��ʼ���ݳ���
    frame->data = (void *)0;           // ָ���ʼ��ΪNULL
    frame->checksum = 0x0000;
}

// ��������֡У��ͣ����ڶ�̬���飩
uint16_t data_calc_checksum(const DataFrame *frame)
{
    if (frame == (void *)0 || frame->data == (void *)0)
        return 0x0000;
    uint32_t sum = 0x000000; // ��32λ�������

    // �ۼ�����ͷ�����ֽڲ�֣�
    sum += (uint8_t)(frame->header.id >> 8);
    sum += (uint8_t)(frame->header.id & 0xFF);
    sum += (uint8_t)((frame->header.version << 5) |
                     (frame->header.type << 4) |
                     (frame->header.sub_header << 3) |
                     ((frame->header.apid >> 8) & 0x07)); // apid��3λ
    sum += (uint8_t)(frame->header.apid & 0xFF);          // apid��8λ
    sum += (uint8_t)((frame->header.grid << 6) |
                     ((frame->header.nuid >> 8) & 0x3F)); // grid+nuid��6λ
    sum += (uint8_t)(frame->header.nuid & 0xFF);          // nuid��8λ
    sum += (uint8_t)(frame->header.data_len >> 8);
    sum += (uint8_t)(frame->header.data_len & 0xFF);

    // �ۼ������򣨰��ֽڣ�����data_len��
    for (uint16_t i = 0; i <= frame->header.data_len; i++)
    {
        // data_len=ʵ�ʳ���-1
        sum += frame->data[i];
    }

    return (uint16_t)(sum & 0xFFFF); // ȡ��16λ
}

// ��������֡���ݣ���̬�����ڴ棩
void data_frame_set_data(DataFrame *frame, const uint8_t *data, uint16_t data_len)
{
    if (frame == (void *)0 || data == (void *)0)
        return;

    // �ͷ������ڴ棨��ֹ�ڴ�й©��
    if (frame->data != (void *)0)
    {
        free(frame->data);
        frame->data = (void *)0;
    }

    // ���ݳ���У�飺���������ֵ����Ϊż���ֽ�
    uint16_t len = (data_len > MAX_DATA_LEN) ? MAX_DATA_LEN : data_len;
    len = (len % 2 != 0) ? len - 1 : len; // ȷ��ż���ֽ�
    if (len == 0)
        return; // ����2�ֽڣ��ĵ�Ҫ��

    // ��̬�����ڴ�
    frame->data = (uint8_t *)malloc(len);
    if (frame->data == (void *)0)
        return; // ����ʧ��

    // ��������
    for (uint16_t i = 0; i < len; i++)
    {
        frame->data[i] = data[i];
    }
    frame->header.data_len = len - 1; // �����򳤶�=ʵ�ʳ���-1
    frame->checksum = data_calc_checksum(frame);
}

// �ͷ�����֡��̬�ڴ�
void data_frame_free(DataFrame *frame)
{
    if (frame == (void *)0)
        return;
    if (frame->data != (void *)0)
    {
        free(frame->data);
        frame->data = (void *)0; // ����Ұָ��
    }
    frame->header.data_len = 0x0000;
    frame->checksum = 0x0000;
}

// ����֡���룺���ṹ�����л�Ϊ�ֽ���������ʵ�ʳ��ȣ���У��ͣ�
uint16_t data_frame_encode(const DataFrame *frame, uint8_t *buf, uint16_t buf_len)
{
    if (frame == NULL || buf == NULL || frame->data == NULL)
    {
        return 0;
    }

    // �������軺�������ȣ���ͷ 8 �ֽ� + ������data_len+1�� + У��� 2 �ֽ�
    uint16_t required_len = 8 + (frame->header.data_len + 1) + 2;

    if (buf_len < required_len)
    {
        return 0; // ����������
    }

    uint16_t pos = 0;
    // ����ͷ����ʶ����16bit����ˣ�
    buf[pos++] = (frame->header.id >> 8) & 0xFF;
    buf[pos++] = frame->header.id & 0xFF;
    // �汾�� (3bit)+ ���� (1bit)+ ����ͷ��ʶ (1bit)+APID �� 3bit (3bit)
    uint8_t byte1 = 0;
    byte1 |= (frame->header.version & 0x07) << 5;    // �汾��ռ bit5-7
    byte1 |= (frame->header.type & 0x01) << 4;       // ����ռ bit4
    byte1 |= (frame->header.sub_header & 0x01) << 3; // ����ͷ��ʶռ bit3
    byte1 |= (frame->header.apid >> 8) & 0x07;       // APID �� 3bit ռ bit0-2
    buf[pos++] = byte1;
    // APID �� 8bit
    buf[pos++] = frame->header.apid & 0xFF;
    // �����־ (2bit)+ Դ�����м����� 6bit
    uint8_t byte3 = 0;
    byte3 |= (frame->header.grid & 0x03) << 6; // �����־ռ bit6-7
    byte3 |= (frame->header.nuid >> 8) & 0x3F; // nuid �� 6bit ռ bit0-5
    buf[pos++] = byte3;
    // Դ�����м����� 8bit
    buf[pos++] = frame->header.nuid & 0xFF;
    // �����򳤶ȣ�16bit����ˣ�
    buf[pos++] = (frame->header.data_len >> 8) & 0xFF;
    buf[pos++] = frame->header.data_len & 0xFF;

    // ������data_len+1 �ֽڣ�
    for (uint16_t i = 0; i <= frame->header.data_len; i++)
    {
        buf[pos++] = frame->data[i];
    }

    // ���㲢д��У��ͣ����ǽṹ���п��ܵľ�ֵ��
    uint16_t checksum = data_calc_checksum(frame);
    buf[pos++] = (checksum >> 8) & 0xFF;
    buf[pos++] = checksum & 0xFF;

    return pos; // �ܳ��� = 8 + (data_len+1) + 2
}

// ����֡���룺���ֽ����������ṹ�壬���� 0 �ɹ���<0 ʧ��
int8_t data_frame_decode(DataFrame *frame, const uint8_t *buf, uint16_t buf_len)
{
    if (frame == NULL || buf == NULL || frame->data != NULL)
    {
        return -1; // ���ȵ��� init ��ʼ����data Ϊ NULL��
    }

    // ������ͷ���� 8 �ֽڣ�У��� 2 �ֽڣ����������� 1 �ֽڣ�data_len>=0��
    if (buf_len < 8 + 1 + 2)
    {
        return -2; // ���Ȳ���
    }

    // ������ʶ��
    frame->header.id = (buf[0] << 8) | buf[1];
    if (frame->header.id != DATA_FRAME_ID)
    {
        return -3; // ��ʶ������
    }

    // �����汾�š����͡�����ͷ��ʶ��APID
    uint8_t byte1 = buf[2];
    frame->header.version = (byte1 >> 5) & 0x07;
    frame->header.type = (byte1 >> 4) & 0x01;
    frame->header.sub_header = (byte1 >> 3) & 0x01;
    frame->header.apid = ((byte1 & 0x07) << 8) | buf[3]; // �� 3bit + �� 8bit
    // ���������־��Դ�����м���
    uint8_t byte3 = buf[4];
    frame->header.grid = (byte3 >> 6) & 0x03;
    frame->header.nuid = ((byte3 & 0x3F) << 8) | buf[5]; // �� 6bit + �� 8bit
    // ���������򳤶Ȳ�У��
    frame->header.data_len = (buf[6] << 8) | buf[7];
    uint16_t data_actual_len = frame->header.data_len + 1; // ʵ�����ݳ���

    // �ܳ���ӦΪ��8����ͷ�� + data_actual_len + 2��У��ͣ�
    if (buf_len != 8 + data_actual_len + 2)
    {
        return -4; // ���ݳ��Ȳ�ƥ��
    }

    // �����������ڴ沢��������
    frame->data = (uint8_t *)malloc(data_actual_len);
    if (frame->data == NULL)
    {
        return -5; // �ڴ����ʧ��
    }
    for (uint16_t i = 0; i < data_actual_len; i++)
    {
        frame->data[i] = buf[8 + i]; // 8 �ǰ�ͷ����
    }

    // У�����֤
    frame->checksum = (buf[buf_len - 2] << 8) | buf[buf_len - 1];
    uint16_t calc_checksum = data_calc_checksum(frame);
    if (frame->checksum != calc_checksum)
    {
        data_frame_free(frame); // �ͷ��ѷ����ڴ�
        return -6;              // У��ʹ���
    }

    return 0; // �ɹ�
}
