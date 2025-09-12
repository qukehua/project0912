#include "../include/common.h"



#include <errno.h>

// 用户结构体定义
typedef struct {
    char username[32];
    char password[32];
} User;

// 有效的用户列表
User valid_users[] = {
    {"admin", "admin123"},
    {"user", "user123"}
};



// CORS辅助函数 - 添加跨域支持头信息
void add_cors_headers(struct _u_response* response) {
    ulfius_add_header_to_response(response, "Access-Control-Allow-Origin", "*");
    ulfius_add_header_to_response(response, "Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
    ulfius_add_header_to_response(response, "Access-Control-Allow-Headers", "Content-Type, Authorization, accesstoken");
    ulfius_add_header_to_response(response, "Access-Control-Allow-Credentials", "true");
}

// OPTIONS请求处理函数 - 处理CORS预检请求
int handle_options_request(const struct _u_request* request, struct _u_response* response, void* user_data) {
    add_cors_headers(response);
    ulfius_set_empty_body_response(response, 204);
    return U_CALLBACK_CONTINUE;
}

// FTP下载进度回调函数 (在文件下方有完整实现)

// ================== 以太网0服务线程 (RESTful服务端) ==================
void* eth0_service_thread(void* arg) {
    struct _u_instance rest_instance;
    ulfius_init_instance(&rest_instance, PORT, NULL, NULL);

    // 注册路由
    ulfius_add_endpoint_by_val(&rest_instance, "POST",
        "/api/:apiVersion/securityManagement/oauth/token",
        NULL, 0, &login_callback, NULL);

    ulfius_add_endpoint_by_val(&rest_instance, "POST",
        "/api/:apiVersion/securityManagement/oauth/handshake",
        NULL, 0, &handle_handshake_request, NULL);

    ulfius_add_endpoint_by_val(&rest_instance, "DELETE",
        "/api/:apiVersion/securityManagement/oauth/token",
        NULL, 0, &logout_callback, NULL);
    // 注册软件版本查询接口
    ulfius_add_endpoint_by_val(&rest_instance, "GET",
        "/api/:apiVersion/systemManagement/softwareVersion",
        NULL, 0, &software_version_callback, NULL);
    // 注册下载接口路由
    ulfius_add_endpoint_by_val(&rest_instance, "POST",
        "/api/:apiVersion/systemManagement/softwareDownload",
        NULL, 0, &software_download_callback, NULL);
    // 注册软件下载状态查询接口
    ulfius_add_endpoint_by_val(&rest_instance, "GET",
        "/api/:apiVersion/systemManagement/softwareDownload",
        NULL, 0, &software_download_status_callback, NULL);
    // 注册软件更新接口
    ulfius_add_endpoint_by_val(&rest_instance, "POST",
        "/api/:apiVersion/systemManagement/softwareUpdate",
        NULL, 0, &software_update_callback, NULL);

    ulfius_add_endpoint_by_val(&rest_instance, "GET",
        "/api/:apiVersion/systemManagement/softwareUpdate",
        NULL, 0, &software_update_status_callback, NULL);

    // 恢复出厂配置请求
    ulfius_add_endpoint_by_val(&rest_instance, "POST",
        "/api/:apiVersion/systemManagement/reinitiate",
        NULL, 0, &reinitiate_callback, NULL);

    // 恢复出厂配置状态查询
    ulfius_add_endpoint_by_val(&rest_instance, "GET",
        "/api/:apiVersion/systemManagement/reinitiate",
        NULL, 0, &reinitiate_status_callback, NULL);

    // 软件回退请求
    ulfius_add_endpoint_by_val(&rest_instance, "POST",
        "/api/:apiVersion/systemManagement/softwareRollback",
        NULL, 0, &software_rollback_callback, NULL);

    // 软件回退状态查询
    ulfius_add_endpoint_by_val(&rest_instance, "GET",
        "/api/:apiVersion/systemManagement/softwareRollback",
        NULL, 0, &software_rollback_status_callback, NULL);

    
    // 注册OPTIONS请求处理程序，用于处理CORS预检请求
    ulfius_add_endpoint_by_val(&rest_instance, "OPTIONS", "/*", NULL, 0, &handle_options_request, NULL);
    // 检查证书文件是否存在
    printf("[DEBUG] 检查证书文件: %s\n", KEY_PEM);
    FILE *key_file = fopen(KEY_PEM, "r");
    if (!key_file) {
        fprintf(stderr, "[ERROR] 无法打开私钥文件 %s: %s\n", KEY_PEM, strerror(errno));
        return NULL;
    }
    fclose(key_file);
    
    printf("[DEBUG] 检查证书文件: %s\n", CERT_PEM);
    FILE *cert_file = fopen(CERT_PEM, "r");
    if (!cert_file) {
        fprintf(stderr, "[ERROR] 无法打开证书文件 %s: %s\n", CERT_PEM, strerror(errno));
        return NULL;
    }
    fclose(cert_file);
    
    printf("[DEBUG] 证书文件检查通过，尝试启动HTTPS服务器...\n");
    // 启动服务器
    // if (ulfius_start_secure_framework(&rest_instance, KEY_PEM, CERT_PEM) != U_OK) {
    //     fprintf(stderr, "[ERROR] Failed to start HTTPS server\n");
    //     return NULL;
    // }
    
    // printf("[DEBUG] HTTPS服务器启动成功，端口: %d\n", PORT);
    // printf("[DEBUG] 服务器监听地址: 0.0.0.0:%d\n", PORT);
    
        if (ulfius_start_framework(&rest_instance) != U_OK) {
        fprintf(stderr, "[ERROR] Failed to start HTTPS server\n");
        return NULL;
    }
    printf("[DEBUG] HTTPS服务器启动成功");
    // 主服务循环
     // 主循环（等待关闭信号）
    while (!atomic_load(&global_state.shutdown_requested)) {
        sleep(1);
    }


    // 清理
    ulfius_stop_framework(&rest_instance);
    ulfius_clean_instance(&rest_instance);

    return NULL;
}


// ================== FTP下载工作线程 ==================
// FTP进度回调函数前向声明
int FTP_dl_progress_callback(void* clientp, double dltotal, double dlnow, double ultotal, double ulnow);

int add_dl_task(DownloadTask task) {
    pthread_mutex_lock(&global_state.dl_task_mutex);
    if (global_state.dl_task_count >= MAX_DL_TASK_QUEUE) {
        pthread_mutex_unlock(&global_state.dl_task_mutex);
        return 0;
    }
    global_state.dl_task_queue[global_state.dl_task_count++] = task;
    pthread_cond_signal(&global_state.dl_task_cond);
    pthread_mutex_unlock(&global_state.dl_task_mutex);
    
    // 同时将任务添加到任务跟踪表中
    pthread_mutex_lock(&global_state.tracked_task_mutex);
    if (global_state.tracked_task_count < MAX_TRACKED_TASKS) {
        global_state.tracked_tasks[global_state.tracked_task_count++] = task;
    }
    pthread_mutex_unlock(&global_state.tracked_task_mutex);
    
    return 1;
}

// FTP文件下载线程
// 在跟踪表中查找并更新指定requestId的任务
void update_tracked_task_status(const char* requestId, const char* status, const char* error_info) {
    pthread_mutex_lock(&global_state.tracked_task_mutex);
    for (int i = 0; i < global_state.tracked_task_count; i++) {
        if (strcmp(global_state.tracked_tasks[i].requestId, requestId) == 0) {
            strncpy(global_state.tracked_tasks[i].status, status, sizeof(global_state.tracked_tasks[i].status));
            if (error_info) {
                strncpy(global_state.tracked_tasks[i].error_info, error_info, sizeof(global_state.tracked_tasks[i].error_info));
            }
            break;
        }
    }
    pthread_mutex_unlock(&global_state.tracked_task_mutex);
}

void* ftp_dl_worker(void* arg) {
    GlobalState* state = (GlobalState*)arg;
    int i = 0;
    // 从环境变量获取FTP凭证
    const char* ftp_user = getenv("FTP_USER");
    const char* ftp_pass = getenv("FTP_PASS");

    if (!ftp_user || !ftp_pass) {
        fprintf(stderr, "FTP凭证未设置\n");
        return NULL;
    }

    while (!atomic_load(&global_state.shutdown_requested)) {
        pthread_mutex_lock(&global_state.dl_task_mutex);

        // 等待新任务
        while (global_state.dl_task_count == 0 && !atomic_load(&global_state.shutdown_requested)) {
            pthread_cond_wait(&global_state.dl_task_cond, &global_state.dl_task_mutex);
        }
        if (atomic_load(&global_state.shutdown_requested)) {
            pthread_mutex_unlock(&global_state.dl_task_mutex);
            break;
        }

        // 获取任务
        DownloadTask task = global_state.dl_task_queue[0];

        // 移动队列
        for (i = 0; i < global_state.dl_task_count - 1; i++) {
            global_state.dl_task_queue[i] = global_state.dl_task_queue[i + 1];
        }
        global_state.dl_task_count--;

        pthread_mutex_unlock(&global_state.dl_task_mutex);
        
        // 增加活动下载计数
        pthread_mutex_lock(&global_state.dl_task_mutex);
        global_state.active_downloads++;
        pthread_mutex_unlock(&global_state.dl_task_mutex);

        // 5. 创建目录
        char dir_path[256];
        strncpy(dir_path, task.save_path, sizeof(dir_path) - 1);
        dir_path[sizeof(dir_path) - 1] = '\0';
        char* last_slash = strrchr(dir_path, '/');
        if (last_slash) {
            *last_slash = '\0';
            struct stat st = { 0 };
            if (stat(dir_path, &st) == -1) {
                if (mkdir(dir_path, 0700) == -1) {
                    fprintf(stderr, "无法创建目录: %s\n", dir_path);

                    // 更新任务跟踪表中的状态为失败
                    char error_msg[256];
                    snprintf(error_msg, sizeof(error_msg), "创建目录失败: %s", dir_path);
                    update_tracked_task_status(task.requestId, "fail", error_msg);

                    // 减少活动下载计数
                    pthread_mutex_lock(&global_state.dl_task_mutex);
                    if (global_state.active_downloads > 0) {
                        global_state.active_downloads--;
                    }
                    pthread_mutex_unlock(&global_state.dl_task_mutex);
                    
                    continue;
                }
            }
        }

        // 6. 执行下载
        CURL* curl = curl_easy_init();
        FILE* fp = fopen(task.save_path, "wb");

        if (curl && fp) {
            // 更新任务跟踪表中的状态为"updating"
            update_tracked_task_status(task.requestId, "updating", NULL);
            // 基础配置
            curl_easy_setopt(curl, CURLOPT_URL, task.download_url);
            curl_easy_setopt(curl, CURLOPT_USERNAME, ftp_user);  // 实际应用中应从配置获取
            curl_easy_setopt(curl, CURLOPT_PASSWORD, ftp_pass);  // 实际应用中应从配置获取
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);

            // 进度监控关键配置
            curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);  // 启用进度回调
            curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, FTP_dl_progress_callback);
            curl_easy_setopt(curl, CURLOPT_PROGRESSDATA, &task);  // 传递任务对象指针
           // curl_easy_setopt(curl, CURLOPT_FTP_USE_EPSV, 0);
            // 执行下载
            CURLcode res = curl_easy_perform(curl);
            fclose(fp);

            if (res != CURLE_OK) {
                // 准备错误信息
                char error_msg[256];
                snprintf(error_msg, sizeof(error_msg), "FTP下载失败: %s", curl_easy_strerror(res));
                
                fprintf(stderr, "FTP下载失败: %s -> %s\n",
                    task.download_url, curl_easy_strerror(res));
                remove(task.save_path);
                
                // 更新任务跟踪表中的状态为失败
                update_tracked_task_status(task.requestId, "fail", error_msg);
            } else {
                strncpy(task.status, "success", sizeof(task.status));
                printf("FTP下载成功: %s -> %s\n",
                    task.download_url, task.save_path);
                
                // 更新任务跟踪表中的状态为成功
                update_tracked_task_status(task.requestId, "success", NULL);
            }

            curl_easy_cleanup(curl);
        } else {
            // 错误处理
            // 准备错误信息
            char error_msg[256];
            snprintf(error_msg, sizeof(error_msg), "FTP初始化失败");

            // 减少活动下载计数
            pthread_mutex_lock(&global_state.dl_task_mutex);
            if (global_state.active_downloads > 0) {
                global_state.active_downloads--;
            }
            pthread_mutex_unlock(&global_state.dl_task_mutex);
            
            // 更新任务跟踪表中的状态为失败
            update_tracked_task_status(task.requestId, "fail", error_msg);
            
            fprintf(stderr, "FTP任务初始化失败: %s\n", task.download_url);
            if (curl) {
                curl_easy_cleanup(curl);
            }
            continue;
        }
        
        // 7. 减少活动下载计数
        pthread_mutex_lock(&global_state.dl_task_mutex);
        if (global_state.active_downloads > 0) {
            global_state.active_downloads--;
        }
        pthread_mutex_unlock(&global_state.dl_task_mutex);
    }
    return NULL;
}


