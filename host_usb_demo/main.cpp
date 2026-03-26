/*
 * Copyright (C) 2024 - FlyingChip Technology (Shanghai) Co., Ltd. All rights reserved.
 *
 * This file contains information that is proprietary to FlyingChip.
 * The holder of this file shall treat all information contained herein as
 * confidential, use the information only for its intended purpose, illustrate
 * the copyright of FlyingChip and not duplicate, disclose, modificate or disseminate
 * any of this information in any manner unless FlyingChip has otherwise
 * provided express written permission.
 * Use of the file may require a license of intellectual property from FlyingChip.
 * This file conveys no express or implied licenses to any intellectual property
 * rights belonging to FlyingChip.
 *
 * ALL INFORMATION CONTAINED IN THIS FILE IS FURNISHED “AS IS”.
 * FLYINGCHIP DISCLAIMS ANY AND ALL TYPES OF WARRANTIES, EXPRESS, IMPLIED, OR
 * STATUTORY, INCLUDING ANY IMPLIED WARRANTY OF MERCHANTABILITY OR FITNESS FOR
 * A PARTICULAR PURPOSE WITH RESPECT TO THE INFORMATION PROVIDE HEREUNDER.
 *
 * FLYINGCHIP RESERVES ALL RIGHTS NOT EXPRESSLY GRANTED TO YOU HEREUNDER.
 */
#include <iostream>   // 仅用于 std::cin.get()
#include <thread>
#include <chrono>
#include <Windows.h>

#include "log.h"
#include "usb_driver.h"
#include "proto.h"

// 设备 VID/PID
constexpr uint16_t STM32_VID = 0x0483;
constexpr uint16_t STM32_PID = 0x5750;  // 根据实际修改

// 接收数据回调函数
void onDataReceived(const uint8_t* data, int length) {
    static int count = 0;
    LOG_INFO("RX #%d: %d bytes", ++count, length);

    // 尝试解析为采样数据包
    if (0) {
        SampleDataPacket pkt;
        Protocol_ParseSampleData(data, length, &pkt);
        LOG_INFO("  Sample: time=%u ms, CH1=%u mV/%u mA, CH2=%u mV/%u mA",
                 pkt.timestamp,
                 pkt.ch1_voltage, pkt.ch1_current,
                 pkt.ch2_voltage, pkt.ch2_current);
    } else {
        // 原始数据打印
        printf("  Raw: ");
        for (int i = 0; i < length; ++i) {
            printf("%02X ", data[i]);
        }
        printf("\n");
    }
}

int main(int argc, char* argv[]) {
    // 设置控制台输出为 UTF-8，支持中文
    SetConsoleOutputCP(CP_UTF8);
    // 可选：设置日志级别
    log_set_level(LOG_LEVEL_INFO);

    LOG_INFO("STM32 Custom HID Test Program");
    LOG_INFO("=============================");

    // 允许命令行指定 PID（十六进制）
    uint16_t pid = STM32_PID;
    if (argc > 1) {
        pid = static_cast<uint16_t>(std::stoi(argv[1], nullptr, 16));
    }

    USBDriver dev(STM32_VID, pid);
    if (!dev.open()) {
        LOG_ERROR("Cannot open device. Check VID/PID and connection.");
        LOG_ERROR("Press Enter to exit...");
        std::cin.get();
        return -1;
    }

    // 注册接收回调
    dev.setReceiveCallback(onDataReceived);

    // 测试1：发送电压配置命令
    LOG_INFO("Test 1: Configure voltage (channel 1, 3.3V)");
    VoltageConfigPacket cfg;
    Protocol_PackVoltageConfig(&cfg, 1, 3300, 500);
    dev.send(reinterpret_cast<uint8_t*>(&cfg), sizeof(cfg));
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // 测试2：发送启动采样命令
    LOG_INFO("Test 2: Start sampling");
    uint8_t start_cmd[] = {0x03, 0x01};   // 命令ID=0x03, 启动
    dev.send(start_cmd, sizeof(start_cmd));

    // 等待接收数据
    LOG_INFO("Waiting for data (10 seconds)...");
    std::this_thread::sleep_for(std::chrono::seconds(10));

    // 测试3：停止采样（可选）
    LOG_INFO("Test 3: Stop sampling");
    uint8_t stop_cmd[] = {0x03, 0x00};
    dev.send(stop_cmd, sizeof(stop_cmd));

    LOG_INFO("Test finished. Press Enter to exit...");
    std::cin.get();

    dev.close();
    return 0;
}