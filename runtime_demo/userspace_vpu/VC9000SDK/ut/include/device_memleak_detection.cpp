/*
 *  device_memleak_detection.cpp
 *  Author: zhangbo
 *  Generated: 2024-12-18 14:48:29.332000
 *  ----------------------------------------------------------
 *  This file is used to detect memory leak in device side for every test case.
 */
#include "device_memleak_detection.hpp"

// class DetectionFixture {
// public:
//     DetectionFixture() {
//         runDetection(true);  // 开始检测
//     }

//     ~DetectionFixture() {
//         runDetection(false); // 结束检测
//     }

// private:
    void runDetection(bool isBefore) {
        if (isBefore) {
            printf("Running detection before test case.\n");
            sleep(1);  // 等待一段时间，等待内存回收
        } else {
            printf("Running detection after test case.\n");
            sleep(2);  // 等待一段时间，等待内存回收
        }

        const char* command = nullptr;

    #if defined __aarch64__
        printf("this is arm cpu\n");
        command = isBefore 
            ?  "/video-case/lowlevel_SDK/vatools_SV100_arm/vasmi dmon --loop 3 | awk -F'|' '{print $2}' | grep 835 | awk '{print $6}' > mem.log"
            :  "/video-case/lowlevel_SDK/vatools_SV100_arm/vasmi dmon --loop 3 | awk -F'|' '{print $2}' | grep 835 | awk '{print $6}' >> mem.log; sort -u mem.log | wc -l | grep -q '^1$' && echo '所有行相等' && exit 0 || (echo '行不相等' && exit 1)";
    #elif defined __x86_64__
        printf("this is x86 cpu\n");
        command = isBefore 
            ?  "/video-case/lowlevel_SDK/vatools_SV100/vasmi dmon --loop 3 | awk -F'|' '{print $2}' | grep 835 | awk '{print $6}' > mem.log"
            :  "/video-case/lowlevel_SDK/vatools_SV100/vasmi dmon --loop 3 | awk -F'|' '{print $2}' | grep 835 | awk '{print $6}' >> mem.log; sort -u mem.log | wc -l | grep -q '^1$' && echo '所有行相等' && exit 0 || (echo '行不相等' && exit 1)";
    #else
        fprintf(stderr, "Unsupported CPU architecture.\n");
        return;  // 早期退出，避免后续不必要的操作
    #endif

        int status = system(command);  // 仅在命令不为空时才调用

        if (status == -1) {
            perror("Failed to execute command");
            REQUIRE(false);  // 失败时退出测试
        } 
        
        if (WIFEXITED(status)) {
            printf("Command exited with status %d\n", WEXITSTATUS(status));
            REQUIRE(WEXITSTATUS(status) == 0);
        } else if (WIFSIGNALED(status)) {
            printf("Command terminated by signal %d\n", WTERMSIG(status));
        } else {
            printf("Command terminated abnormally.\n");
        }
    }
// };