// FTP进度回调函数

int FTP_dl_progress_callback(void* clientp, double dltotal, double dlnow, double ultotal, double ulnow) {
    // clientp 可传递自定义数据（如任务ID）
    DownloadTask* task = (DownloadTask*)clientp;

    // 避免除零错误（文件大小未知时）
    if (dltotal <= 0) return 0;

    // 计算下载进度百分比
    int percent = (int)((dlnow / dltotal) * 100);

    // 更新任务状态和跟踪表
    if (task) {
        char status_str[32];
        snprintf(status_str, sizeof(status_str), "downloading (%d%%)", percent);
        
        // 更新本地任务对象状态
        strncpy(task->status, status_str, sizeof(task->status));
        
        // 同时更新任务跟踪表中的状态
        update_tracked_task_status(task->requestId, status_str, NULL);
    }

    // 实时输出进度（可选）
    printf("\r[%s] 进度: %d%% (%.0f/%.0f bytes)",
        task->requestId, percent, dlnow, dltotal);
    fflush(stdout);

    return 0;  // 必须返回0
}
///////////////////////////////////////////////////////////
// ================== Token管理 ==================
// 添加Token到黑名单
void blacklist_token(const char* token, time_t expire_time) {
    int i = 0;
    pthread_mutex_lock(&global_state.blacklist_mutex);
    for (i = 0; i < MAX_BLACKLIST_TOKENS; i++) {
        if (global_state.token_blacklist[i] == NULL) {
            global_state.token_blacklist[i] = malloc(sizeof(BlacklistEntry));
            global_state.token_blacklist[i]->token = strdup(token);
            global_state.token_blacklist[i]->expire_time = expire_time;
            break;
        }
    }
    pthread_mutex_unlock(&global_state.blacklist_mutex);
}

// 检查Token是否在黑名单
int is_token_blacklisted(const char* token) {
    int i = 0;
    pthread_mutex_lock(&global_state.blacklist_mutex);
    for (i = 0; i < MAX_BLACKLIST_TOKENS; i++) {
        if (global_state.token_blacklist[i] != NULL &&
            strcmp(global_state.token_blacklist[i]->token, token) == 0) {
            pthread_mutex_unlock(&global_state.blacklist_mutex);
            return 1;
        }
    }
    pthread_mutex_unlock(&global_state.blacklist_mutex);
    return 0;
}

// 清理过期Token
void cleanup_expired_tokens(void* arg) {
    pthread_mutex_lock(&global_state.blacklist_mutex);
    for (int i = 0; i < MAX_BLACKLIST_TOKENS; i++) {
        if (global_state.token_blacklist[i] != NULL &&
            time(NULL) > global_state.token_blacklist[i]->expire_time) {
            free(global_state.token_blacklist[i]->token);
            free(global_state.token_blacklist[i]);
            global_state.token_blacklist[i] = NULL;
        }
    }
    pthread_mutex_unlock(&global_state.blacklist_mutex);
}

// ================== JWT功能 ==================
// 生成JWT Token
char* generate_jwt(const char* username) {
    const char* jwt_secret = getenv("JWT_SECRET");
    if (!jwt_secret) {
        fprintf(stderr, "JWT_SECRET 环境变量未设置\n");
        return NULL;
    }

    jwt_t* token = NULL;
    time_t exp_time = time(NULL) + TOKEN_EXPIRES;

    jwt_new(&token);
    jwt_set_alg(token, JWT_ALG_HS256,
        (const unsigned char*)jwt_secret,
        strlen(jwt_secret));
    jwt_add_grant(token, "sub", username);
    jwt_add_grant_int(token, "exp", exp_time);

    char* jwt_str = jwt_encode_str(token);
    jwt_free(token);
    return jwt_str;
}

int verify_jwt(const char* access_token) {
    const char* jwt_secret = getenv("JWT_SECRET");
    if (!jwt_secret) {
        fprintf(stderr, "JWT_SECRET 环境变量未设置\n");
        return -1;
    }

    jwt_t* jwt_ptr = NULL;
    if (jwt_decode(&jwt_ptr, access_token,
        (const unsigned char*)jwt_secret,
        strlen(jwt_secret))) {
        return -2;  //  无效签名
    }

    time_t exp_time = jwt_get_grant_int(jwt_ptr, "exp");
    if (exp_time < 0) {
        jwt_free(jwt_ptr);
        return -3;  //   令牌过期
    }

    if (is_token_blacklisted(access_token)) {
        jwt_free(jwt_ptr);
        return -4;  // 令牌被撤销
    }

    jwt_free(jwt_ptr);
    return 0;  //  验证成功
}
// ================== RESTful接口回调 ==================
// 登录接口回调
int login_callback(const struct _u_request* request,
    struct _u_response* response,
    void* user_data) {
    int i = 0;
    // 解析JSON请求
    json_t* root = ulfius_get_json_body_request(request, NULL);
    if (!root) {
        // 添加CORS头信息
        add_cors_headers(response);
        ulfius_set_string_body_response(response, 400, "Invalid JSON");
        return U_CALLBACK_CONTINUE;
    }

    // 验证请求参数
    json_t* grantType = json_object_get(root, "grantType");
    json_t* userName = json_object_get(root, "username");
    json_t* value = json_object_get(root, "value");

    if (!json_is_string(grantType) ||
        strcmp(json_string_value(grantType), "password") != 0 ||
        !json_is_string(userName) ||
        !json_is_string(value)) {
        json_decref(root);
        // 添加CORS头信息
        add_cors_headers(response);
        ulfius_set_string_body_response(response, 400,
            "Missing or invalid parameters");
        return U_CALLBACK_CONTINUE;
    }

    // 用户认证
    const char* username = json_string_value(userName);
    const char* password = json_string_value(value);
    int valid = 0;

    for (i = 0; i < sizeof(valid_users) / sizeof(User); i++) {
        if (strcmp(valid_users[i].username, username) == 0 &&
            strcmp(valid_users[i].password, password) == 0) {
            valid = 1;
            break;
        }
    }

    if (!valid) {
        json_decref(root);
        // 添加CORS头信息
        add_cors_headers(response);
        ulfius_set_string_body_response(response, 401, "Invalid credentials");
        return U_CALLBACK_CONTINUE;
    }

    // 生成JWT响应
    char* jwt = generate_jwt(username);
    if (!jwt) {
        json_decref(root);
        // 添加CORS头信息
        add_cors_headers(response);
        ulfius_set_string_body_response(response, 500, "Internal server error");
        return U_CALLBACK_CONTINUE;
    }
    json_t* resp = json_object();
    json_object_set_new(resp, "accessToken", json_string(jwt));
    json_object_set_new(resp, "expires", json_integer(TOKEN_EXPIRES));

    // 添加安全头
    ulfius_add_header_to_response(response, "Strict-Transport-Security",
        "max-age=31536000");
    
    // 添加CORS头信息
    add_cors_headers(response);

    ulfius_set_json_body_response(response, 200, resp);

    // 清理资源
    free(jwt);
    json_decref(root);
    json_decref(resp);
    return U_CALLBACK_CONTINUE;
}

