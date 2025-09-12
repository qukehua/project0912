#ifndef DATA_FRAME_H
#define DATA_FRAME_H

#include <stdint.h>

// ����֡�̶���ʶ��
#define DATA_FRAME_ID 0x1ACF
// ������ݳ��ȣ��ĵ������65536�ֽڣ�
#define MAX_DATA_LEN 65536

// �����־��GrID��
#define GRID_FIRST 0b01  // �׶Σ���֡��ʼ��
#define GRID_MIDDLE 0b00 // �м��
#define GRID_LAST 0b10   // β�Σ���֡������
#define GRID_SINGLE 0b11 // ��֡��δ�ֶΣ�

// ����֡����ͷ��λ���壩
typedef struct
{
    uint16_t id;            // ��ʶ�����̶�0x1ACF��
    uint8_t version : 3;    // �汾�ţ��̶�0b000��
    uint8_t type : 1;       // ���ͣ��̶�0b0��
    uint8_t sub_header : 1; // ����ͷ��ʶ���̶�0b0��
    uint16_t apid : 11;     // Ӧ�ù��̱�ʶ��11bit��
    uint8_t grid : 2;       // �����־
    uint16_t nuid : 14;     // Դ�����м���
    uint16_t data_len;      // �����򳤶ȣ������ݳ��ȼ�1��
} DataHeader;

// ����֡�ṹ�壨��̬����ָ����ʽ��
typedef struct
{
    DataHeader header; // ����ͷ
    uint8_t *data;     // ������ָ�루��̬���䣩
    uint16_t checksum; // ��У�飨��ͷ+������˫�ֽ����ȡ��16bit��
} DataFrame;

// ��������

/**
 * @brief ��ʼ������֡�ṹ��
 * @param frame ָ��DataFrame�ṹ���ָ��
 * @param apid Ӧ�ù��̱�ʶ��11λ��0~2047��
 * @param grid �����־��GRID_FIRST/GRID_MIDDLE/GRID_LAST/GRID_SINGLE��
 */
void data_frame_init(DataFrame *frame, uint16_t apid, uint8_t grid);

/**
 * @brief ��������֡�ĺ�У��ֵ
 * @param frame ָ��DataFrame�ṹ���ָ��
 * @return ���ؼ���ó���У��ֵ����ͷ+������˫�ֽ����ȡ��16λ��
 */
uint16_t data_calc_checksum(const DataFrame *frame);

/**
 * @brief ��������֡������������
 * @param frame ָ��DataFrame�ṹ���ָ��
 * @param data ���ݻ�����ָ��
 * @param data_len ���ݳ��ȣ����65536�ֽڣ�
 * @note �ú����ᶯ̬�����ڴ�洢���ݣ�ʹ�ú������data_frame_free�ͷ�
 */
void data_frame_set_data(DataFrame *frame, const uint8_t *data, uint16_t data_len);

/**
 * @brief �ͷ�����֡�Ķ�̬�ڴ�
 * @param frame ָ��DataFrame�ṹ���ָ��
 * @note �ͷ�data�ֶζ�̬������ڴ棬��ֹ�ڴ�й©
 */
void data_frame_free(DataFrame *frame);

/**
 * @brief ������֡����Ϊ�ֽ���
 * @param frame ָ��DataFrame�ṹ���ָ��
 * @param buf ���������ָ�룬���ڴ�ű���������
 * @param buf_len �������������
 * @return ����ʵ�ʱ�����ֽ�����0��ʾ����ʧ��
 */
uint16_t data_frame_encode(const DataFrame *frame, uint8_t *buf, uint16_t buf_len);

/**
 * @brief ���ֽ�������Ϊ����֡
 * @param frame ָ��DataFrame�ṹ���ָ�룬���ڴ�Ž�����
 * @param buf ���뻺����ָ�룬������������ֽ���
 * @param buf_len ���뻺��������
 * @return ���ؽ���״̬��>=0��ʾ�ɹ���<0��ʾʧ�ܣ���У����󡢳��Ȳ���ȣ�
 * @note ����ɹ���ᶯ̬�����ڴ�洢���ݣ�ʹ�ú������data_frame_free�ͷ�
 */
int8_t data_frame_decode(DataFrame *frame, const uint8_t *buf, uint16_t buf_len);

#endif
