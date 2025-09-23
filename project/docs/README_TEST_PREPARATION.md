# RESTful接口测试准备文档

## 1. 测试环境概述

### 1.1 网络拓扑
- **板子IP**: 192.168.0.3
- **虚拟机IP**: 192.168.0.2
- **电脑主机IP**: 192.168.0.1
- **通信端口**: 443 (HTTPS)

### 1.2 可用工具
- **命令行测试**: curl (推荐)
- **专业测试软件**: Postman、Insomnia等
- **网络监控**: 虚拟机已安装Wireshark

### 1.3 前置条件
- 默认debug已成功完成
- 设备正常运行并已启动RESTful服务
- 电脑主机可以访问板子(192.168.0.3)
- 已安装必要的依赖工具：curl和python



## 2. 已准备的测试文档

### 2.1 RESTful接口测试指南
**文件名**: `restful_test_guide.md`
**主要内容**: 
- 详细的接口说明（URL、方法、参数、请求体格式）
- 认证流程说明
- 各功能接口（版本查询、软件下载、更新、回退等）的测试方法
- curl命令示例
- 测试注意事项

### 2.2 RESTful测试脚本
**文件名**: `restful_test_script.sh`
**主要功能**: 
- 封装了所有RESTful接口的测试命令
- 自动管理认证令牌
- 提供交互式命令行界面
- 支持所有系统管理功能的测试
- 可直接在Linux/Mac终端或Windows WSL中运行

## 3. 测试脚本使用指南

### 3.1 脚本授权
在使用脚本前，需要先添加执行权限：
```bash
chmod +x docs/restful_test_script.sh
```

### 3.2 基本使用方法
```bash
# 查看帮助信息
bash docs/restful_test_script.sh help

# 登录系统
bash docs/restful_test_script.sh login

# 查询软件版本
bash docs/restful_test_script.sh get_version

# 开始软件下载
bash docs/restful_test_script.sh start_download

# 检查下载状态
bash docs/restful_test_script.sh check_download
```

### 3.3 主要命令说明
- `login`: 登录系统并获取访问令牌
- `handshake`: 验证令牌有效性
- `logout`: 注销当前令牌
- `get_version`: 查询软件版本信息
- `start_download`: 发起软件下载请求
- `check_download`: 检查下载进度和状态
- `start_update`: 发起软件更新请求
- `check_update`: 检查更新进度和状态
- `start_reinitiate`: 发起恢复出厂配置请求
- `check_reinitiate`: 检查恢复出厂配置状态
- `start_rollback`: 发起软件回退请求
- `check_rollback`: 检查软件回退状态

### 3.4 环境变量配置
脚本支持通过环境变量自定义配置：
```bash
# 临时修改BASE_URL
export BASE_URL="https://192.168.0.3:443/api/v4"
bash docs/restful_test_script.sh login
```

## 4. 调试流程总结

### 4.1 基本调试步骤
1. **确认网络连通性**: 使用ping命令测试电脑主机到板子的连接
2. **服务可用性检查**: 确认RESTful服务是否在板子上正常启动
3. **认证测试**: 执行登录命令获取访问令牌
4. **功能测试**: 使用获取的令牌测试各个RESTful接口
5. **问题排查**: 如遇失败，检查错误信息并分析原因

### 4.2 网络监控方法
在虚拟机(192.168.0.2)上使用Wireshark监控网络通信：
1. 启动Wireshark
2. 选择对应网络接口
3. 设置过滤条件: `host 192.168.0.3 and port 443`
4. 开始捕获数据包
5. 分析请求和响应数据

## 5. 联调准备事项

### 5.1 与地面设计联调前准备
1. **确认接口规范**: 确保双方对接口格式、参数和返回值有一致理解
2. **准备测试用例**: 制定详细的测试用例，覆盖正常流程和异常情况
3. **环境准备**: 确保测试环境（网络、设备状态）满足联调要求
4. **问题记录方案**: 准备问题记录表格，记录测试过程中发现的问题

### 5.2 联调测试流程建议
1. **基础通信测试**: 验证双方能正常发送和接收请求
2. **认证流程测试**: 测试登录、握手、登出全流程
3. **功能接口测试**: 逐一测试各个功能接口的正确性
4. **边界条件测试**: 测试参数边界、异常输入等情况



