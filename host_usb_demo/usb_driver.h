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
#ifndef USB_DRIVER_H
#define USB_DRIVER_H

#include <stdint.h>
#include <functional>
#include <thread>
#include <atomic>
#include <memory>

// 接收回调函数类型
using ReceiveCallback = std::function<void(const uint8_t* data, int length)>;

class USBDriver {
public:
    USBDriver(uint16_t vid, uint16_t pid);
    ~USBDriver();

    // 打开设备，成功返回 true
    bool open();

    // 关闭设备并停止接收线程
    void close();

    // 检查设备是否已打开
    bool isOpen() const { return handle != nullptr; }

    // 发送数据（最大 64 字节），内部自动添加报告 ID（固定为 1）
    bool send(const uint8_t* data, size_t length);

    // 注册接收数据回调，并启动内部接收线程
    void setReceiveCallback(ReceiveCallback cb);

    // 获取底层设备句柄（仅供测试使用）
    void* getNativeHandle() const { return handle; }

    int receive(const uint8_t* data, size_t length);

private:
    uint16_t vid_;
    uint16_t pid_;
    void* handle;                 // hid_device*，使用 void* 避免暴露 hidapi 头
    std::atomic<bool> running_;
    std::thread receive_thread_;
    ReceiveCallback callback_;

    void receiveLoop();
};

#endif // USB_DRIVER_H