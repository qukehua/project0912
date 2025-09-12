#define _GNU_SOURCE
#include "../include/ftp_server.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

/* 简单进程管理：优先尝试 vsftpd；没有则回退 busybox ftpd */
static pid_t g_pid = -1;

static int have_cmd(const char* exe){
	char cmd[256];
	snprintf(cmd, sizeof(cmd), "command -v %s >/dev/null 2>&1", exe);
	return system(cmd) == 0;
}

static int write_vsftpd_conf(const ftp_srv_cfg_t* c, char* outpath, size_t cap){
	snprintf(outpath, cap, "/tmp/vsftpd_eth1.conf");
	FILE* f = fopen(outpath, "w");
	if (!f) return -1;
	fprintf(f,
		"listen=YES\n"
		"background=YES\n"
		"listen_address=%s\n"
		"listen_port=%u\n"
		"anonymous_enable=NO\n"
		"local_enable=YES\n"
		"write_enable=%s\n"
		"local_umask=022\n"
		"dirmessage_enable=NO\n"
		"xferlog_enable=YES\n"
		"connect_from_port_20=YES\n"
		"chroot_local_user=YES\n"
		"allow_writeable_chroot=YES\n"
		"local_root=%s\n"
		"user_sub_token=%s\n"
		"pam_service_name=vsftpd\n"
		"pasv_enable=YES\n"
		"pasv_min_port=%d\n"
		"pasv_max_port=%d\n",
		c->bind_ip, c->port,
		c->allow_write? "YES":"NO",
		c->root_dir, c->user,
		c->pasv_min, c->pasv_max
		);
	fclose(f);
	return 0;
}

int ftp_srv_start(const ftp_srv_cfg_t* cfg){
	if (!cfg) return -1;
	if (g_pid > 0) {
		/* 已记录子进程，简单认为已启动 */
		return 0;
	}
	
	if (have_cmd("vsftpd")){
		char conf[256];
		if (write_vsftpd_conf(cfg, conf, sizeof(conf)) != 0)
			return -2;
		
		pid_t pid = fork();
		if (pid < 0) return -3;
		if (pid == 0) {
			execlp("vsftpd", "vsftpd", conf, (char*)NULL);
			_exit(127);
		}
		g_pid = pid;
		return 0;
	}
	
	if (have_cmd("busybox")){
		/* BusyBox ftpd 较简单：ftpd [-w] [-S] DIR
		不能直接绑 IP/端口；如需限制请用防火墙。 */
		pid_t pid = fork();
		if (pid < 0) return -3;
		if (pid == 0) {
			if (cfg->allow_write)
				execlp("busybox","busybox","ftpd","-wS", cfg->root_dir, (char*)NULL);
			else
				execlp("busybox","busybox","ftpd","-S",  cfg->root_dir, (char*)NULL);
			_exit(127);
		}
		g_pid = pid;
		return 0;
	}
	
	fprintf(stderr, "[FTP] neither vsftpd nor busybox ftpd found\n");
	return -4;
}

int ftp_srv_stop(void){
	if (g_pid > 0) {
		kill(g_pid, SIGTERM);
		int st; (void)waitpid(g_pid, &st, 0);
		g_pid = -1;
	}
	return 0;
}

int ftp_srv_ensure(const ftp_srv_cfg_t* cfg){
	if (g_pid > 0 && kill(g_pid, 0) == 0) return 0; /* 仍在运行 */
	g_pid = -1;
	return ftp_srv_start(cfg);
}