// 握手接口回调
int handle_handshake_request(const struct _u_request* request,
    struct _u_response* response,
    void* user_data) {
    // 获取accessToken头
    const char* access_token = u_map_get(request->map_header, "accessToken");
    if (!access_token) {
        ulfius_add_header_to_response(response, "Content-Type", "application/JSON;charset=UTF-8");
        ulfius_set_string_body_response(response, 401,
            "{\"error\":\"Missing accessToken header\"}");
        return U_CALLBACK_CONTINUE;
    }

    int result = verify_jwt(access_token);
    switch (result) {
    case -1:
        ulfius_add_header_to_response(response, "Content-Type", "application/json");
        ulfius_set_string_body_response(response, 500, "Internal server error");
        break;
    case -2:
        ulfius_add_header_to_response(response, "Content-Type", "application/json");
        ulfius_set_string_body_response(response, 401,
            "{\"error\":\"Invalid token signature\"}");
        break;
    case -3:
        ulfius_add_header_to_response(response, "Content-Type", "application/json");
        ulfius_set_string_body_response(response, 401,
            "{\"error\":\"Token expired\"}");
        break;
    case -4:
        ulfius_add_header_to_response(response, "Content-Type", "application/json");
        ulfius_set_string_body_response(response, 401,
            "{\"error\":\"Token revoked\"}");
        break;
    case 0:
        ulfius_set_empty_body_response(response, 200);
        break;
    }
    return U_CALLBACK_CONTINUE;
}

// 退出接口回调
int logout_callback(const struct _u_request* request,
    struct _u_response* response,
    void* user_data) {
    const char* access_token = u_map_get(request->map_header, "accessToken");
    if (!access_token) {
        // 添加CORS头信息
        add_cors_headers(response);
        ulfius_set_empty_body_response(response, 400);
        return U_CALLBACK_CONTINUE;
    }

    // 验证Token有效性
    jwt_t* jwt_ptr = NULL;
    const char* jwt_secret = getenv("JWT_SECRET");
    if (!jwt_secret) {
        // 添加CORS头信息
        add_cors_headers(response);
        ulfius_add_header_to_response(response, "Content-Type", "application/json");
        ulfius_set_string_body_response(response, 500, "Internal server error");
        return U_CALLBACK_CONTINUE;
    }
    if (jwt_decode(&jwt_ptr, access_token,
        (const unsigned char*)jwt_secret,
        strlen(jwt_secret))) {
        // 添加CORS头信息
        add_cors_headers(response);
        ulfius_set_empty_body_response(response, 401);
        return U_CALLBACK_CONTINUE;
    }

    // 获取过期时间
    time_t exp_time = jwt_get_grant_int(jwt_ptr, "exp");
    if (exp_time < 0) {
        jwt_free(jwt_ptr);
        // 添加CORS头信息
        add_cors_headers(response);
        ulfius_set_empty_body_response(response, 401);
        return U_CALLBACK_CONTINUE;
    }

    // 添加到黑名单
    blacklist_token(access_token, exp_time);
    jwt_free(jwt_ptr);

    // 添加CORS头信息
    add_cors_headers(response);
    ulfius_set_empty_body_response(response, 204);
    return U_CALLBACK_CONTINUE;
}

// 软件版本查询回调 ////////////1111111111 
//接收到地面信息后 发送 MSG_QUERY_SOFTWARE_VERSION 类别的 message 给中控
//中控反馈
int software_version_callback(const struct _u_request* request, 
    struct _u_response* response, 
    void* user_data) {
    // 1. 验证accessToken
    const char* access_token = u_map_get(request->map_header, "accessToken");
    if (!access_token) {
        // 错误响应应包含requestId
        json_t* resp = json_object();
        json_object_set_new(resp, "requestId", json_string(""));
        json_object_set_new(resp, "errorInfo", json_string("Missing accessToken header"));
        // 添加CORS头信息
        add_cors_headers(response);
        ulfius_set_json_body_response(response, 401, resp);
        json_decref(resp);
        return U_CALLBACK_CONTINUE;
    }
    
    int result = verify_jwt(access_token);
     if (result != 0) {
        // 错误响应应包含requestId
        json_t* resp = json_object();
        json_object_set_new(resp, "requestId", json_string(""));
        
        switch (result) {
        case -1:
            json_object_set_new(resp, "errorInfo", json_string("Internal server error"));
            // 添加CORS头信息
            add_cors_headers(response);
            ulfius_set_json_body_response(response, 500, resp);
            break;
        case -2:
            json_object_set_new(resp, "errorInfo", json_string("Invalid token signature"));
            // 添加CORS头信息
            add_cors_headers(response);
            ulfius_set_json_body_response(response, 401, resp);
            break;
        case -3:
            json_object_set_new(resp, "errorInfo", json_string("Token expired"));
            // 添加CORS头信息
            add_cors_headers(response);
            ulfius_set_json_body_response(response, 401, resp);
            break;
        case -4:
            json_object_set_new(resp, "errorInfo", json_string("Token revoked"));
            // 添加CORS头信息
            add_cors_headers(response);
            ulfius_set_json_body_response(response, 401, resp);
            break;
        }
        json_decref(resp);
        return U_CALLBACK_CONTINUE;
    }
    
    // 2. 解析请求体
    json_t* root = ulfius_get_json_body_request(request, NULL);
    if (!root) {
        json_t* resp = json_object();
        json_object_set_new(resp, "requestId", json_string(""));
        json_object_set_new(resp, "errorInfo", json_string("Invalid JSON format"));
        // 添加CORS头信息
        add_cors_headers(response);
        ulfius_set_json_body_response(response, 400, resp);
        json_decref(resp);
        return U_CALLBACK_CONTINUE;
    }


    // 3. 提取关键参数
    json_t* json_requestId = json_object_get(root, "requestId");
    json_t* json_ruid = json_object_get(root, "ruid");
   

    //const char* requestId = u_map_get(request->map_url, "requestId");
    //const char* ruid = u_map_get(request->map_url, "ruid");
    
    // 验证参数类型
    if (!json_is_integer(json_requestId)) {
        json_decref(root);
        json_t* resp = json_object();
        json_object_set_new(resp, "requestId", json_string(""));
        json_object_set_new(resp, "errorInfo", json_string("requestId must be an integer"));
        // 添加CORS头信息
        add_cors_headers(response);
        ulfius_set_json_body_response(response, 400, resp);
        json_decref(resp);
        return U_CALLBACK_CONTINUE;
    }
    
    if (!json_is_string(json_ruid)) {
        json_decref(root);
        json_t* resp = json_object();
        char requestId_str[64];
        snprintf(requestId_str, sizeof(requestId_str), "%lld", json_integer_value(json_requestId));
        json_object_set_new(resp, "requestId", json_string(requestId_str));
        json_object_set_new(resp, "errorInfo", json_string("ruid must be a string"));
        // 添加CORS头信息
        add_cors_headers(response);
        ulfius_set_json_body_response(response, 400, resp);
        json_decref(resp);
        return U_CALLBACK_CONTINUE;
    }

    
    long long requestId_val = json_integer_value(json_requestId);
    char requestId_str[64];
    snprintf(requestId_str, sizeof(requestId_str), "%lld", requestId_val);
    const char* requestId = requestId_str;
    const char* ruid = json_string_value(json_ruid);            // 


    json_decref(root);

    // 4. 初始化请求上下文（栈分配）
    eth0_Query_SOFTWARE_VERSION_Context ctx;
    memset(&ctx, 0, sizeof(ctx));  // 清零所有字段
    //pthread_mutex_t* mutex = malloc(sizeof(pthread_mutex_t));
    //pthread_cond_t* cond = malloc(sizeof(pthread_cond_t));
    ///pthread_mutex_init(mutex, NULL);
    //pthread_cond_init(cond, NULL);

    strncpy(ctx.requestId, requestId, sizeof(ctx.requestId) - 1);
    strncpy(ctx.ruid, ruid, sizeof(ctx.ruid) - 1);
    //ctx.response_mutex = mutex;
    //ctx.response_cond = cond;
    ctx.response_ready = false;
    

    // 5. 创建并发送消息给中控
    eth0_data_QUERY_SOFTWARE_VERSION data_out;
    strncpy(data_out.requestId, requestId, sizeof(data_out.requestId) - 1);
    strncpy(data_out.ruid, ruid, sizeof(data_out.ruid) - 1);
    Message msg;
    msg.type = MSG_QUERY_SOFTWARE_VERSION;
    msg.data = data_out; //query_request;
    msg.data_size = sizeof(data_out);
    msg.source = ETH0_SOURCE;

    //memcpy(msg.data, &task, sizeof(UpdateTask));

    if (!enqueue_message(&global_state, &msg)) {
        // 消息队列已满
        pthread_mutex_destroy(ctx.response_mutex);
        pthread_cond_destroy(ctx.response_cond);
        json_t* resp = json_object();
        char requestId_str[64];
        snprintf(requestId_str, sizeof(requestId_str), "%lld", json_integer_value(json_requestId));
        json_object_set_new(resp, "requestId", json_string(requestId_str));
        json_object_set_new(resp, "errorInfo", json_string("Service unavailable"));
        // 添加CORS头信息
        add_cors_headers(response);
        ulfius_add_header_to_response(response, "Content-Type", "application/JSON;charset=UTF-8");
        ulfius_set_string_body_response(response, 503,resp);
        json_decref(resp);
        return U_CALLBACK_CONTINUE;
    }
    
     // 6. 阻塞等待响应（带超时）
    //pthread_mutex_lock(ctx.response_mutex);
    
    struct timespec timeout;
    clock_gettime(CLOCK_REALTIME, &timeout);
    timeout.tv_sec += 5; // 5秒超时
    
    while (!ctx.response_ready) {
        int wait_result = pthread_cond_timedwait(ctx.response_cond, ctx.response_mutex, &timeout);
        if (wait_result == ETIMEDOUT) {
            break;  // 跳转到超时处理
        }    
    }

    if  (!ctx.response_ready) {
       

        json_object_set_new(resp, "requestId", json_string(requestId));
        json_object_set_new(resp, "errorInfo", json_string("Request timeout"));
        // 添加CORS头信息
        add_cors_headers(response);
        ulfius_add_header_to_response(response, "Content-Type", "application/JSON;charset=UTF-8");
        ulfius_set_string_body_response(response, 504,resp);
        json_decref(resp);
        
        return U_CALLBACK_CONTINUE;
    }


    
    // 7. 构造响应JSON
    json_t* json_response = json_object();
    json_object_set_new(json_response, "requestId", json_string(ctx.requestId));

    if (ctx.errorInfo[0] != '\0') {  // 失败响应
        json_object_set_new(json_response, "errorInfo", json_string(ctx.errorInfo));
        add_cors_headers(response);
        ulfius_set_json_body_response(response, 500, json_response);
    } else {// 成功响应
        json_t* version_list = json_array();
        for (int i = 0; i < 10 && ctx.backupVersionList[i][0] != '\0'; i++) {
            json_array_append_new(version_list, json_string(ctx.backupVersionList[i]));
        }
        json_object_set_new(json_response, "currentVersion", json_string(ctx.currentVersion));
        json_object_set_new(json_response, "backupVersionList", version_list);
        add_cors_headers(response);
        ulfius_set_json_body_response(response, 200, json_response);

    } 
    
    json_decref(json_response);
    // 8. 清理资源

    
    return U_CALLBACK_CONTINUE;
}

