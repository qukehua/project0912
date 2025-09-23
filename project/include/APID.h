/* xyk2112_apid.h
* APID mapping (Appendix A) for Q/NI XYK2112-2024 (Ethernet Communication Protocol — Satellite Management)
* APID = [ device_id_7bit | data_type_4bit ]
* device_id_7bit: 高7位（0..127）  |  data_type_4bit: 低4位（0..15）
* 用法：
*   uint16_t apid = NIXYK_APID(NIXYK_DEVID_GPC, NIXYK_DTYPE_CTRL); // 通用计算组件-控制指令
*   uint8_t dev7  = NIXYK_APID_DEV(apid);
*   uint8_t dt4   = NIXYK_APID_DTYPE(apid);
*/
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
	
	/* =========================
	* 低4位数据类型（Appendix A）
	* ========================= */
#define NIXYK_DTYPE_CTRL          0x0u  /* 遥控指令（控制指令） */
#define NIXYK_DTYPE_QUERY_TLM     0x1u  /* 查询遥测（需下行至地面） */
#define NIXYK_DTYPE_FAST_TLM      0x5u  /* 快包遥测 */
#define NIXYK_DTYPE_SLOW_TLM      0x6u  /* 慢包遥测 */
#define NIXYK_DTYPE_ACK           0x7u  /* 应答帧（不需要下行至地面） */
#define NIXYK_DTYPE_SLOW_TLM_2    0x8u  /* 慢包遥测2 */
#define NIXYK_DTYPE_SLOW_TLM_3    0x9u  /* 慢包遥测3 */
#define NIXYK_DTYPE_FILE          0xFu  /* 文件型传输（如重构数据/工参） */
	
	/* 若附录A对 0x2/0x3/0x4/0xA..0xE 另有定义，可在此补充。
	* 未明确者可视为保留/自定义类型，需按规范备案。 */
	
	/* =========================
	* 高7位设备标识（Appendix A）
	* 注：综合电子组件与路由交换组件在§9提到“应用层无协议”，
	* 这里仍提供其设备标识以便统一封装/过滤。
	* ========================= */
#define NIXYK_DEVID_COMPREHENSIVE 0x00u /* 综合电子组件（星务管理软件）——如附录A另有具体值，请改此处 */
#define NIXYK_DEVID_BASEBAND      0x18u /* 0011000b：基带处理组件 */
#define NIXYK_DEVID_AUTONOMY      0x1Bu /* 0011011b：自主管理组件 */
#define NIXYK_DEVID_ISL_1_MGMT    0x28u /* 0101000b：星间通信终端1（管理口） */
#define NIXYK_DEVID_ISL_2_MGMT    0x2Bu /* 0101011b：星间通信终端2（管理口） */
#define NIXYK_DEVID_ISL_3_MGMT    0x2Du /* 0101101b：星间通信终端3（管理口） */
#define NIXYK_DEVID_ISL_4_MGMT    0x2Eu /* 0101110b：星间通信终端4（管理口） */
#define NIXYK_DEVID_NAV_ENH       0x32u /* 0110010b：导航增强 */
#define NIXYK_DEVID_ROUTER        0x35u /* 0110101b：路由交换组件 */
#define NIXYK_DEVID_POWER_CTRL    0x36u /* 0110110b：星上馈电控制组件 */
#define NIXYK_DEVID_SECURITY      0x39u /* 0111001b：星上安全组件 */
#define NIXYK_DEVID_GPC           0x3Au /* 0111010b：通用计算组件（General-Purpose Compute） */
#define NIXYK_DEVID_PAYLOAD_1     0x3Cu /* 0111100b：预留载荷1 */
#define NIXYK_DEVID_PAYLOAD_2     0x3Fu /* 0111111b：预留载荷2 */
#define NIXYK_DEVID_PAYLOAD_3     0x5Eu /* 1011110b：预留载荷3 */
#define NIXYK_DEVID_PAYLOAD_4     0x60u /* 1100000b：预留载荷4 */
	
	/* =========================
	* APID 拼装/拆解工具
	* ========================= */
	static inline uint16_t NIXYK_APID(uint8_t dev7, uint8_t dtype4) {
		return (uint16_t)(((uint16_t)(dev7 & 0x7Fu) << 4) | (uint16_t)(dtype4 & 0x0Fu));
	}
	static inline uint8_t NIXYK_APID_DEV(uint16_t apid11)   { return (uint8_t)((apid11 >> 4) & 0x7Fu); }
	static inline uint8_t NIXYK_APID_DTYPE(uint16_t apid11) { return (uint8_t)( apid11        & 0x0Fu); }
	
	/* 便捷宏：常用APID组合 */
