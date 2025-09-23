#!/bin/bash

# RESTful接口测试脚本
# 使用方法：bash restful_test_script.sh [command] [options]

# 配置参数 - 根据API示例更新
BASE_URL="http://192.168.11.3:443/api/1.0"
CURL_OPTS="-s"  # -s: 静默模式 (注意：服务器使用HTTP协议，不需要SSL验证)
ACCESS_TOKEN=""
REQUEST_ID="$(date +%Y%m%d%H%M%S)"
RUID="device_001"

# 函数：显示帮助信息
display_help() {
    echo "RESTful接口测试脚本"
    echo ""
    echo "用法: bash restful_test_script.sh [command] [options]"
    echo ""
    echo "命令:"
    echo "  login                登录系统获取访问令牌"
    echo "  handshake            验证令牌有效性"
    echo "  logout               注销令牌"
    echo "  get_version          查询软件版本"
    echo "  start_download       开始软件下载"
    echo "  check_download       检查下载状态"
    echo "  start_update         开始软件更新"
    echo "  check_update         检查更新状态"
    echo "  start_reinitiate     开始恢复出厂配置"
    echo "  check_reinitiate     检查恢复出厂配置状态"
    echo "  start_rollback       开始软件回退"
    echo "  check_rollback       检查软件回退状态"
    echo "  help                 显示帮助信息"
    echo ""
    echo "环境变量:"
    echo "  BASE_URL             API基础URL (默认: $BASE_URL)"
    echo "  ACCESS_TOKEN         访问令牌 (登录后自动设置)"
    echo "  REQUEST_ID           请求ID (默认自动生成)"
    echo "  RUID                 网元标识 (默认: $RUID)"
}

# 函数：登录获取令牌 - 按照API示例格式
login() {
    echo "[INFO] 尝试登录系统..."
    echo "[INFO] 登录请求示例格式："
    echo "POST $BASE_URL/securityManagement/oauth/token HTTP/1.1"
    echo "Host: 192.168.0.3:443"
    echo "Content-Type: application/json;charset=UTF-8"
    echo ""
    echo "{" 
    echo "  \"grantType\":\"password\"," 
    echo "  \"userName\":\"test\"," 
    echo "  \"value\":\"XXXXXX\"" 
    echo "}"
    
    # 使用服务器端支持的用户名和密码
    response=$(curl $CURL_OPTS -X POST "$BASE_URL/securityManagement/oauth/token" \
        -H "Content-Type: application/json" \
        -d '{"grantType":"password","username":"admin","value":"admin123"}')
    
    # 检查响应
    if [[ $? -ne 0 ]]; then
        echo "[ERROR] 登录请求失败，请检查网络连接或服务状态"
        return 1
    fi
    
    # 提取accessToken
    ACCESS_TOKEN=$(echo $response | grep -o '"accessToken":"[^"]*' | cut -d':' -f2 | tr -d '"')
    expires=$(echo $response | grep -o '"expires":[0-9]*' | cut -d':' -f2)
    
    if [[ -z "$ACCESS_TOKEN" ]]; then
        echo "[ERROR] 登录失败: $response"
        # 显示失败响应示例
        echo "[INFO] 失败响应示例："
        echo "HTTP/1.1 400 BAD REQUEST"
        echo "Content-Type: application/json;charset=UTF-8"
        echo ""
        echo "{" 
        echo "  // 错误信息" 
        echo "}"
        return 1
    fi
    
    echo "[SUCCESS] 登录成功!"
    echo "[INFO] 成功响应示例："
    echo "HTTP/1.1 200 OK"
    echo "Content-Type: application/json;charset=UTF-8"
    echo ""
    echo "{" 
    echo "  \"accessToken\":\"$ACCESS_TOKEN\"," 
    echo "  \"expires\":$expires" 
    echo "}"
    echo "[INFO] 令牌有效期: $expires秒"
    
    # 保存令牌到临时文件供其他命令使用
    echo "$ACCESS_TOKEN" > "/tmp/restful_access_token"
    echo "[INFO] 令牌已保存到 /tmp/restful_access_token"
    
    return 0
}

# 函数：验证令牌有效性
handshake() {
    load_token || return 1
    
    echo "[INFO] 验证令牌有效性..."
    response=$(curl $CURL_OPTS -X POST "$BASE_URL/securityManagement/oauth/handshake" \
        -H "accesstoken: $ACCESS_TOKEN" \
        -w "%{http_code}")
    
    # 提取HTTP状态码
    http_code=${response: -3}
    response_body=${response%???}
    
    if [[ $http_code -eq 200 ]]; then
        echo "[SUCCESS] 令牌验证成功!"
        return 0
    else
        echo "[ERROR] 令牌验证失败 (HTTP状态码: $http_code)"
        [[ -n "$response_body" ]] && echo "[ERROR] 响应: $response_body"
        return 1
    fi
}