// ================== 软件下载接口 ==================
int software_download_callback(const struct _u_request* request,
    struct _u_response* response,
    void* user_data) {
    int i = 0;
    // 1. 验证accessToken
    const char* access_token = u_map_get(request->map_header, "accessToken");
    if (!access_token) {
        // 错误响应应包含requestId
        json_t* resp = json_object();
        json_object_set_new(resp, "requestId", json_string(""));
        json_object_set_new(resp, "errorInfo", json_string("Missing accessToken header"));
        // 添加CORS头信息
        add_cors_headers(response);
        ulfius_set_json_body_response(response, 401, resp);
        json_decref(resp);
        return U_CALLBACK_CONTINUE;
    }

    int result = verify_jwt(access_token);
    if (result != 0) {
        // 错误响应应包含requestId
        json_t* resp = json_object();
        json_object_set_new(resp, "requestId", json_string(""));
        
        switch (result) {
        case -1:
            json_object_set_new(resp, "errorInfo", json_string("Internal server error"));
            // 添加CORS头信息
            add_cors_headers(response);
            ulfius_set_json_body_response(response, 500, resp);
            break;
        case -2:
            json_object_set_new(resp, "errorInfo", json_string("Invalid token signature"));
            // 添加CORS头信息
            add_cors_headers(response);
            ulfius_set_json_body_response(response, 401, resp);
            break;
        case -3:
            json_object_set_new(resp, "errorInfo", json_string("Token expired"));
            // 添加CORS头信息
            add_cors_headers(response);
            ulfius_set_json_body_response(response, 401, resp);
            break;
        case -4:
            json_object_set_new(resp, "errorInfo", json_string("Token revoked"));
            // 添加CORS头信息
            add_cors_headers(response);
            ulfius_set_json_body_response(response, 401, resp);
            break;
        }
        json_decref(resp);
        return U_CALLBACK_CONTINUE;
    }

    // 2. 解析请求体
    json_t* root = ulfius_get_json_body_request(request, NULL);
    if (!root) {
        json_t* resp = json_object();
        json_object_set_new(resp, "requestId", json_string(""));
        json_object_set_new(resp, "errorInfo", json_string("Invalid JSON format"));
        // 添加CORS头信息
        add_cors_headers(response);
        ulfius_set_json_body_response(response, 400, resp);
        json_decref(resp);
        return U_CALLBACK_CONTINUE;
    }

    // 3. 提取关键参数
    json_t* json_requestId = json_object_get(root, "requestId");
    json_t* json_version = json_object_get(root, "softwareVersion");
    json_t* json_dl_path = json_object_get(root, "downloadPath");

    // 验证参数类型
    if (!json_is_integer(json_requestId)) {
        json_decref(root);
        json_t* resp = json_object();
        json_object_set_new(resp, "requestId", json_string(""));
        json_object_set_new(resp, "errorInfo", json_string("requestId must be an integer"));
        // 添加CORS头信息
        add_cors_headers(response);
        ulfius_set_json_body_response(response, 400, resp);
        json_decref(resp);
        return U_CALLBACK_CONTINUE;
    }
    
    if (!json_is_string(json_version)) {
        json_decref(root);
        json_t* resp = json_object();
        char requestId_str[32];
        snprintf(requestId_str, sizeof(requestId_str), "%lld", json_integer_value(json_requestId));
        json_object_set_new(resp, "requestId", json_string(requestId_str));
        json_object_set_new(resp, "errorInfo", json_string("softwareVersion must be a string"));
        // 添加CORS头信息
        add_cors_headers(response);
        ulfius_set_json_body_response(response, 400, resp);
        json_decref(resp);
        return U_CALLBACK_CONTINUE;
    }
    
    if (!json_is_string(json_dl_path)) {
        json_decref(root);
        json_t* resp = json_object();
        char requestId_str[32];
        snprintf(requestId_str, sizeof(requestId_str), "%lld", json_integer_value(json_requestId));
        json_object_set_new(resp, "requestId", json_string(requestId_str));
        json_object_set_new(resp, "errorInfo", json_string("downloadPath must be a string"));
        // 添加CORS头信息
        add_cors_headers(response);
        ulfius_set_json_body_response(response, 400, resp);
        json_decref(resp);
        return U_CALLBACK_CONTINUE;
    }

    // 将requestId转换为字符串用于保存
    char requestId_str[32];
    snprintf(requestId_str, sizeof(requestId_str), "%lld", json_integer_value(json_requestId));
    const char* version = json_string_value(json_version);
    const char* download_url = json_string_value(json_dl_path);

    // 构建保存路径 - 根据下载路径和版本信息
    char save_path[256];
    // 从downloadPath提取文件名
    const char* filename = strrchr(download_url, '/');
    if (filename) {
        filename++;
        snprintf(save_path, sizeof(save_path), "/var/sw_updates/%s", filename);
    } else {
        snprintf(save_path, sizeof(save_path), "/var/sw_updates/v%s.zip", version);
    }

    // 5. 创建下载任务
    DownloadTask new_task;

    strncpy(new_task.download_url, download_url, sizeof(new_task.download_url) - 1);
    new_task.download_url[sizeof(new_task.download_url) - 1] = '\0';
    strncpy(new_task.save_path, save_path, sizeof(new_task.save_path) - 1);
    new_task.save_path[sizeof(new_task.save_path) - 1] = '\0';

    // 保存requestId并初始化状态
    strncpy(new_task.requestId, requestId_str, sizeof(new_task.requestId) - 1);
    new_task.requestId[sizeof(new_task.requestId) - 1] = '\0';
    strncpy(new_task.status, "pending", sizeof(new_task.status));  // 初始状态
    new_task.error_info[0] = '\0';  // 清空错误信息


    // 6. 添加任务到队列 
    json_t* resp = json_object();
    json_object_set_new(resp, "requestId", json_string(requestId_str));


    if (add_dl_task(new_task)) {
        // 添加CORS头信息
        add_cors_headers(response);
        ulfius_set_json_body_response(response, 200, resp);
    } else {
        json_object_set_new(resp, "errorInfo", json_string("队列已满"));
        // 添加CORS头信息
        add_cors_headers(response);
        ulfius_add_header_to_response(response, "Content-Type", "application/JSON;charset=UTF-8");
        ulfius_set_json_body_response(response, 503, resp);
    }

    // 7. 清理资源
    json_decref(resp);
    json_decref(root);

    return U_CALLBACK_CONTINUE;
}