#define NIXYK_APID_CTRL(dev7)     NIXYK_APID((dev7), NIXYK_DTYPE_CTRL)
#define NIXYK_APID_FILE(dev7)     NIXYK_APID((dev7), NIXYK_DTYPE_FILE)
#define NIXYK_APID_ACK(dev7)      NIXYK_APID((dev7), NIXYK_DTYPE_ACK)
#define NIXYK_APID_FAST_TLM(dev7) NIXYK_APID((dev7), NIXYK_DTYPE_FAST_TLM)
#define NIXYK_APID_SLOW_TLM(dev7) NIXYK_APID((dev7), NIXYK_DTYPE_SLOW_TLM)
	
	/* 可选：把 dtype 映射为简短字符串（用于日志/调试） */
	static inline const char* NIXYK_dtype_str(uint8_t dt4) {
		switch (dt4 & 0x0F) {
			case NIXYK_DTYPE_CTRL:       return "CTRL";
			case NIXYK_DTYPE_QUERY_TLM:  return "QRY_TLM";
			case NIXYK_DTYPE_FAST_TLM:   return "FAST_TLM";
			case NIXYK_DTYPE_SLOW_TLM:   return "SLOW_TLM";
			case NIXYK_DTYPE_ACK:        return "ACK";
			case NIXYK_DTYPE_SLOW_TLM_2: return "SLOW_TLM2";
			case NIXYK_DTYPE_SLOW_TLM_3: return "SLOW_TLM3";
			case NIXYK_DTYPE_FILE:       return "FILE";
			default:                     return "RESERVED";
		}
	}
	
	/* 可选：把 device_id_7bit 映射为简短字符串（用于日志/调试） */
	static inline const char* NIXYK_dev7_str(uint8_t dev7) {
		switch (dev7 & 0x7F) {
			case NIXYK_DEVID_COMPREHENSIVE: return "COMPREHENSIVE";
			case NIXYK_DEVID_BASEBAND:      return "BASEBAND";
			case NIXYK_DEVID_AUTONOMY:      return "AUTONOMY";
			case NIXYK_DEVID_ISL_1_MGMT:    return "ISL1_MGMT";
			case NIXYK_DEVID_ISL_2_MGMT:    return "ISL2_MGMT";
			case NIXYK_DEVID_ISL_3_MGMT:    return "ISL3_MGMT";
			case NIXYK_DEVID_ISL_4_MGMT:    return "ISL4_MGMT";
			case NIXYK_DEVID_NAV_ENH:       return "NAV_ENH";
			case NIXYK_DEVID_ROUTER:        return "ROUTER";
			case NIXYK_DEVID_POWER_CTRL:    return "POWER_CTRL";
			case NIXYK_DEVID_SECURITY:      return "SECURITY";
			case NIXYK_DEVID_GPC:           return "GPC";
			case NIXYK_DEVID_PAYLOAD_1:     return "PAYLOAD1";
			case NIXYK_DEVID_PAYLOAD_2:     return "PAYLOAD2";
			case NIXYK_DEVID_PAYLOAD_3:     return "PAYLOAD3";
			case NIXYK_DEVID_PAYLOAD_4:     return "PAYLOAD4";
			default:                        return "UNKNOWN";
		}
	}
	
#ifdef __cplusplus
}
#endif