# 函数：注销令牌
logout() {
    load_token || return 1
    
    echo "[INFO] 注销令牌..."
    response=$(curl $CURL_OPTS -X DELETE "$BASE_URL/securityManagement/oauth/token" \
        -H "accesstoken: $ACCESS_TOKEN" \
        -w "%{http_code}")
    
    # 提取HTTP状态码
    http_code=${response: -3}
    
    if [[ $http_code -eq 204 ]]; then
        echo "[SUCCESS] 令牌注销成功!"
        # 删除保存的令牌
        rm -f "/tmp/restful_access_token"
        ACCESS_TOKEN=""
        return 0
    else
        echo "[ERROR] 令牌注销失败 (HTTP状态码: $http_code)"
        return 1
    fi
}

# 函数：查询软件版本
get_version() {
    load_token || return 1
    
    echo "[INFO] 查询软件版本..."
    response=$(curl $CURL_OPTS -X GET "$BASE_URL/systemManagement/softwareVersion?requestId=$REQUEST_ID&ruid=$RUID" \
        -H "accessToken: $ACCESS_TOKEN")
    
    if [[ $? -ne 0 ]]; then
        echo "[ERROR] 查询失败，请检查网络连接或服务状态"
        return 1
    fi
    
    echo "[INFO] 软件版本查询结果:"
    echo $response | python -m json.tool 2>/dev/null || echo $response
    return 0
}

# 函数：开始软件下载
start_download() {
    load_token || return 1
    
    # 生成唯一的请求ID（根据接口文档应为整型）
    local dl_request_id=$(date +%Y%m%d%H%M%S)
    
    echo "[INFO] 开始软件下载任务..."
    echo "[INFO] 请求ID: $dl_request_id"
    
    # 根据接口文裆要求，需要提供以下参数：
    # - requestId: 整型，请求消息序号
    # - softwareVersion: 字符串，下载软件版本
    # - downloadPath: 字符串，软件的下载路径（HTTP/HTTPS格式）
    local software_version="7.1.12"
    local download_path="https://example.com/nms/data/test.zip"
    
    echo "执行下载请求: curl $CURL_OPTS -X POST \"$BASE_URL/systemManagement/softwareDownload\" \
        -H \"accessToken: $ACCESS_TOKEN\" \
        -H \"Content-Type: application/json\" \
        -d \"{\"requestId\":$dl_request_id,\"softwareVersion\":\"$software_version\",\"downloadPath\":\"$download_path\"}\""
    
    response=$(curl $CURL_OPTS -X POST "$BASE_URL/systemManagement/softwareDownload" \
        -H "accessToken: $ACCESS_TOKEN" \
        -H "Content-Type: application/json" \
        -d "{\"requestId\":$dl_request_id,\"softwareVersion\":\"$software_version\",\"downloadPath\":\"$download_path\"}")
    if [[ $? -ne 0 ]]; then
        echo "[ERROR] 下载请求失败，请检查网络连接或服务状态"
        return 1
    fi
    echo $response
    echo "[INFO] 软件下载请求结果:"
    echo $response | python -m json.tool 2>/dev/null || echo $response
    echo "[INFO] 可使用 check_download 命令查询下载状态"
    
    # 保存请求ID供后续查询使用
    echo "$dl_request_id" > "/tmp/restful_last_download_id"
    echo "[INFO] 下载请求ID已保存到 /tmp/restful_last_download_id"
    
    return 0
}

# 函数：检查下载状态
check_download() {
    load_token || return 1
    
    # 获取下载请求ID
    local dl_request_id=$1
    if [[ -z "$dl_request_id" ]]; then
        dl_request_id=$(cat "/tmp/restful_last_download_id" 2>/dev/null)
    fi
    
    if [[ -z "$dl_request_id" ]]; then
        echo "[ERROR] 请提供下载请求ID，或先执行start_download命令"
        echo "[INFO] 用法: check_download [request_id]"
        return 1
    fi
    
    echo "[INFO] 检查下载状态 (请求ID: $dl_request_id)..."
    # 根据接口文档 IF_NMS_SMC_SYS_003 要求，GET请求只需包含requestId参数
    response=$(curl $CURL_OPTS -X GET "$BASE_URL/systemManagement/softwareDownload?requestId=$dl_request_id" \
        -H "accesstoken: $ACCESS_TOKEN")
    
    if [[ $? -ne 0 ]]; then
        echo "[ERROR] 查询失败，请检查网络连接或服务状态"
        return 1
    fi
    
    echo "[INFO] 下载状态查询结果:"
    echo $response | python -m json.tool 2>/dev/null || echo $response
    return 0
}