// ================== 软件下载状态查询接口 ==================
int software_download_status_callback(const struct _u_request* request,
    struct _u_response* response,
    void* user_data) {
    int i = 0;
    const char* requestId_str = NULL;
    
    // 1. 验证accessToken
    const char* access_token = u_map_get(request->map_header, "accessToken");
    if (!access_token) {
        // 所有错误响应都应包含requestId
        requestId_str = u_map_get(request->map_url, "requestId");
        json_t* resp = json_object();
        if (requestId_str) {
            json_object_set_new(resp, "requestId", json_string(requestId_str));
        } else {
            json_object_set_new(resp, "requestId", json_string(""));
        }
        json_object_set_new(resp, "errorInfo", json_string("Missing accessToken header"));
        // 添加CORS头信息
        add_cors_headers(response);
        ulfius_add_header_to_response(response, "Content-Type", "application/JSON;charset=UTF-8");
        ulfius_set_json_body_response(response, 401, resp);
        json_decref(resp);
        return U_CALLBACK_CONTINUE;
    }

    int result = verify_jwt(access_token);
    if (result != 0) {
        // 所有错误响应都应包含requestId
        requestId_str = u_map_get(request->map_url, "requestId");
        json_t* resp = json_object();
        if (requestId_str) {
            json_object_set_new(resp, "requestId", json_string(requestId_str));
        } else {
            json_object_set_new(resp, "requestId", json_string(""));
        }
        // 添加CORS头信息
        add_cors_headers(response);
        ulfius_add_header_to_response(response, "Content-Type", "application/JSON;charset=UTF-8");
        
        switch (result) {
        case -1:
            json_object_set_new(resp, "errorInfo", json_string("Internal server error"));
            ulfius_set_json_body_response(response, 500, resp);
            break;
        case -2:
            json_object_set_new(resp, "errorInfo", json_string("Invalid token signature"));
            ulfius_set_json_body_response(response, 401, resp);
            break;
        case -3:
            json_object_set_new(resp, "errorInfo", json_string("Token expired"));
            ulfius_set_json_body_response(response, 401, resp);
            break;
        case -4:
            json_object_set_new(resp, "errorInfo", json_string("Token revoked"));
            ulfius_set_json_body_response(response, 401, resp);
            break;
        }
        json_decref(resp);
        return U_CALLBACK_CONTINUE;
    }

    // 2. 获取请求参数
    requestId_str = u_map_get(request->map_url, "requestId");
    if (!requestId_str) {
        json_t* resp = json_object();
        json_object_set_new(resp, "requestId", json_string(""));
        json_object_set_new(resp, "errorInfo", json_string("Missing requestId parameter"));
        // 添加CORS头信息
        add_cors_headers(response);
        ulfius_add_header_to_response(response, "Content-Type", "application/JSON;charset=UTF-8");
        ulfius_set_json_body_response(response, 400, resp);
        json_decref(resp);
        return U_CALLBACK_CONTINUE;
    }
    
    // requestId必须是整数类型
    char* endptr;
    long requestId = strtol(requestId_str, &endptr, 10);
    if (*endptr != '\0') {
        json_t* resp = json_object();
        json_object_set_new(resp, "requestId", json_string(requestId_str));
        json_object_set_new(resp, "errorInfo", json_string("requestId must be an integer"));
        // 添加CORS头信息
        add_cors_headers(response);
        ulfius_add_header_to_response(response, "Content-Type", "application/JSON;charset=UTF-8");
        ulfius_set_json_body_response(response, 400, resp);
        json_decref(resp);
        return U_CALLBACK_CONTINUE;
    }

    // 3. 在当前任务队列中查找
    pthread_mutex_lock(&global_state.dl_task_mutex);
    for (i = 0; i < global_state.dl_task_count; i++) {
        if (strcmp(global_state.dl_task_queue[i].requestId, requestId_str) == 0) {
            json_t* resp = json_object();
            json_object_set_new(resp, "requestId", json_string(requestId_str));
            json_object_set_new(resp, "status", json_string(global_state.dl_task_queue[i].status));

            // 当状态为fail时，应返回additionalInfo字段
            if (strcmp(global_state.dl_task_queue[i].status, "fail") == 0) {
                json_object_set_new(resp, "additionalInfo", json_string(global_state.dl_task_queue[i].error_info));
            }

            // 添加CORS头信息
            add_cors_headers(response);
            ulfius_set_json_body_response(response, 200, resp);
            json_decref(resp);
            pthread_mutex_unlock(&global_state.dl_task_mutex);
            return U_CALLBACK_CONTINUE;
        }
    }
    pthread_mutex_unlock(&global_state.dl_task_mutex);


    // 5. 在任务跟踪表中查找
    pthread_mutex_lock(&global_state.tracked_task_mutex);
    for (i = 0; i < global_state.tracked_task_count; i++) {
        if (strcmp(global_state.tracked_tasks[i].requestId, requestId_str) == 0) {
            json_t* resp = json_object();
            json_object_set_new(resp, "requestId", json_string(requestId_str));
            json_object_set_new(resp, "status", json_string(global_state.tracked_tasks[i].status));

            // 当状态为fail时，应返回additionalInfo字段
            if (strcmp(global_state.tracked_tasks[i].status, "fail") == 0) {
                json_object_set_new(resp, "additionalInfo", json_string(global_state.tracked_tasks[i].error_info));
            }

            // 添加CORS头信息
            add_cors_headers(response);
            ulfius_set_json_body_response(response, 200, resp);
            json_decref(resp);
            pthread_mutex_unlock(&global_state.tracked_task_mutex);
            return U_CALLBACK_CONTINUE;
        }
    }
    pthread_mutex_unlock(&global_state.tracked_task_mutex);

    // 6. 未找到任务
    json_t* resp = json_object();
    json_object_set_new(resp, "requestId", json_string(requestId_str));
    json_object_set_new(resp, "errorInfo", json_string("Task not found"));
    // 添加CORS头信息
    add_cors_headers(response);
    ulfius_add_header_to_response(response, "Content-Type", "application/JSON;charset=UTF-8");
    ulfius_set_json_body_response(response, 404, resp);
    json_decref(resp);

    return U_CALLBACK_CONTINUE;
}


// ================== 软件更新请求接口 ==================
int software_update_callback(const struct _u_request* request,
    struct _u_response* response,
    void* user_data) {
    int i = 0;
    // char save_path[256];
    // 1. 验证accessToken
    const char* access_token = u_map_get(request->map_header, "accessToken");
    if (!access_token) {
        // 添加CORS头信息
        add_cors_headers(response);
        ulfius_add_header_to_response(response, "Content-Type", "application/JSON;charset=UTF-8");
        ulfius_set_string_body_response(response, 401,
            "{\"error\":\"Missing accessToken header\"}");
        return U_CALLBACK_CONTINUE;
    }
    int result = verify_jwt(access_token);
    if (result != 0) {
        // 添加CORS头信息
        add_cors_headers(response);
        ulfius_add_header_to_response(response, "Content-Type", "application/JSON;charset=UTF-8");
        switch (result) {
        case -1:
            ulfius_add_header_to_response(response, "Content-Type", "application/json");
            ulfius_set_string_body_response(response, 500, "Internal server error");
            break;
        case -2:
            ulfius_add_header_to_response(response, "Content-Type", "application/json");
            ulfius_set_string_body_response(response, 401,
                "{\"error\":\"Invalid token signature\"}");
            break;
        case -3:
            ulfius_add_header_to_response(response, "Content-Type", "application/json");
            ulfius_set_string_body_response(response, 401,
                "{\"error\":\"Token expired\"}");
            break;
        case -4:
            ulfius_add_header_to_response(response, "Content-Type", "application/json");
            ulfius_set_string_body_response(response, 401,
                "{\"error\":\"Token revoked\"}");
            break;
        }
        return U_CALLBACK_CONTINUE;
    }

    // 2. 解析请求体
    json_t* root = ulfius_get_json_body_request(request, NULL);
    if (!root) {
        // 添加CORS头信息
        add_cors_headers(response);
        ulfius_add_header_to_response(response, "Content-Type", "application/JSON;charset=UTF-8");
        ulfius_set_string_body_response(response, 400, "Invalid JSON");
        return U_CALLBACK_CONTINUE;
    }

    // 3. 获取参数
    json_t* json_requestId = json_object_get(root, "requestId");
    json_t* json_ruid = json_object_get(root, "ruid");
    json_t* json_updateVersion = json_object_get(root, "updateVersion");

    if (!json_is_integer(json_requestId) ||
        !json_is_string(json_updateVersion)) {
        json_decref(root);
        // 添加CORS头信息
        add_cors_headers(response);
        ulfius_add_header_to_response(response, "Content-Type", "application/JSON;charset=UTF-8");
        ulfius_set_string_body_response(response, 400,
            "Missing or invalid parameters");
        return U_CALLBACK_CONTINUE;
    }
    // 4. 创建更新任务
    UpdateTask task;
    pthread_mutex_lock(&global_state.ruid_list_mutex);
    for (i = 0; i < cnt_ruid; i++) {
        if (strcmp(list_ruid[i].ruid, json_string_value(json_ruid)) == 0) {
            strncpy(list_ruid[i].lastVersion, list_ruid[i].currentVersion, sizeof(list_ruid[i].lastVersion));
            strncpy(list_ruid[i].last_save_path, list_ruid[i].current_save_path, sizeof(list_ruid[i].last_save_path));
            strncpy(task.updateVersion, json_string_value(json_updateVersion), sizeof(task.updateVersion));
            strncpy(task.save_path, list_ruid[i].current_save_path, sizeof(task.save_path));
        }
    }
    pthread_mutex_unlock(&global_state.ruid_list_mutex);

    const char* requestId = json_string_value(json_requestId);
    const char* ruid = json_string_value(json_ruid);
    const char* updateVersion = json_string_value(json_updateVersion);
    //
    strncpy(task.requestId, requestId, sizeof(task.requestId));
    strncpy(task.ruid, ruid, sizeof(task.ruid));
    strncpy(task.updateVersion, updateVersion, sizeof(task.updateVersion));
    task.updateType = UPDATE_TYPE_SOFTWARE;
    strncpy(task.status, "pending", sizeof(task.status));

    // 5. 添加到全局任务队列
    pthread_mutex_lock(&global_state.update_task_mutex);
    if (global_state.update_task_count < MAX_UPDATE_TASK_QUEUE) {
        global_state.update_task_queue[global_state.update_task_count++] = task;

    }
    pthread_mutex_unlock(&global_state.update_task_mutex);


    // 6. 创建消息
    Message msg = {
        .type = MSG_UPDATE_TASK,
        .data = malloc(sizeof(UpdateTask)),
        .data_size = sizeof(UpdateTask),
        .source = 0  //  eth0
    };
    memcpy(msg.data, &task, sizeof(UpdateTask));

    //  7. 入队处理
    if (enqueue_message(&global_state, &msg)) {
        json_t* resp = json_object();
        json_object_set_new(resp, "requestId", json_string(requestId));
        // 添加CORS头信息
        add_cors_headers(response);
        ulfius_set_json_body_response(response, 200, resp);
        json_decref(resp);
    }  else {
        json_t* resp = json_object();
        json_object_set_new(resp, "errorInfo", json_string("更新队列已满"));
        // 添加CORS头信息
        add_cors_headers(response);
        ulfius_set_json_body_response(response, 503, resp);
        json_decref(resp);
    }

    // // 8. 释放资源

    //free(msg.data);  // 释放内存

    return U_CALLBACK_CONTINUE;
}

