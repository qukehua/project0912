/* ftp_demo.c — 最小联调示例：UDP 控制下 FTP（通知 + 进度查询）
 *
 * 依赖：
 *   - udp_sm.h / udp_sm.c
 *   - cmd_codes.h
 *   - APID.h
 *
 * 目标：向“基带处理组件”发起 FTP 文件更新通知（读取方向），
 *      随后按 3s 周期轮询进度，直到完成(0x11)或异常(0xFF)。
 *
 * 构建示例：
 *   gcc -std=c11 -O2 -Wall -Wextra -o ftp_demo \
 *       ftp_demo.c udp_sm.c cmd_frame.c data_frame.c \
 *       -lpthread
 *
 * 运行示例（默认参数可直接运行）：
 *   ./ftp_demo
 *
 * 可选命令行覆盖：
 *   ./ftp_demo [ipv6] [port] [dev7(hex)] [op(read|write)] [file_type(hex)] [sub_type(hex)] [path]
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <time.h>

#include "udp_sm.h"
#include "cmd_codes.h"
#include "APID.h"

/* ==================== 默认参数 ==================== */
static const char* DEF_PEER_IP6   = "FC00::5A39:0002";
static const uint16_t DEF_PEER_PORT = 10030;
/* 基带 dev7：取 APID.h 中的 NIXYK_DEVID_BASEBAND */
static const uint8_t DEF_DEV7 = NIXYK_DEVID_BASEBAND;
/* 本次是“对方从我方读取文件” -> 操作类型 0x00 (READ) */
static const uint8_t DEF_OP_READ  = 0x00;
static const uint8_t DEF_OP_WRITE = 0x11;
/* 文件类型/子类型（示例） */
static const uint8_t DEF_FILE_TYPE = 0xFB;
static const uint8_t DEF_SUB_TYPE  = 0x00;
/* 目录及名称（128B ASCII，不足补 0、不得全 0） */
static const char*   DEF_FILE_PATH = "./18_fb_47";

/* 用于监听12071端口的额外套接字 */
static int monitor_fd = -1;

/* ==================== 工具：时间（ms） ==================== */
static uint64_t now_ms(void) {
  struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000ull + (uint64_t)(ts.tv_nsec / 1000000ull);
}

/* ==================== 工具：ASCII 检查（禁止 UTF-8） ==================== */
static bool is_ascii_str(const char* s){
  if(!s || !*s) return false;
  for(const unsigned char* p=(const unsigned char*)s; *p; ++p){
    if(*p > 0x7F) return false;
  }
  return true;
}

/* ==================== 工具：读取文件大小（>0） ==================== */
static int get_file_size(const char* path, uint32_t* out_size){
  struct stat st;
  if(stat(path, &st) != 0) return -1;
  if(!S_ISREG(st.st_mode)) return -2;
  if(st.st_size <= 0) return -3;
  if(st.st_size > 0xFFFFFFFFLL) return -4;
  *out_size = (uint32_t)st.st_size;
  return 0;
}



int start_vsftpd_ipv6(const char* bind_ip6_or_null,
                      const char* root_dir) {
  const char* conf = "/tmp/vsftpd_smc.conf";
  FILE* f = fopen(conf, "w");
  if (!f) return -1;
  fprintf(f,
    "listen=NO\n"
    "listen_ipv6=YES\n"
    "listen_port=10002\n"
    "background=YES\n"
    "%s%s%s"
    "anonymous_enable=YES\n"
    "no_anon_password=YES\n"
    "anon_root=%s\n"
    "write_enable=NO\n"
    "dirmessage_enable=NO\n"
    "xferlog_enable=YES\n"
    "pasv_enable=YES\n"
    "pasv_min_port=30000\n"
    "pasv_max_port=30049\n"
    "seccomp_sandbox=NO\n"
    "use_localtime=YES\n",
    bind_ip6_or_null ? "listen_address6=" : "",
    bind_ip6_or_null ? bind_ip6_or_null : "",
    bind_ip6_or_null ? "\n" : "",
    root_dir ? root_dir : "/srv/ftp"
  );
  fclose(f);

  pid_t pid = fork();
  if (pid < 0) return -2;
  if (pid == 0) { execlp("vsftpd", "vsftpd", conf, (char*)NULL); _exit(127); }
  return 0;
}


/* ==================== 构造 FTP 通知参数 ====================
 * 参数格式（按“指令-以太网”表）：
 *  参数1: 文件类型 (1B)
 *  参数2: 子类型   (1B)
 *  参数3: 文件大小 (4B, 大端) —— 必须 > 0
 *  参数4: 操作类型 (1B) —— 0x00=读取、0x11=写入
 *  参数5: 文件目录及名称 (128B ASCII，不允许 UTF-8，不足补 0，且不得全 0)
 */
