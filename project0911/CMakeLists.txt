Cmake_minimum_required(VERSION 3.9)
project(gdb_zizhu VERSION 0.1)

# 指定c++标准
set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)
#set(CMAKE_SYSTEM_NAME linux)
set(CMAKE_C_COMPILER powerpc-fsl-linux-gnuspe-gcc)
set(CMAKE_CXX_COMPILER powerpc-fsl-linux-gnuspe-gcc)
SET(CMAKE_BUILD_TYPE "Debug")
SET(CMAKE_CXX_FLAGS_DEBUG "-m32 -mcpu=8548 -mabi=spe -mspe -mfloat-gprs=double --sysroot=/opt/fsl-qoriq/1.9/sysroots/ppce500v2-fsl-linux-gnuspe -g2 -ggdb ")
SET(CMAKE_CXX_FLAGS_RELEASE "-m32 -mcpu=8548 -mabi=spe -mspe -mfloat-gprs=double --sysroot=/opt/fsl-qoriq/1.9/sysroots/ppce500v2-fsl-linux-gnuspe  -O3 -Wall")

# 混合链接策略：大部分库静态链接，glibc动态链接
# 不设置全局静态链接标志，而是在target_link_libraries中控制
unset(CMAKE_EXE_LINKER_FLAGS)
set(CMAKE_FIND_LIBRARY_SUFFIXES ".a") # 优先查找静态库
set(BUILD_SHARED_LIBS OFF)
set(CMAKE_LINK_SEARCH_END_STATIC FALSE)



set(SYSROOT_PATH "/opt/fsl-qoriq/1.9/sysroots/ppce500v2-fsl-linux-gnuspe")

# 包含目录
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${SYSROOT_PATH}/usr/include
)

# 链接目录
link_directories(
    ${SYSROOT_PATH}/usr/lib
)


# 添加调试信息
#message(STATUS "ULFIUS include path: ${ULFIUS_INCLUDE_DIR}")

###############################
# 可执行文件
add_executable(test0904
    src/main.c
    src/eth0.c
    src/eth1.c
    src/rs422.c
    src/cmd_frame.c
    src/resp_frame.c
    src/data_frame_rs422.c
    src/data_frame_eth1.c
    src/ftp_server.c
    src/app_transfer.c
    src/protocol_tx.c
    src/udp_ctrl_ftp.c
    src/command_frame.c
    src/response_frame.c
    src/app_report.c
    src/udp_cmd.c
    src/update_runner.c)

# 链接库
# 以下代码已被注释，实际使用下方优化后的链接方式
# target_link_libraries(test0904
#     ${SYSROOT_PATH}/usr/lib/libulfius.a
#     ${SYSROOT_PATH}/usr/lib/libmicrohttpd.a
#     ${SYSROOT_PATH}/usr/lib/liborcania.a
#     ${SYSROOT_PATH}/usr/lib/libjansson.a
#     ${SYSROOT_PATH}/usr/lib/libjwt.a
#     ${SYSROOT_PATH}/usr/lib/libcurl.a
#     ${SYSROOT_PATH}/usr/lib/libgnutls.a
   
#     #${SYSROOT_PATH}/usr/lib/libcrypto.a
#     ${SYSROOT_PATH}/usr/lib/libz.a
#     ${SYSROOT_PATH}/usr/lib/libgmp.a
#     ${SYSROOT_PATH}/usr/lib/libhogweed.a
#     ${SYSROOT_PATH}/usr/lib/libnettle.a
#     ${SYSROOT_PATH}/usr/lib/librt.a
#     ${SYSROOT_PATH}/usr/lib/libdl.a
#     ${SYSROOT_PATH}/usr/lib/libutil.a
#     ${SYSROOT_PATH}/usr/lib/libm.a
#     ${SYSROOT_PATH}/usr/lib/libpthread.a
# )
target_link_libraries(test0904
    -Wl,-Bstatic  # 开始静态链接第三方库
    -Wl,--start-group 
    ${SYSROOT_PATH}/usr/lib/liborcania.a
    ${SYSROOT_PATH}/usr/lib/libmicrohttpd.a
    ${SYSROOT_PATH}/usr/lib/libulfius.a
    ${SYSROOT_PATH}/usr/lib/libjansson.a
    ${SYSROOT_PATH}/usr/lib/libjwt.a
    ${SYSROOT_PATH}/usr/lib/libcurl.a
    ${SYSROOT_PATH}/usr/lib/libgnutls.a
   
    #${SYSROOT_PATH}/usr/lib/libcrypto.a
    ${SYSROOT_PATH}/usr/lib/libz.a
    ${SYSROOT_PATH}/usr/lib/libgmp.a
    ${SYSROOT_PATH}/usr/lib/libhogweed.a
    ${SYSROOT_PATH}/usr/lib/libnettle.a
   -Wl,--end-group    
    -Wl,-Bdynamic # 切换到动态链接glibc相关库

    rt
    dl
    util
    m
    pthread
)