/// ==================恢复出厂配置请求接口回调 ==================
int reinitiate_callback(const struct _u_request* request,
    struct _u_response* response,
    void* user_data) {
    int i = 0;
    int gotVersionCount = 0;
    // 1. 验证accessToken
    const char* access_token = u_map_get(request->map_header, "accessToken");
    if (!access_token) {
        ulfius_add_header_to_response(response, "Content-Type", "application/JSON;charset=UTF-8");
        ulfius_set_string_body_response(response, 401,
            "{\"error\":\"Missing accessToken header\"}");
        return U_CALLBACK_CONTINUE;
    }
    int result = verify_jwt(access_token);
    if (result != 0) {
        ulfius_add_header_to_response(response, "Content-Type", "application/JSON;charset=UTF-8");
        switch (result) {
        case -1:
            ulfius_add_header_to_response(response, "Content-Type", "application/json");
            ulfius_set_string_body_response(response, 500, "Internal server error");
            break;
        case -2:
            ulfius_add_header_to_response(response, "Content-Type", "application/json");
            ulfius_set_string_body_response(response, 401,
                "{\"error\":\"Invalid token signature\"}");
            break;
        case -3:
            ulfius_add_header_to_response(response, "Content-Type", "application/json");
            ulfius_set_string_body_response(response, 401,
                "{\"error\":\"Token expired\"}");
            break;
        case -4:
            ulfius_add_header_to_response(response, "Content-Type", "application/json");
            ulfius_set_string_body_response(response, 401,
                "{\"error\":\"Token revoked\"}");
            break;
        }
        return U_CALLBACK_CONTINUE;
    }

    // 2. 解析请求体
    json_t* root = ulfius_get_json_body_request(request, NULL);
    if (!root) {
        ulfius_add_header_to_response(response, "Content-Type", "application/JSON;charset=UTF-8");
        ulfius_set_string_body_response(response, 400, "Invalid JSON");
        return U_CALLBACK_CONTINUE;
    }

    // 3. 获取参数
    json_t* json_requestId = json_object_get(root, "requestId");
    json_t* json_ruid = json_object_get(root, "ruid");
    // json_t* json_updateVersion = json_object_get(root, "updateVersion");
    pthread_mutex_lock(&global_state.ruid_list_mutex);
    for (i = 0; i < cnt_ruid; i++) {
        if (strcmp(list_ruid[i].ruid, json_string_value(json_ruid)) == 0) {
            gotVersionCount = list_ruid[i].versionCount;
        }
    }
    pthread_mutex_unlock(&global_state.ruid_list_mutex);

    if (!json_is_string(json_requestId) ||
        !json_is_string(json_ruid)) {
        json_decref(root);
        // 添加CORS头信息
        add_cors_headers(response);
        ulfius_add_header_to_response(response, "Content-Type", "application/JSON;charset=UTF-8");
        ulfius_set_string_body_response(response, 400,
            "Missing or invalid parameters");
        return U_CALLBACK_CONTINUE;
    }
    // 检查ruid是否有效
    pthread_mutex_lock(&global_state.ruid_list_mutex);
    int ruid_found = 0;
    for (i = 0; i < cnt_ruid; i++) {
        if (strcmp(list_ruid[i].ruid, json_string_value(json_ruid)) == 0) {
            ruid_found = 1;
            break;
        }
    }
    pthread_mutex_unlock(&global_state.ruid_list_mutex);
    
    if (!ruid_found) {
        json_decref(root);
        // 添加CORS头信息
        add_cors_headers(response);
        ulfius_add_header_to_response(response, "Content-Type", "application/JSON;charset=UTF-8");
        ulfius_set_string_body_response(response, 400,
            "Invalid ruid");
        return U_CALLBACK_CONTINUE;
    }

    const char* requestId = json_string_value(json_requestId);
    const char* ruid = json_string_value(json_ruid);
    
    // 获取备份版本信息
    pthread_mutex_lock(&global_state.ruid_list_mutex);
    char updateVersion[64] = "";
    for (i = 0; i < cnt_ruid; i++) {
        if (strcmp(list_ruid[i].ruid, ruid) == 0 && list_ruid[i].backupVersionList[0] != NULL) {
            strncpy(updateVersion, list_ruid[i].backupVersionList[0], sizeof(updateVersion) - 1);
            break;
        }
    }
    pthread_mutex_unlock(&global_state.ruid_list_mutex);

    //  4. 创建更新任务
    UpdateTask task;
    strncpy(task.requestId, requestId, sizeof(task.requestId));
    strncpy(task.ruid, ruid, sizeof(task.ruid));
    strncpy(task.updateVersion, updateVersion, sizeof(task.updateVersion));
    task.updateType = UPDATE_TYPE_REINITIATE;
    strncpy(task.status, "pending", sizeof(task.status));

    // 5. 添加到全局任务队列
    pthread_mutex_lock(&global_state.update_task_mutex);
    if (global_state.update_task_count < MAX_UPDATE_TASK_QUEUE) {
        global_state.update_task_queue[global_state.update_task_count++] = task;
        global_state.update_task_count++;
    }
    pthread_mutex_unlock(&global_state.update_task_mutex);


    // 6. 创建消息
    Message msg = {
        .type = MSG_UPDATE_TASK,
        .data = malloc(sizeof(UpdateTask)),
        .data_size = sizeof(UpdateTask),
        .source = 0  // eth0
    };
    memcpy(msg.data, &task, sizeof(UpdateTask));

    //  7. 入队处理
    json_t* resp = json_object();
    
    if (enqueue_message(&global_state, &msg)) {
        json_object_set_new(resp, "requestId", json_string(requestId));
        // 添加CORS头信息
        add_cors_headers(response);
        ulfius_set_json_body_response(response, 200, resp);
    } else {
        json_object_set_new(resp, "errorInfo", json_string("更新队列已满"));
        // 添加CORS头信息
        add_cors_headers(response);
        ulfius_set_json_body_response(response, 503, resp);
    }
    json_decref(resp);

    // // 8. 释放资源

    free(msg.data);  // 释放内存
    json_decref(root);

    return U_CALLBACK_CONTINUE;
}

// ==================软件回退请求接口回调
int software_rollback_callback(const struct _u_request* request,
    struct _u_response* response,
    void* user_data) {
    int i = 0;
    int gotVersionCount = 0;
    char gotLastVersion[64];
    // 1. 验证accessToken
    const char* access_token = u_map_get(request->map_header, "accessToken");
    if (!access_token) {
        // 添加CORS头信息
        add_cors_headers(response);
        ulfius_add_header_to_response(response, "Content-Type", "application/JSON;charset=UTF-8");
        ulfius_set_string_body_response(response, 401,
            "{\"error\":\"Missing accessToken header\"}");
        return U_CALLBACK_CONTINUE;
    }
    int result = verify_jwt(access_token);
    if (result != 0) {
        // 添加CORS头信息
        add_cors_headers(response);
        ulfius_add_header_to_response(response, "Content-Type", "application/JSON;charset=UTF-8");
        switch (result) {
        case -1:
            ulfius_set_string_body_response(response, 500, "Internal server error");
            break;
        case -2:
            ulfius_set_string_body_response(response, 401,
                "{\"error\":\"Invalid token signature\"}");
            break;
        case -3:
            ulfius_set_string_body_response(response, 401,
                "{\"error\":\"Token expired\"}");
            break;
        case -4:
            ulfius_set_string_body_response(response, 401,
                "{\"error\":\"Token revoked\"}");
            break;
        }
        return U_CALLBACK_CONTINUE;
    }

    // 2. 解析请求体
    json_t* root = ulfius_get_json_body_request(request, NULL);
    if (!root) {
        // 添加CORS头信息
        add_cors_headers(response);
        ulfius_set_string_body_response(response, 400, "Invalid JSON");
        return U_CALLBACK_CONTINUE;
    }

    // 3. 获取参数
    json_t* json_requestId = json_object_get(root, "requestId");
    json_t* json_ruid = json_object_get(root, "ruid");
    // json_t* json_updateVersion = json_object_get(root, "updateVersion");
    
    pthread_mutex_lock(&global_state.ruid_list_mutex);
    for (i = 0; i < cnt_ruid; i++) {
        if (strcmp(list_ruid[i].ruid, json_string_value(json_ruid)) == 0) {
            gotVersionCount = list_ruid[i].versionCount;
            strncpy(gotLastVersion, list_ruid[i].lastVersion, sizeof(gotLastVersion) - 1);
        }
    }
    pthread_mutex_unlock(&global_state.ruid_list_mutex);


    if (!json_is_integer(json_requestId) ||
        !json_is_string(json_ruid)) {
        json_decref(root);
        // 添加CORS头信息
        add_cors_headers(response);
        ulfius_add_header_to_response(response, "Content-Type", "application/JSON;charset=UTF-8");
        ulfius_set_string_body_response(response, 400,
            "Missing or invalid parameters");
        return U_CALLBACK_CONTINUE;
    }
    if (gotVersionCount == 0) {
        json_decref(root);
        // 添加CORS头信息
        add_cors_headers(response);
        ulfius_add_header_to_response(response, "Content-Type", "application/JSON;charset=UTF-8");
        ulfius_set_string_body_response(response, 400,
            "Invalid firmware version information");
        return U_CALLBACK_CONTINUE;
    }

    const char* requestId = json_string_value(json_requestId);
    const char* ruid = json_string_value(json_ruid);
    
    // 设置回滚版本
    char updateVersion[64] = "";
    if (strlen(gotLastVersion) > 0) {
        strncpy(updateVersion, gotLastVersion, sizeof(updateVersion) - 1);
    }

    // 4. 创建更新任务
    UpdateTask task;
    strncpy(task.requestId, requestId, sizeof(task.requestId));
    strncpy(task.ruid, ruid, sizeof(task.ruid));
    strncpy(task.updateVersion, updateVersion, sizeof(task.updateVersion));
    task.updateType = UPDATE_TYPE_ROLLBACK;
    strncpy(task.status, "pending", sizeof(task.status));

    // 5. 添加到全局任务队列
    pthread_mutex_lock(&global_state.update_task_mutex);
    if (global_state.update_task_count < MAX_UPDATE_TASK_QUEUE) {
        global_state.update_task_queue[global_state.update_task_count++] = task;
        global_state.update_task_count++;
    }
    pthread_mutex_unlock(&global_state.update_task_mutex);


    // 6. 创建消息
    Message msg = {
        .type = MSG_UPDATE_TASK,
        .data = malloc(sizeof(UpdateTask)),
        .data_size = sizeof(UpdateTask),
        .source = 0  // eth0
    };
    memcpy(msg.data, &task, sizeof(UpdateTask));

    // 7. 入队处理
    json_t* resp = json_object();
    
    if (enqueue_message(&global_state, &msg)) {
        json_object_set_new(resp, "requestId", json_string(requestId));
        // 添加CORS头信息
        add_cors_headers(response);
        ulfius_set_json_body_response(response, 200, resp);
    } else {
        json_object_set_new(resp, "errorInfo", json_string("更新队列已满"));
        // 添加CORS头信息
        add_cors_headers(response);
        ulfius_set_json_body_response(response, 503, resp);
    }
    json_decref(resp);

    // // 8. 释放资源

    free(msg.data);  //  释放内存
    json_decref(root);

    return U_CALLBACK_CONTINUE;
}