static int build_ftp_notice_params(uint8_t* buf, size_t cap, size_t* out_len,
                                   uint8_t file_type, uint8_t sub_type,
                                   uint32_t file_size, uint8_t op_type,
                                   const char* file_path)
{
  if(!buf || !out_len || !file_path) return -1;
  if(cap < (size_t)(1+1+4+1+128))    return -2;
  if(file_size == 0)                 return -3;
  if(!is_ascii_str(file_path))       return -4;

  size_t off = 0;
  buf[off++] = file_type;
  buf[off++] = sub_type;
  buf[off++] = (uint8_t)((file_size >> 24) & 0xFF);
  buf[off++] = (uint8_t)((file_size >> 16) & 0xFF);
  buf[off++] = (uint8_t)((file_size >>  8) & 0xFF);
  buf[off++] = (uint8_t)( file_size        & 0xFF);
  buf[off++] = op_type;

  /* 128B 文件路径，ASCII，补 0 */
  memset(&buf[off], 0, 128);
  /* 避免溢出：最多写入 127 个可见字符，保留最后 1B 给 '\0' */
  size_t n = strlen(file_path);
  if(n > 127) n = 127;
  memcpy(&buf[off], file_path, n);
  off += 128;

  *out_len = off;
  return 0;
}

/* ==================== 发送回调：UDP/IPv6 ==================== */
typedef struct {
  int sockfd;
  struct sockaddr_in6 peer;
} udp_io_ctx_t;

static int udp_send_cb(const uint8_t* buf, size_t len, void* user) {
  udp_io_ctx_t* io = (udp_io_ctx_t*)user;
  if(!io || io->sockfd < 0) return -1;
  ssize_t n = sendto(io->sockfd, buf, len, 0,
                     (struct sockaddr*)&io->peer, sizeof(io->peer));
  return (n == (ssize_t)len) ? 0 : -1;
}

