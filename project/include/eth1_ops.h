#pragma once
/*
 * ETH1 具体业务操作声明
 * 阶段B：在阶段A的基础上，增加事件回调 Eth1Emitter，用于“收到对端应答就回 main”
 */
#include "eth1_task.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ===== 阶段B新增：事件发射器，把ACK/进度/完成抛回 eth1.c（再回 main） ===== */
typedef struct {
    Eth1MsgSink sink;      /* 回调函数（来自 eth1_init 注册） */
    void*       user;      /* 回调的 user 指针 */
    uint32_t    task_id;   /* 当前任务ID（回传用） */
    uint8_t     ruid;      /* 目标 ruid（回传用） */
} Eth1Emitter;

/* ---- 控制类5项：成功返回0，失败<0；o_resp 带回“应答参数区原始字节” ---- */
int eth1_sw_version_query     (const EthTarget* target, const CtrlPolicy* policy,
                               eth1_raw_resp_t* o_resp, const Eth1Emitter* em);
int eth1_factory_reset_notify (const EthTarget* target, const CtrlPolicy* policy,
                               eth1_raw_resp_t* o_resp, const Eth1Emitter* em);
int eth1_factory_reset_query  (const EthTarget* target, const CtrlPolicy* policy,
                               eth1_raw_resp_t* o_resp, const Eth1Emitter* em);
/* 若你们的 cmd_codes.h 未含回退，请在其中补宏或用兜底宏 */
int eth1_rollback_set         (const EthTarget* target, const CtrlPolicy* policy,
                               eth1_raw_resp_t* o_resp, const Eth1Emitter* em);
int eth1_rollback_query       (const EthTarget* target, const CtrlPolicy* policy,
                               eth1_raw_resp_t* o_resp, const Eth1Emitter* em);

/* ---- UDP 控制 FTP（单独暴露，供重构编排调用） ---- */
int eth1_run_udp_ctrl_ftp     (const EthTarget* target, const CtrlPolicy* policy,
                               const FileTransferSpec* spec,
                               eth1_raw_resp_t* o_last_resp, const Eth1Emitter* em);

/* ---- UDP 分片直传（单独暴露，供重构编排调用） ---- */
int eth1_run_udp_segmented    (const EthTarget* target, const CtrlPolicy* policy,
                               const FileTransferSpec* spec,
                               eth1_raw_resp_t* o_last_resp, const Eth1Emitter* em);

/* ---- 重构编排（准备→文件传输→开始；开始无需等待ACK） ---- */
int eth1_reconfig_execute     (const ReconfigPlan* plan,
                               eth1_raw_resp_t* o_last_resp, const Eth1Emitter* em);

#ifdef __cplusplus
}
#endif