// ================== 更新状态查询接口回调 ==================
int software_update_status_callback(const struct _u_request* request,
    struct _u_response* response,
    void* user_data) {

    int i = 0;
    // 1. 验证accessToken
    const char* access_token = u_map_get(request->map_header, "accessToken");
    if (!access_token) {
        // 添加CORS头信息
        add_cors_headers(response);
        ulfius_add_header_to_response(response, "Content-Type", "application/json");
        ulfius_set_string_body_response(response, 401,
            "{\"error\":\"Missing accessToken header\"}");
        return U_CALLBACK_CONTINUE;
    }

    int result = verify_jwt(access_token);
    if (result != 0) {
        // 添加CORS头信息
        add_cors_headers(response);
        switch (result) {
        case -1:
            ulfius_set_string_body_response(response, 500, "Internal server error");
            break;
        case -2:
            ulfius_set_string_body_response(response, 401,
                "{\"error\":\"Invalid token signature\"}");
            break;
        case -3:
            ulfius_set_string_body_response(response, 401,
                "{\"error\":\"Token expired\"}");
            break;
        case -4:
            ulfius_set_string_body_response(response, 401,
                "{\"error\":\"Token revoked\"}");
            break;
        }
        return U_CALLBACK_CONTINUE;
    }

    // 2. 解析查询参数
    json_t* root = ulfius_get_json_body_request(request, NULL);
    if (!root) {
        // 添加CORS头信息
        add_cors_headers(response);
        ulfius_add_header_to_response(response, "Content-Type", "application/json");
        ulfius_set_string_body_response(response, 400, "Invalid JSON format");
        return U_CALLBACK_CONTINUE;
    }

json_t* json_requestId = json_object_get(root, "requestId");
json_t* json_ruid = json_object_get(root, "ruid");

if (!json_is_integer(json_requestId) ||
    !json_is_string(json_ruid)) {
    json_decref(root);
    // 添加CORS头信息
    add_cors_headers(response);
    ulfius_set_string_body_response(response, 400,
        "Missing or invalid parameters");
    return U_CALLBACK_CONTINUE;
}

const char* requestId = json_string_value(json_requestId);
const char* ruid = json_string_value(json_ruid);

// 3. 创建查询请求和响应结构
QueryRequest* query_request = (QueryRequest*)malloc(sizeof(QueryRequest));
if (!query_request) {
        // 添加CORS头信息
        add_cors_headers(response);
        ulfius_add_header_to_response(response, "Content-Type", "application/json");
        ulfius_set_string_body_response(response, 500, "Internal server error");
        json_decref(root);
        return U_CALLBACK_CONTINUE;
    }

QueryResponse* query_response = (QueryResponse*)malloc(sizeof(QueryResponse));
if (!query_response) {
        free(query_request);
        // 添加CORS头信息
        add_cors_headers(response);
        ulfius_add_header_to_response(response, "Content-Type", "application/json");
        ulfius_set_string_body_response(response, 500, "Internal server error");
        json_decref(root);
        return U_CALLBACK_CONTINUE;
    }

// 初始化互斥锁和条件变量
pthread_mutex_t response_mutex;
 pthread_cond_t response_cond;
 pthread_mutex_init(&response_mutex, NULL);
 pthread_cond_init(&response_cond, NULL);

// 填充查询请求数据
strncpy(query_request->requestId, requestId, sizeof(query_request->requestId) - 1);
strncpy(query_request->ruid, ruid, sizeof(query_request->ruid) - 1);
query_request->response_mutex = &response_mutex;
query_request->response_cond = &response_cond;
query_request->response = query_response;

// 4. 创建并发送消息
Message msg;
msg.type = MSG_QUERY_UPDATE_STATUS;
msg.data = query_request;
msg.data_size = sizeof(QueryRequest);
msg.source = ETH0_SOURCE;

if (!enqueue_message(&global_state, &msg)) {
    // 消息队列已满
    free(query_request);
    free(query_response);
    pthread_mutex_destroy(&response_mutex);
    pthread_cond_destroy(&response_cond);
    // 添加CORS头信息
    add_cors_headers(response);
    ulfius_set_string_body_response(response, 503, "{\"error\":\"Service unavailable\"}");
    json_decref(root);
    return U_CALLBACK_CONTINUE;
}

// 5. 等待查询结果 (设置5秒超时)
pthread_mutex_lock(&response_mutex);

struct timespec timeout;
clock_gettime(CLOCK_REALTIME, &timeout);
timeout.tv_sec += 5; // 5秒超时

int wait_result = pthread_cond_timedwait(&response_cond, &response_mutex, &timeout);
pthread_mutex_unlock(&response_mutex);

if (wait_result == ETIMEDOUT) {
        // 查询超时
        // 添加CORS头信息
        add_cors_headers(response);
        ulfius_add_header_to_response(response, "Content-Type", "application/json");
        ulfius_set_string_body_response(response, 504, "{\"error\":\"Query timeout\"}");
    } else {
    // 使用查询结果构建HTTP响应
    if (query_response->status_code == 200) {
        // 成功响应
        json_t* resp_json = json_loads(query_response->response_data, 0, NULL);
        if (resp_json) {
            // 添加CORS头信息
            add_cors_headers(response);
            ulfius_set_json_body_response(response, 200, resp_json);
            json_decref(resp_json);
        } else {
            // 添加CORS头信息
            add_cors_headers(response);
            ulfius_add_header_to_response(response, "Content-Type", "application/json");
            ulfius_set_string_body_response(response, 500, "{\"error\":\"Invalid response data\"}");
        }
    } else {
        // 错误响应
        // 添加CORS头信息
        add_cors_headers(response);
        ulfius_add_header_to_response(response, "Content-Type", "application/json");
        ulfius_set_string_body_response(response, query_response->status_code, query_response->response_data);
    }
}

// 6. 清理资源
free(query_response);
pthread_mutex_destroy(&response_mutex);
pthread_cond_destroy(&response_cond);
json_decref(root);
return U_CALLBACK_CONTINUE;
}

//================软件恢复出厂设置状态查询====================
int reinitiate_status_callback(const struct _u_request* request,
    struct _u_response* response,
    void* user_data) {
    int i = 0;
    // 1. 验证accessToken
    const char* access_token = u_map_get(request->map_header, "accessToken");
    if (!access_token) {
        // 添加CORS头信息
        add_cors_headers(response);
        ulfius_add_header_to_response(response, "Content-Type", "application/json");
        ulfius_set_string_body_response(response, 401,
            "{\"error\":\"Missing accessToken header\"}");
        return U_CALLBACK_CONTINUE;
    }

    int result = verify_jwt(access_token);
    if (result != 0) {
        // 添加CORS头信息
        add_cors_headers(response);
        switch (result) {
        case -1:
            ulfius_set_string_body_response(response, 500, "Internal server error");
            break;
        case -2:
            ulfius_set_string_body_response(response, 401,
                "{\"error\":\"Invalid token signature\"}");
            break;
        case -3:
            ulfius_set_string_body_response(response, 401,
                "{\"error\":\"Token expired\"}");
            break;
        case -4:
            ulfius_set_string_body_response(response, 401,
                "{\"error\":\"Token revoked\"}");
            break;
        }
        return U_CALLBACK_CONTINUE;
    }

    // 2. 解析查询参数
    json_t* root = ulfius_get_json_body_request(request, NULL);
    if (!root) {
        // 添加CORS头信息
        add_cors_headers(response);
        ulfius_add_header_to_response(response, "Content-Type", "application/json");
        ulfius_set_string_body_response(response, 400, "Invalid JSON format");
        return U_CALLBACK_CONTINUE;
    }

json_t* json_requestId = json_object_get(root, "requestId");
json_t* json_ruid = json_object_get(root, "ruid");

if (!json_is_integer(json_requestId) ||
    !json_is_string(json_ruid)) {
    json_decref(root);
    // 添加CORS头信息
    add_cors_headers(response);
    ulfius_set_string_body_response(response, 400,
        "Missing or invalid parameters");
    return U_CALLBACK_CONTINUE;
}

const char* requestId = json_string_value(json_requestId);
const char* ruid = json_string_value(json_ruid);

// 3. 创建查询请求和响应结构
eth0_data_Query_REINITIATE* query_request = (eth0_data_Query_REINITIATE*)malloc(sizeof(eth0_data_Query_REINITIATE));
if (!query_request) {
        // 添加CORS头信息
        add_cors_headers(response);
        ulfius_add_header_to_response(response, "Content-Type", "application/json");
        ulfius_set_string_body_response(response, 500, "Internal server error");
        json_decref(root);
        return U_CALLBACK_CONTINUE;
    }

eth0_Query_REINITIATE_Context* query_response = (eth0_Query_REINITIATE_Context*)malloc(sizeof(eth0_Query_REINITIATE_Context));
if (!query_response) {
        free(query_request);
        // 添加CORS头信息
        add_cors_headers(response);
        ulfius_add_header_to_response(response, "Content-Type", "application/json");
        ulfius_set_string_body_response(response, 500, "Internal server error");
        json_decref(root);
        return U_CALLBACK_CONTINUE;
    }

// 初始化互斥锁和条件变量
pthread_mutex_t response_mutex;
 pthread_cond_t response_cond;
 pthread_mutex_init(&response_mutex, NULL);
 pthread_cond_init(&response_cond, NULL);

// 填充查询请求数据
strncpy(query_request->requestId, requestId, sizeof(query_request->requestId) - 1);
strncpy(query_request->ruid, ruid, sizeof(query_request->ruid) - 1);
query_request->response_mutex = &response_mutex;
query_request->response_cond = &response_cond;
query_request->response = query_response;

// 4. 创建并发送消息
Message msg;
msg.type = MSG_QUERY_REINITIATE_STATUS;
msg.data = query_request;
msg.data_size = sizeof(QueryRequest);
msg.source = ETH0_SOURCE;

if (!enqueue_message(&global_state, &msg)) {
    // 消息队列已满
    free(query_request);
    free(query_response);
    pthread_mutex_destroy(&response_mutex);
    pthread_cond_destroy(&response_cond);
    // 添加CORS头信息
    add_cors_headers(response);
    ulfius_set_string_body_response(response, 503, "{\"error\":\"Service unavailable\"}");
    json_decref(root);
    return U_CALLBACK_CONTINUE;
}

// 5. 等待查询结果 (设置5秒超时)
pthread_mutex_lock(&response_mutex);

struct timespec timeout;
clock_gettime(CLOCK_REALTIME, &timeout);
timeout.tv_sec += 5; // 5秒超时

int wait_result = pthread_cond_timedwait(&response_cond, &response_mutex, &timeout);
pthread_mutex_unlock(&response_mutex);

if (wait_result == ETIMEDOUT) {
        // 查询超时
        // 添加CORS头信息
        add_cors_headers(response);
        ulfius_add_header_to_response(response, "Content-Type", "application/json");
        ulfius_set_string_body_response(response, 504, "{\"error\":\"Query timeout\"}");
    } else {
    // 使用查询结果构建HTTP响应
    if (query_response->status_code == 200) {
        // 成功响应
        json_t* resp_json = json_loads(query_response->response_data, 0, NULL);
        if (resp_json) {
            // 添加CORS头信息
            add_cors_headers(response);
            ulfius_set_json_body_response(response, 200, resp_json);
            json_decref(resp_json);
        } else {
            // 添加CORS头信息
            add_cors_headers(response);
            ulfius_add_header_to_response(response, "Content-Type", "application/json");
            ulfius_set_string_body_response(response, 500, "{\"error\":\"Invalid response data\"}");
        }
    } else {
        // 错误响应
        // 添加CORS头信息
        add_cors_headers(response);
        ulfius_add_header_to_response(response, "Content-Type", "application/json");
        ulfius_set_string_body_response(response, query_response->status_code, query_response->response_data);
    }
}

// 6. 清理资源
free(query_response);
pthread_mutex_destroy(&response_mutex);
pthread_cond_destroy(&response_cond);
json_decref(root);
return U_CALLBACK_CONTINUE;
}