static void log_cb(const char* msg, void* user){
  (void)user; fprintf(stderr, "%s\n", msg);
}
extern int start_vsftpd_ipv6(const char*, const char*);
/* ==================== main ==================== */
int main(int argc, char** argv)
{
  // 确保根目录存在
  system("mkdir -p /srv/ftp/opt");
  // 可选：指定只监听内网 IPv6；不传则全接口监听
  if (start_vsftpd_ipv6(NULL, "/srv/ftp") != 0) {
    fprintf(stderr, "vsftpd start failed\n");
    return 1;
  }

  const char*  ip6   = (argc>1)? argv[1] : DEF_PEER_IP6;
  uint16_t     port  = (argc>2)? (uint16_t)strtoul(argv[2],NULL,0) : DEF_PEER_PORT;
  uint8_t      dev7  = (argc>3)? (uint8_t)strtoul(argv[3],NULL,16): DEF_DEV7;
  const char*  opstr = (argc>4)? argv[4] : "read";
  uint8_t      file_type = (argc>5)? (uint8_t)strtoul(argv[5],NULL,16) : DEF_FILE_TYPE;
  uint8_t      sub_type  = (argc>6)? (uint8_t)strtoul(argv[6],NULL,16) : DEF_SUB_TYPE;
  const char*  path  = (argc>7)? argv[7] : DEF_FILE_PATH;

  uint8_t op = (strcmp(opstr,"write")==0 || strcmp(opstr,"w")==0) ? DEF_OP_WRITE : DEF_OP_READ;

  /* 读取文件大小（必须 >0） */
  uint32_t fsize = 0;
  if(get_file_size(path, &fsize) != 0){
    fprintf(stderr, "[ERR] 无法获取文件大小或文件无效: %s\n", path);
    return 2;
  }

  /* 构造 FTP 通知参数 */
  uint8_t params[1+1+4+1+128];
  size_t  plen = 0;
  int brc = build_ftp_notice_params(params, sizeof(params), &plen,
                                    file_type, sub_type, fsize, op, path);
  if(brc != 0){
    fprintf(stderr, "[ERR] 构建 FTP 通知参数失败, rc=%d\n", brc);
    return 3;
  }

  /* 建立 UDP/IPv6 套接字并绑定指定范围的本地端口（用于接收应答） */
  int fd = socket(AF_INET6, SOCK_DGRAM, 0);
  if(fd < 0){ perror("socket"); return 4; }
  /* 绑定 - 使用自主管理组件UDP通信端口范围：10031~10062 */
  struct sockaddr_in6 local = {0};
  local.sin6_family = AF_INET6;
  local.sin6_port   = htons(10031);        /* 使用规则中指定的端口范围起始值 */
  local.sin6_addr   = in6addr_any;         /* :: */
  if(bind(fd, (struct sockaddr*)&local, sizeof(local)) != 0){
    perror("bind"); close(fd); return 5;
  }
  /* 获取实际绑定的本地端口号 */
  socklen_t addr_len = sizeof(local);
  if(getsockname(fd, (struct sockaddr*)&local, &addr_len) == 0){
    uint16_t actual_port = ntohs(local.sin6_port);
    printf("[FTP] 实际绑定本地端口: %u\n", actual_port);
  }

  /* 创建额外的套接字专门用于监听12071端口 */
  monitor_fd = socket(AF_INET6, SOCK_DGRAM, 0);
  if(monitor_fd < 0){
    perror("socket for monitoring");
    /* 这是非致命错误，继续使用主套接字 */
  } else {
    /* 设置套接字选项允许地址重用 */
    int optval = 1;
    if (setsockopt(monitor_fd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(optval)) < 0) {
      perror("setsockopt for monitoring");
    }
    /* 绑定到12071端口 */
    struct sockaddr_in6 monitor_local = {0};
    monitor_local.sin6_family = AF_INET6;
    monitor_local.sin6_port   = htons(12071);
    monitor_local.sin6_addr   = in6addr_any;
    if(bind(monitor_fd, (struct sockaddr*)&monitor_local, sizeof(monitor_local)) != 0){
      perror("bind for monitoring");
      close(monitor_fd);
      monitor_fd = -1;
    } else {
      printf("[FTP] 已创建监控套接字并绑定到12071端口\n");
    }
  }
  udp_io_ctx_t io = {0};
  io.sockfd = fd;
  memset(&io.peer, 0, sizeof(io.peer));
  io.peer.sin6_family = AF_INET6;
  io.peer.sin6_port   = htons(port);
  if(inet_pton(AF_INET6, ip6, &io.peer.sin6_addr) != 1){
    fprintf(stderr, "[ERR] 非法 IPv6 地址: %s\n", ip6);
    close(fd); return 6;
  }

  /* 非阻塞 + poll 轮询 */
  int flags = fcntl(fd, F_GETFL, 0);
  fcntl(fd, F_SETFL, flags | O_NONBLOCK);

  /* 初始化 FTP 状态机 */
  sm_ftp_t ftp;
  sm_ftp_init(&ftp, dev7, udp_send_cb, &io, log_cb, NULL);

  uint64_t t0 = now_ms();
  if(sm_ftp_start(&ftp,
                  CMD_CODE_FTP_NOTICE, CMD_CODE_FTP_NOTICE_ACK,
                  CMD_CODE_FTP_PROGRESS_QUERY, CMD_CODE_FTP_PROGRESS_RESP,
                  params, (uint16_t)plen, t0) != 0){
    fprintf(stderr, "[ERR] sm_ftp_start 失败\n");
    close(fd); return 7;
  }

  printf("[FTP] 目标[%s]:%u dev7=0x%02X op=%s type=0x%02X sub=0x%02X size=%u path=\"%s\"\n",
         ip6, port, dev7, (op==DEF_OP_WRITE?"write":"read"),
         file_type, sub_type, fsize, path);

  /* 主循环：收包 -> 喂状态机；轮询 -> 超时/重试/查询 */
  uint8_t rxbuf[2048];
  struct pollfd pfds[2] = {
    { .fd = fd, .events = POLLIN },
    { .fd = monitor_fd, .events = POLLIN }
  };
  int num_fds = (monitor_fd >= 0) ? 2 : 1;
  
  for(;;){
    /* 先 poll 一下，最多 100ms；随后调用 sm_ftp_poll 驱动超时/查询 */
    int pr = poll(pfds, num_fds, 100);
    if(pr > 0){
      /* 检查主套接字是否有数据 */
      if(pfds[0].revents & POLLIN){
        printf("[FTP] 主套接字等待应答\n");
        struct sockaddr_in6 from; socklen_t flen = sizeof(from);
        ssize_t n = recvfrom(fd, rxbuf, sizeof(rxbuf), 0, (struct sockaddr*)&from, &flen);
        if(n > 0){
          printf("[FTP] 主套接字收到应答\n");
          sm_ftp_on_udp(&ftp, rxbuf, (size_t)n, now_ms());
        }
      }
      
      /* 检查监控套接字是否有数据（如果已创建） */
      if(monitor_fd >= 0 && pfds[1].revents & POLLIN){
        printf("[FTP] 监控套接字收到数据\n");
        struct sockaddr_in6 from; socklen_t flen = sizeof(from);
        ssize_t n = recvfrom(monitor_fd, rxbuf, sizeof(rxbuf), 0, (struct sockaddr*)&from, &flen);
        if(n > 0){
          printf("[FTP] 处理监控套接字收到的应答\n");
          sm_ftp_on_udp(&ftp, rxbuf, (size_t)n, now_ms());
        }
      }
    }

    int prc = sm_ftp_poll(&ftp, now_ms());
    if (ftp.st == FTP_DONE){
      printf("[FTP] 完成：文件更新已完成(0x11)\n");
      break;
    }
    if (ftp.st == FTP_FAIL){
      fprintf(stderr, "[FTP] 失败：通知被拒或进度查询超次/异常\n");
      break;
    }
  }

  close(fd);
  if(monitor_fd >= 0){
    close(monitor_fd);
  }
  return (ftp.st == FTP_DONE) ? 0 : 1;
}
