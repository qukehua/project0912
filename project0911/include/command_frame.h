#ifndef COMMAND_FRAME_H
#define COMMAND_FRAME_H

#include <stdint.h>

// ָ��֡�̶���ʶ��
#define CMD_FRAME_ID 0xEB90

// ���ͱ�ʶ
#define CMD_TYPE_STANDARD 0x55 // �淶����
#define CMD_TYPE_CUSTOM 0xAA   // ���ж���

// �ص�ָ���루16bit��
#define CMD_FILE_START 0x0155     // �ļ����俪��
#define CMD_FILE_END 0x01AA       // �ļ��������
#define CMD_FILE_UPDATE 0x01BB    // �ļ�����֪ͨ
#define CMD_PROGRESS_QUERY 0x01CC // �ļ����½��Ȳ�ѯ

// �豸��ʶ��APID��7λ�������ƣ�   �������͵�4λ  0001~1111
#define APID_SYS_ELEC 0000000   // �ۺϵ������
#define APID_BASEBAND 0011000   // �����������
#define APID_AUTOMNG 0011011    // �����������
#define APID_INTERCOM1 0101000  // �Ǽ�ͨ���ն�1
#define APID_INTERCOM2 0101011  // �Ǽ�ͨ���ն�2
#define APID_INTERCOM3 0101101  // �Ǽ�ͨ���ն�3
#define APID_INTERCOM4 0101110  // �Ǽ�ͨ���ն�4
#define APID_ROUTE 0110101      // ·�ɽ������
#define APID_POWER_CTRL 0110110 // ��������������
#define APID_SECURITY 0111001   // ���ϰ�ȫ���
#define APID_COMPUTE 0111010    // ͨ�ü������

// �����ļ����ͣ�APID��4λ��
#define FILE_RECONFIG 0b0000        // �ع�����
#define FILE_BEAM_PLAN 0b0001       // ��λ�滮
#define FILE_ORBIT_ALARM 0b0010     // ���������Ԥ������
#define FILE_SPECTRUM 0b0011        // Ƶ�׼������
#define FILE_LASER_IMG 0b0100       // �Ǽ伤���ն�ͼ����Ϣ
#define FILE_POWER_PLAN 0b0101      // �ǵ�����ƻ�
#define FILE_EPHEMERIS_BATCH 0b0110 // ֱ�ӽ���������������
#define FILE_IP_UPDATE 0b0111       // ����IP����
#define FILE_EPHEMERIS 0b1000       // ��������

// ָ��֡�ṹ��
typedef struct
{
    uint16_t id;         // ��ʶ�����̶�0xEB90��
    uint8_t type;        // ���ͱ�ʶ
    uint8_t seq_count;   // ָ�����м�����0x00~0xFF��
    uint8_t cmd_len;     // ָ���򳤶ȣ�0x02~0xFF��
    uint16_t cmd_code;   // ָ����
    uint8_t params[254]; // �������254�ֽڣ�
    uint8_t checksum;    // ��У�飨��ͷ+ָ�����ֽ����ȡ��8bit��
} CommandFrame;

// ��������

/**
 * @brief ��ʼ��ָ��֡�ṹ��
 * @param frame ָ��CommandFrame�ṹ���ָ��
 * @param type ���ͱ�ʶ��CMD_TYPE_STANDARD��CMD_TYPE_CUSTOM��
 * @param cmd_code ָ���루16λ��
 */
void cmd_frame_init(CommandFrame *frame, uint8_t type, uint16_t cmd_code);

/**
 * @brief ����ָ��֡�ĺ�У��ֵ
 * @param frame ָ��CommandFrame�ṹ���ָ��
 * @return ���ؼ���ó���У��ֵ����ͷ+ָ�����ֽ����ȡ��8λ��
 */
uint8_t cmd_calc_checksum(const CommandFrame *frame);

/**
 * @brief ����ָ��֡�Ĳ�������
 * @param frame ָ��CommandFrame�ṹ���ָ��
 * @param params �������ݻ�����ָ��
 * @param param_len �������ݳ��ȣ����254�ֽڣ�
 */
void cmd_frame_set_param(CommandFrame *frame, const uint8_t *params, uint8_t param_len);

/**
 * @brief ��ָ��֡����Ϊ�ֽ���
 * @param frame ָ��CommandFrame�ṹ���ָ��
 * @param buf ���������ָ�룬���ڴ�ű���������
 * @param buf_len �������������
 * @return ����ʵ�ʱ�����ֽ�����0��ʾ����ʧ��
 */
uint16_t cmd_frame_encode(const CommandFrame *frame, uint8_t *buf, uint16_t buf_len);

/**
 * @brief ���ֽ�������Ϊָ��֡
 * @param frame ָ��CommandFrame�ṹ���ָ�룬���ڴ�Ž�����
 * @param buf ���뻺����ָ�룬������������ֽ���
 * @param buf_len ���뻺��������
 * @return ���ؽ���״̬��>=0��ʾ�ɹ���<0��ʾʧ�ܣ���У����󡢳��Ȳ���ȣ�
 */
int8_t cmd_frame_decode(CommandFrame *frame, const uint8_t *buf, uint16_t buf_len);

#endif