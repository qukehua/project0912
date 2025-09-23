#include "eth1.h"
#include "endpoint_map.h"
// 阶段A不包含具体业务实现；下面仅打印并回报“未实现”
// 阶段B接入 eth1_ops.h 并在调度器中调用相应函数

#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>

/* ===================== 内部队列实现（环形） ===================== */
#define ETH1_Q_CAP 64

typedef struct {
    Eth1Task items[ETH1_Q_CAP];
    int head;   /* 读指针 */
    int tail;   /* 写指针 */
    int count;  /* 当前元素数 */
    pthread_mutex_t mtx;
    pthread_cond_t  cv_put;
    pthread_cond_t  cv_get;
} TaskQueue;

static TaskQueue g_q;
static int       g_inited = 0;
static int       g_shutdown = 0;
static pthread_t g_thr;

/* 回传回调 */
static Eth1MsgSink g_sink = NULL;
static void*       g_sink_user = NULL;

/* 发送一条消息到上层 */
static void emit_msg(const Eth1Msg* msg){
    if (g_sink) g_sink(g_sink_user, msg);
}

/* 简易日志 */
static void emit_log(uint32_t task_id, const char* line){
    Eth1Msg m = {0};
    m.type = ETH1_MSG_LOG;
    m.u.log.task_id = task_id;
    snprintf(m.u.log.line, sizeof(m.u.log.line), "%s", line ? line : "");
    emit_msg(&m);
}

/* 入队/出队 */
static int q_push(const Eth1Task* t){
    pthread_mutex_lock(&g_q.mtx);
    if (g_q.count == ETH1_Q_CAP){
        pthread_mutex_unlock(&g_q.mtx);
        return -1; /* 满 */
    }
    g_q.items[g_q.tail] = *t;  /* 结构体整体拷贝 */
    g_q.tail = (g_q.tail + 1) % ETH1_Q_CAP;
    g_q.count++;
    pthread_cond_signal(&g_q.cv_get);
    pthread_mutex_unlock(&g_q.mtx);
    return 0;
}
static int q_pop(Eth1Task* t){
    pthread_mutex_lock(&g_q.mtx);
    while (g_q.count == 0 && !g_shutdown){
        pthread_cond_wait(&g_q.cv_get, &g_q.mtx);
    }
    if (g_shutdown && g_q.count == 0){
        pthread_mutex_unlock(&g_q.mtx);
        return -1;
    }
    *t = g_q.items[g_q.head];
    g_q.head = (g_q.head + 1) % ETH1_Q_CAP;
    g_q.count--;
    pthread_mutex_unlock(&g_q.mtx);
    return 0;
}

/* ===================== 线程主体 ===================== */
static void* eth1_thread(void* arg){
    (void)arg;
    emit_log(0, "[ETH1] 服务线程启动");

    while(!g_shutdown){
        Eth1Task task;
        if (q_pop(&task) != 0) break;

        /* 解析端点（仅用于日志/校验；真正发送由各实现内部处理） */
        char ip6[40]={0}; uint16_t port=0, apid_low_data=task.target.apid_low_ctrl?task.target.apid_low_ctrl:0;
        endpoint_query_by_ruid(task.target.ruid, ip6, &port, NULL);

        /* 事件发射器（把 task_id/ruid 带入） */
        Eth1Emitter em = { .sink=g_sink, .user=g_sink_user, .task_id=task.task_id, .ruid=task.target.ruid };

        char line[160];
        snprintf(line,sizeof(line),
                 "[ETH1] 处理任务 id=%u type=%d ruid=0x%02X ip6=%s port=%u",
                 task.task_id,(int)task.type,task.target.ruid, (*ip6?ip6:"<未知>"),(unsigned)port);
        emit_log(task.task_id, line);

        eth1_raw_resp_t last={0};
        int rc = -ENOSYS;

        /* >>> 阶段B：真正调用对应实现，并在完成后通过 DONE 回 main */
        switch(task.type){
            case ETH1_TASK_SWVER_QUERY:
                rc = eth1_sw_version_query(&task.target, &task.policy, &last, &em);
                break;
            case ETH1_TASK_FACTORY_RESET_NOTIFY:
                rc = eth1_factory_reset_notify(&task.target, &task.policy, &last, &em);
                break;
            case ETH1_TASK_FACTORY_RESET_QUERY:
                rc = eth1_factory_reset_query(&task.target, &task.policy, &last, &em);
                break;
            case ETH1_TASK_ROLLBACK_SET:
                rc = eth1_rollback_set(&task.target, &task.policy, &last, &em);
                break;
            case ETH1_TASK_ROLLBACK_QUERY:
                rc = eth1_rollback_query(&task.target, &task.policy, &last, &em);
                break;
            case ETH1_TASK_RECONFIG_EXECUTE:
                rc = eth1_reconfig_execute(&task.u.reconfig, &last, &em);
                break;
            default:
                rc = -ENOSYS;
                break;
        }

        /* 统一回 DONE（成功/失败 + 最后一次应答原始字节） */
        Eth1Msg m = {0};
        m.type = ETH1_MSG_DONE;
        m.u.done.task_id = task.task_id;
        m.u.done.ruid    = task.target.ruid;
        m.u.done.status  = rc;
        snprintf(m.u.done.note, sizeof(m.u.done.note),
                 (rc==0)? "OK":"ERR(%d)", rc);
        m.u.done.last_resp = last;
        emit_msg(&m);
    }

    emit_log(0, "[ETH1] 服务线程退出");
    return NULL;
}

/* ===================== 对外接口 ===================== */
int eth1_init(Eth1MsgSink sink, void* user){
    if (g_inited) return 0;

    memset(&g_q, 0, sizeof(g_q));
    pthread_mutex_init(&g_q.mtx, NULL);
    pthread_cond_init(&g_q.cv_put, NULL);
    pthread_cond_init(&g_q.cv_get, NULL);

    g_sink = sink;
    g_sink_user = user;
    g_shutdown = 0;

    if (pthread_create(&g_thr, NULL, eth1_thread, NULL) != 0){
        return -1;
    }
    g_inited = 1;
    return 0;
}

void eth1_shutdown(void){
    if (!g_inited) return;
    pthread_mutex_lock(&g_q.mtx);
    g_shutdown = 1;
    pthread_cond_broadcast(&g_q.cv_get);
    pthread_mutex_unlock(&g_q.mtx);
    pthread_join(g_thr, NULL);

    pthread_mutex_destroy(&g_q.mtx);
    pthread_cond_destroy(&g_q.cv_put);
    pthread_cond_destroy(&g_q.cv_get);

    g_inited = 0;
    g_sink = NULL;
    g_sink_user = NULL;
}

int eth1_enqueue_task(const Eth1Task* t){
    if (!g_inited) return -2;
    if (!t) return -3;
    return q_push(t);
}

int eth1_queue_size(void){
    if (!g_inited) return 0;
    pthread_mutex_lock(&g_q.mtx);
    int n = g_q.count;
    pthread_mutex_unlock(&g_q.mtx);
    return n;
}