# 函数：开始软件更新
start_update() {
    load_token || return 1
    
    # 生成唯一的请求ID
    local update_request_id="$(date +%Y%m%d%H%M%S)"
    
    echo "[INFO] 开始软件更新任务..."
    echo "[INFO] 请求ID: $update_request_id"
    
    # 使用文档要求的updateVersion参数，而不是filePath
    local update_version="7.1.12"  # 默认版本号，可根据实际情况修改
    
    response=$(curl $CURL_OPTS -X POST "$BASE_URL/systemManagement/softwareUpdate" \
        -H "accessToken: $ACCESS_TOKEN" \
        -H "Content-Type: application/json" \
        -d "{\"requestId\":\"$update_request_id\",\"ruid\":\"$RUID\",\"updateVersion\":\"$update_version\"}")
    
    if [[ $? -ne 0 ]]; then
        echo "[ERROR] 更新请求失败，请检查网络连接或服务状态"
        return 1
    fi
    
    echo "[INFO] 软件更新请求结果:"
    echo $response | python -m json.tool 2>/dev/null || echo $response
    echo "[INFO] 可使用 check_update 命令查询更新状态"
    
    # 保存请求ID供后续查询使用
    echo "$update_request_id" > "/tmp/restful_last_update_id"
    echo "[INFO] 更新请求ID已保存到 /tmp/restful_last_update_id"
    
    return 0
}

# 函数：检查更新状态
check_update() {
    load_token || return 1
    
    # 获取更新请求ID
    local update_request_id=$1
    if [[ -z "$update_request_id" ]]; then
        update_request_id=$(cat "/tmp/restful_last_update_id" 2>/dev/null)
    fi
    
    if [[ -z "$update_request_id" ]]; then
        echo "[ERROR] 请提供更新请求ID，或先执行start_update命令"
        echo "[INFO] 用法: check_update [request_id]"
        return 1
    fi
    
    echo "[INFO] 检查更新状态 (请求ID: $update_request_id)..."
    response=$(curl $CURL_OPTS -X GET "$BASE_URL/systemManagement/softwareUpdate?requestId=$update_request_id&ruid=$RUID" \
        -H "accessToken: $ACCESS_TOKEN")
    
    if [[ $? -ne 0 ]]; then
        echo "[ERROR] 查询失败，请检查网络连接或服务状态"
        return 1
    fi
    
    echo "[INFO] 更新状态查询结果:"
    echo $response | python -m json.tool 2>/dev/null || echo $response
    return 0
}

# 函数：开始恢复出厂配置
start_reinitiate() {
    load_token || return 1
    
    # 生成唯一的请求ID（使用时间戳作为整型requestId）
    local reinit_request_id=$(date +%s%N | cut -c1-13)
    
    echo "[WARNING] 即将执行恢复出厂配置操作，此操作不可恢复!"
    echo -n "[WARNING] 请确认是否继续? (y/n): "
    read confirm
    
    if [[ $confirm != "y" && $confirm != "Y" ]]; then
        echo "[INFO] 操作已取消"
        return 0
    fi
    
    echo "[INFO] 开始恢复出厂配置任务..."
    echo "[INFO] 请求ID: $reinit_request_id"
    
    response=$(curl $CURL_OPTS -X POST "$BASE_URL/systemManagement/reinitiate" \
        -H "accesstoken: $ACCESS_TOKEN" \
        -H "Content-Type: application/json" \
        -d "{\"requestId\":\"$reinit_request_id\",\"ruid\":\"$RUID\"}")
    
    if [[ $? -ne 0 ]]; then
        echo "[ERROR] 恢复出厂配置请求失败，请检查网络连接或服务状态"
        return 1
    fi
    
    echo "[INFO] 恢复出厂配置请求结果:"
    echo $response | python -m json.tool 2>/dev/null || echo $response
    echo "[INFO] 可使用 check_reinitiate 命令查询恢复状态"
    
    # 保存请求ID供后续查询使用
    echo "$reinit_request_id" > "/tmp/restful_last_reinit_id"
    echo "[INFO] 恢复出厂配置请求ID已保存到 /tmp/restful_last_reinit_id"
    
    return 0
}

