#pragma once
/*
 * ruid → 端点映射（阶段A）
 * - 依据规范附录：不同 ruid（APID高7位）对应不同对端 IPv6/端口/文件数据类型码
 * - 阶段A提供静态表 + 运行期覆盖接口；后续可换成从配置文件加载
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 查询映射：
 * 返回 0 表示找到并写出 ip6/port/apid_low_data；
 * 返回 -1 表示未找到
 * 说明：ip6 缓冲区至少 40 字节（含终止符）
 */
int endpoint_query_by_ruid(uint8_t ruid, char ip6[40], uint16_t* port, uint16_t* apid_low_data);

/* 覆盖/新增一条映射（运行时注入），成功返回0 */
int endpoint_override(uint8_t ruid, const char* ip6, uint16_t port, uint16_t apid_low_data);

#ifdef __cplusplus
}
#endif
