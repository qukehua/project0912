#pragma once
/*
 * ETH1 服务线程对外接口（阶段A）
 * - 负责：任务队列、线程生命周期、消息回传
 * - 不直接耦合 main 的消息队列；通过 Eth1MsgSink 回调把消息交给上层
 */

#include "eth1_task.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 初始化 ETH1 服务线程
 * - sink/user：注册消息回传回调（ETH1 -> main）
 * - 返回 0 成功；<0 失败
 */
int eth1_init(Eth1MsgSink sink, void* user);

/* 请求关闭并等待线程退出 */
void eth1_shutdown(void);

/* 入队任务（非阻塞）
 * - t：任务内容（将被复制）
 * - 返回 0 成功；-1 队列已满；-2 未初始化
 */
int eth1_enqueue_task(const Eth1Task* t);

/* 查询当前队列深度（调试用） */
int eth1_queue_size(void);

#ifdef __cplusplus
}
#endif