# 函数：检查恢复出厂配置状态
check_reinitiate() {
    load_token || return 1
    
    # 获取恢复出厂配置请求ID
    local reinit_request_id=$1
    if [[ -z "$reinit_request_id" ]]; then
        reinit_request_id=$(cat "/tmp/restful_last_reinit_id" 2>/dev/null)
    fi
    
    if [[ -z "$reinit_request_id" ]]; then
        echo "[ERROR] 请提供恢复出厂配置请求ID，或先执行start_reinitiate命令"
        echo "[INFO] 用法: check_reinitiate [request_id]"
        return 1
    fi
    
    echo "[INFO] 检查恢复出厂配置状态 (请求ID: $reinit_request_id)..."
    response=$(curl $CURL_OPTS -X GET "$BASE_URL/systemManagement/reinitiate?requestId=$reinit_request_id&ruid=$RUID" \
        -H "accesstoken: $ACCESS_TOKEN")
    
    if [[ $? -ne 0 ]]; then
        echo "[ERROR] 查询失败，请检查网络连接或服务状态"
        return 1
    fi
    
    echo "[INFO] 恢复出厂配置状态查询结果:"
    echo $response | python -m json.tool 2>/dev/null || echo $response
    return 0
}

# 函数：开始软件回退
start_rollback() {
    load_token || return 1
    
    # 生成唯一的请求ID（使用时间戳作为整型requestId）
    local rollback_request_id=$(date +%s%N | cut -c1-13)
    
    echo "[WARNING] 即将执行软件回退操作，此操作可能影响系统功能!"
    echo -n "[WARNING] 请确认是否继续? (y/n): "
    read confirm
    
    if [[ $confirm != "y" && $confirm != "Y" ]]; then
        echo "[INFO] 操作已取消"
        return 0
    fi
    
    echo "[INFO] 开始软件回退任务..."
    echo "[INFO] 请求ID: $rollback_request_id"
    
    response=$(curl $CURL_OPTS -X POST "$BASE_URL/systemManagement/softwareRollback" \
        -H "accessToken: $ACCESS_TOKEN" \
        -H "Content-Type: application/json" \
        -d "{\"requestId\":\"$rollback_request_id\",\"ruid\":\"$RUID\"}")
    
    if [[ $? -ne 0 ]]; then
        echo "[ERROR] 软件回退请求失败，请检查网络连接或服务状态"
        return 1
    fi
    
    echo "[INFO] 软件回退请求结果:"
    echo $response | python -m json.tool 2>/dev/null || echo $response
    echo "[INFO] 可使用 check_rollback 命令查询回退状态"
    
    # 保存请求ID供后续查询使用
    echo "$rollback_request_id" > "/tmp/restful_last_rollback_id"
    echo "[INFO] 软件回退请求ID已保存到 /tmp/restful_last_rollback_id"
    
    return 0
}

# 函数：检查软件回退状态
check_rollback() {
    load_token || return 1
    
    # 获取软件回退请求ID
    local rollback_request_id=$1
    if [[ -z "$rollback_request_id" ]]; then
        rollback_request_id=$(cat "/tmp/restful_last_rollback_id" 2>/dev/null)
    fi
    
    if [[ -z "$rollback_request_id" ]]; then
        echo "[ERROR] 请提供软件回退请求ID，或先执行start_rollback命令"
        echo "[INFO] 用法: check_rollback [request_id]"
        return 1
    fi
    
    echo "[INFO] 检查软件回退状态 (请求ID: $rollback_request_id)..."
    response=$(curl $CURL_OPTS -X GET "$BASE_URL/systemManagement/softwareRollback?requestId=$rollback_request_id&ruid=$RUID" \
        -H "accesstoken: $ACCESS_TOKEN")
    
    if [[ $? -ne 0 ]]; then
        echo "[ERROR] 查询失败，请检查网络连接或服务状态"
        return 1
    fi
    
    echo "[INFO] 软件回退状态查询结果:"
    echo $response | python -m json.tool 2>/dev/null || echo $response
    return 0
}

# 函数：从文件加载令牌
load_token() {
    if [[ -z "$ACCESS_TOKEN" ]]; then
        ACCESS_TOKEN=$(cat "/tmp/restful_access_token" 2>/dev/null)
    fi
    
    if [[ -z "$ACCESS_TOKEN" ]]; then
        echo "[ERROR] 未找到有效的访问令牌，请先执行login命令"
        return 1
    fi
    
    return 0
}

# 主函数
main() {
    case "$1" in
        login)
            login
            ;;
        handshake)
            handshake
            ;;
        logout)
            logout
            ;;
        get_version)
            get_version
            ;;
        start_download)
            start_download
            ;;
        check_download)
            check_download "$2"
            ;;
        start_update)
            start_update
            ;;
        check_update)
            check_update "$2"
            ;;
        start_reinitiate)
            start_reinitiate
            ;;
        check_reinitiate)
            check_reinitiate "$2"
            ;;
        start_rollback)
            start_rollback
            ;;
        check_rollback)
            check_rollback "$2"
            ;;
        help|-h|--help)
            display_help
            ;;
        *)
            echo "[ERROR] 未知命令: $1"
            echo "[INFO] 请使用 'help' 命令查看可用命令列表"
            return 1
            ;;
    esac
}

# 执行主函数
main "$@"

exit $?