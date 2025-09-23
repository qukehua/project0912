#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
	
	/* 本机 FTP 服务配置 */
	typedef struct {
		char     bind_ip[32];     /* 绑定 IP（你的 ETH1 IP） */
		uint16_t port;            /* 一般 21 */
		char     root_dir[256];   /* FTP 根目录，比如 /data/ftp */
		char     user[64];        /* 本机可登录用户（vsftpd 需系统用户） */
		int      pasv_min;        /* 被动模式端口范围（需放行防火墙） */
		int      pasv_max;
		int      allow_write;     /* 1 允许上传/删除；0 只读 */
	} ftp_srv_cfg_t;
	
	/* 启动 / 停止 / 确保已运行（若未运行则启动） */
	int ftp_srv_start (const ftp_srv_cfg_t* cfg);
	int ftp_srv_stop  (void);
	int ftp_srv_ensure(const ftp_srv_cfg_t* cfg);
	
#ifdef __cplusplus
}
#endif

