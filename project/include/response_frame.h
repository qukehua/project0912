#ifndef RESPONSE_FRAME_H
#define RESPONSE_FRAME_H

#include <stdint.h>

// Ӧ��֡�̶���ʶ��
#define RESP_FRAME_ID 0xEB91

// Ӧ��֡�̶����ͱ�ʶ
#define RESP_TYPE 0xAA

// Ӧ�𳤶ȣ��̶�ֵ��
#define RESP_LEN 0x05

// Ӧ����2���壨λ������
#define RESP_NORMAL (0 << 7) // bit7��ִ������0
#define RESP_ERROR (1 << 7)  // bit7��ִ�д���1
#define TRANS_ING (0 << 0)   // bit1-0��������00
#define TRANS_DONE (1 << 0)  // bit1-0���������01
#define TRANS_ERROR (3 << 0) // bit1-0���������11

// Ӧ��֡�ṹ��
typedef struct
{
    uint16_t id;         // ��ʶ�����̶�0xEB91��
    uint8_t type;        // ���ͱ�ʶ���̶�0xAA��
    uint8_t len;         // Ӧ�𳤶ȣ��̶�0x05��
    uint16_t resp_code1; // Ӧ����1��ָ���������֡��ʶ����
    uint16_t resp_code2; // Ӧ����2��ִ�н����
    uint8_t checksum;    // ��У�飨��ͷ+�������ֽ����ȡ��8bit��
} ResponseFrame;

// ��������

/**
 * @brief ��ʼ��Ӧ��֡�ṹ��
 * @param frame ָ��ResponseFrame�ṹ���ָ��
 * @param code1 Ӧ����1��ָ���������֡��ʶ����
 * @param code2 Ӧ����2��ִ�н��������ִ��״̬�ʹ���״̬λ��
 */
void resp_frame_init(ResponseFrame *frame, uint16_t code1, uint16_t code2);

/**
 * @brief ����Ӧ��֡�ĺ�У��ֵ
 * @param frame ָ��ResponseFrame�ṹ���ָ��
 * @return ���ؼ���ó���У��ֵ����ͷ+�������ֽ����ȡ��8λ��
 */
uint8_t resp_calc_checksum(const ResponseFrame *frame);

/**
 * @brief ��Ӧ��֡����Ϊ�ֽ���
 * @param frame ָ��ResponseFrame�ṹ���ָ��
 * @param buf ���������ָ�룬���ڴ�ű���������
 * @param buf_len �������������
 * @return ����ʵ�ʱ�����ֽ�����0��ʾ����ʧ��
 */
uint16_t resp_frame_encode(const ResponseFrame *frame, uint8_t *buf, uint16_t buf_len);

/**
 * @brief ���ֽ�������ΪӦ��֡
 * @param frame ָ��ResponseFrame�ṹ���ָ�룬���ڴ�Ž�����
 * @param buf ���뻺����ָ�룬������������ֽ���
 * @param buf_len ���뻺��������
 * @return ���ؽ���״̬��>=0��ʾ�ɹ���<0��ʾʧ�ܣ���У����󡢳��Ȳ���ȣ�
 */
int8_t resp_frame_decode(ResponseFrame *frame, const uint8_t *buf, uint16_t buf_len);

#endif
