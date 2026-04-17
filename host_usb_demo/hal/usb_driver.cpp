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

#include "proto_pkg.h"
#include "usb_driver.h"
#include "log.h"
#include <hidapi.h>
#include <cstring>
#include <thread>
#include <chrono>
#include <cstdlib>

USBDriver::USBDriver(uint16_t vid, uint16_t pid)
    : vid_(vid), pid_(pid), handle(nullptr), running_(false) {
    if (hid_init() != 0) {
        LOG_ERROR("hid_init failed");
    }
}

USBDriver::~USBDriver() {
    close();
    hid_exit();
}

bool USBDriver::open() {
    auto devs = hid_enumerate(vid_, pid_);
    if (!devs) {
        LOG_ERROR("No device found with VID=0x%04X, PID=0x%04X", vid_, pid_);
        return false;
    }

    // 打印设备信息
    LOG_INFO("Found device(s):");
    for (auto* cur = devs; cur; cur = cur->next) {
        LOG_INFO("  Path: %ls", cur->path);
        LOG_INFO("  Manufacturer: %ls", cur->manufacturer_string ? cur->manufacturer_string : L"Unknown");
        LOG_INFO("  Product: %ls", cur->product_string ? cur->product_string : L"Unknown");
        LOG_INFO("  Interface: %d", cur->interface_number);
    }

    // 打开第一个设备
    handle = hid_open_path(devs->path);
    hid_free_enumeration(devs);

    if (!handle) {
        LOG_ERROR("Failed to open device");
        return false;
    }

    hid_set_nonblocking(static_cast<hid_device*>(handle), 1);
    LOG_INFO("Device opened successfully");
    return true;
}

void USBDriver::close() {
    running_ = false;
    if (receive_thread_.joinable()) {
        receive_thread_.join();
    }
    if (handle) {
        hid_close(static_cast<hid_device*>(handle));
        handle = nullptr;
    }
}

bool USBDriver::send(const uint8_t* data, size_t length) {
    if (!handle) {
        LOG_ERROR("Device not open");
        return false;
    }
    if (length > 64) {
        LOG_ERROR("Data length exceeds 64 bytes");
        return false;
    }

    uint8_t report[65] = {1};  // 报告 ID = 1
    memcpy(report + 1, data, length);

    int written = hid_write(static_cast<hid_device*>(handle), report, length + 1);
    if (written < 0) {
        const wchar_t* err = hid_error(static_cast<hid_device*>(handle));
        LOG_ERROR("hid_write failed: %ls", err);
        return false;
    }
  //  LOG_INFO("Sent %zu bytes", length);
    return true;
}

void USBDriver::setReceiveCallback(ReceiveCallback cb) {
    callback_ = cb;
    if (!running_) {
        running_ = true;
        receive_thread_ = std::thread(&USBDriver::receiveLoop, this);
    }
}

void USBDriver::receiveLoop() {
    uint8_t buffer[USB_REPORT_SIZE];
    while (running_) {
        int bytes = hid_read_timeout(static_cast<hid_device*>(handle), buffer, sizeof(buffer), USB_TIMEOUT_MS);
        if (bytes > 0 && callback_) {
            callback_(buffer, bytes);
        }
    }
}


int USBDriver::receive(const uint8_t* data, size_t length) {
    if (!handle) {
        return -1;
    }
    int bytes = hid_read_timeout(static_cast<hid_device*>(handle), const_cast<uint8_t*>(data), length, USB_TIMEOUT_MS);
    if (bytes > 0) {
        return bytes;  // 成功读取到数据
    }

    const wchar_t* err = hid_error(static_cast<hid_device*>(handle));
    LOG_ERROR("hid_read failed: %ls", err ? err : L"unknown");
    std::abort();
    return -1;  // 错误
}