//===================软件回退状态查询==========================
int software_rollback_status_callback(const struct _u_request* request,
    struct _u_response* response,
    void* user_data) {
    int i = 0;
    json_t* root = ulfius_get_json_body_request(request, NULL);
    if (!root) {
        // 添加CORS头信息
        add_cors_headers(response);
        ulfius_add_header_to_response(response, "Content-Type", "application/JSON;charset=UTF-8");
        ulfius_set_string_body_response(response, 400, "Invalid JSON format");
        return U_CALLBACK_CONTINUE;
    }
    // 1. 验证accessToken
    const char* access_token = u_map_get(request->map_header, "accesstoken");
    if (!access_token) {
        // 添加CORS头信息
        add_cors_headers(response);
        ulfius_set_string_body_response(response, 401,
            "{\"error\":\"Missing accessToken header\"}");
        return U_CALLBACK_CONTINUE;
    }

    int result = verify_jwt(access_token);
    if (result != 0) {
        // 添加CORS头信息
        add_cors_headers(response);
        ulfius_add_header_to_response(response, "Content-Type", "application/JSON;charset=UTF-8");
        switch (result) {
        case -1:
            ulfius_set_string_body_response(response, 500, "Internal server error");
            break;
        case -2:
            ulfius_set_string_body_response(response, 401,
                "{\"error\":\"Invalid token signature\"}");
            break;
        case -3:
            ulfius_set_string_body_response(response, 401,
                "{\"error\":\"Token expired\"}");
            break;
        case -4:
            ulfius_set_string_body_response(response, 401,
                "{\"error\":\"Token revoked\"}");
            break;
        }
        return U_CALLBACK_CONTINUE;
    }

    // 2. 解析查询参数
    json_t* json_requestId = json_object_get(root, "requestId");
    json_t* json_ruid = json_object_get(root, "ruid");

    if (!json_is_integer(json_requestId) ||
        !json_is_string(json_ruid)) {
        json_decref(root);
        // 添加CORS头信息
        add_cors_headers(response);
        ulfius_add_header_to_response(response, "Content-Type", "application/JSON;charset=UTF-8");
        ulfius_set_string_body_response(response, 400,
            "Missing or invalid parameters");
        return U_CALLBACK_CONTINUE;
    }

    const char* requestId = json_string_value(json_requestId);
    const char* ruid = json_string_value(json_ruid);

    // 3. 创建查询请求和响应结构
    eth0_data_Query_ROLLBACK* query_request = (eth0_data_Query_ROLLBACK*)malloc(sizeof(eth0_data_Query_ROLLBACK));
    if (!query_request) {
        // 添加CORS头信息
        add_cors_headers(response);
        ulfius_add_header_to_response(response, "Content-Type", "application/JSON;charset=UTF-8");
        ulfius_set_string_body_response(response, 500, "Internal server error");
        json_decref(root);
        return U_CALLBACK_CONTINUE;
    }
    
    eth0_Query_ROLLBACK_Context* query_response = (eth0_Query_ROLLBACK_Context*)malloc(sizeof(eth0_Query_ROLLBACK_Context));
    if (!query_response) {
        free(query_request);
        // 添加CORS头信息
        add_cors_headers(response);
        ulfius_add_header_to_response(response, "Content-Type", "application/JSON;charset=UTF-8");
        ulfius_set_string_body_response(response, 500, "Internal server error");
        json_decref(root);
        return U_CALLBACK_CONTINUE;
    }
    
    // 初始化互斥锁和条件变量
    pthread_mutex_t response_mutex;
    pthread_cond_t response_cond;
    pthread_mutex_init(&response_mutex, NULL);
    pthread_cond_init(&response_cond, NULL);
    
    // 填充查询请求数据
    strncpy(query_request->requestId, requestId, sizeof(query_request->requestId) - 1);
    strncpy(query_request->ruid, ruid, sizeof(query_request->ruid) - 1);
    query_request->response_mutex = &response_mutex;
    query_request->response_cond = &response_cond;
    query_request->response = query_response;
    
    // 4. 创建并发送消息
    Message msg;
    msg.type = MSG_QUERY_ROLLBACK_STATUS;
    msg.data = query_request;
    msg.data_size = sizeof(QueryRequest);
    msg.source = ETH0_SOURCE;
    
    if (!enqueue_message(&global_state, &msg)) {
        // 消息队列已满
        free(query_request);
        free(query_response);
        pthread_mutex_destroy(&response_mutex);
        pthread_cond_destroy(&response_cond);
        // 添加CORS头信息
        add_cors_headers(response);
        ulfius_add_header_to_response(response, "Content-Type", "application/JSON;charset=UTF-8");
        ulfius_set_string_body_response(response, 503, "{\"error\":\"Service unavailable\"}");
        json_decref(root);
        return U_CALLBACK_CONTINUE;
    }
    
    // 5. 等待查询结果 (设置5秒超时)
    pthread_mutex_lock(&response_mutex);
    
    struct timespec timeout;
    clock_gettime(CLOCK_REALTIME, &timeout);
    timeout.tv_sec += 5; // 5秒超时
    
    int wait_result = pthread_cond_timedwait(&response_cond, &response_mutex, &timeout);
    pthread_mutex_unlock(&response_mutex);
    
    if (wait_result == ETIMEDOUT) {
        // 查询超时
        // 添加CORS头信息
        add_cors_headers(response);
        ulfius_add_header_to_response(response, "Content-Type", "application/JSON;charset=UTF-8");
        ulfius_set_string_body_response(response, 504, "{\"error\":\"Query timeout\"}");
    } else {
        // 使用查询结果构建HTTP响应
        if (query_response->status_code == 200) {
            // 成功响应
            json_t* resp_json = json_loads(query_response->response_data, 0, NULL);
            if (resp_json) {
                // 添加CORS头信息
                add_cors_headers(response);
                ulfius_set_json_body_response(response, 200, resp_json);
                json_decref(resp_json);
            } else {
                // 添加CORS头信息
                add_cors_headers(response);
                ulfius_add_header_to_response(response, "Content-Type", "application/JSON;charset=UTF-8");
                ulfius_set_string_body_response(response, 500, "{\"error\":\"Invalid response data\"}");
            }
        } else {
            // 错误响应
            // 添加CORS头信息
            add_cors_headers(response);
            ulfius_add_header_to_response(response, "Content-Type", "application/json");
            ulfius_set_string_body_response(response, query_response->status_code, query_response->response_data);
        }
    }
    
    // 6. 清理资源
    free(query_response);
    pthread_mutex_destroy(&response_mutex);
    pthread_cond_destroy(&response_cond);
    json_decref(root);
    
    return U_CALLBACK_CONTINUE;
}



// 清理资源
void cleanup_resources() {
    int i = 0;
    pthread_mutex_lock(&global_state.blacklist_mutex);
    for (i = 0; i < MAX_BLACKLIST_TOKENS; i++) {
        if (global_state.token_blacklist[i] != NULL) {
            free(global_state.token_blacklist[i]->token);
            free(global_state.token_blacklist[i]);
            global_state.token_blacklist[i] = NULL;
        }
    }
    pthread_mutex_unlock(&global_state.blacklist_mutex);
}

// ================== 信号处理 ==================
void signal_handler(int sig) {
    printf("\n收到信号 %d, 正在关闭...\n", sig);
    atomic_store(&global_state.shutdown_requested, 1);

    // 唤醒所有等待的线程
    pthread_cond_broadcast(&global_state.dl_task_cond);
    // 注意：GlobalState结构体中没有update_task_cond成员，所以移除这一行
}
