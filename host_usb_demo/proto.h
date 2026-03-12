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
#ifndef PROTO_H
#define PROTO_H

#include <stdint.h>

#pragma pack(push, 1)

// 电压配置命令（上位机 → 设备）
struct VoltageConfigPacket {
    uint8_t cmd_id;        // 命令ID: 0x01 = 设置电压
    uint8_t channel;       // 通道号
    uint16_t voltage_mv;   // 电压值(mV)
    uint16_t current_ma;   // 电流限制(mA)
    uint8_t reserved[58];  // 填充到64字节
};

// 采样数据包（设备 → 上位机）
struct SampleDataPacket {
    uint8_t report_id;     // 报告ID（通常为0）
    uint32_t timestamp;    // 时间戳
    uint16_t ch1_voltage;  // 通道1电压
    uint16_t ch1_current;  // 通道1电流
    uint16_t ch2_voltage;  // 通道2电压
    uint16_t ch2_current;  // 通道2电流
    uint8_t reserved[51];  // 填充到64字节
};

#pragma pack(pop)

// 打包电压配置命令（直接返回指针和长度，由调用者保证缓冲区足够）
void Protocol_PackVoltageConfig(VoltageConfigPacket* pkt, uint8_t channel, uint16_t voltage_mv, uint16_t current_ma);

// 解析采样数据包
void Protocol_ParseSampleData(const uint8_t* data, int length, SampleDataPacket* out);

#endif // PROTO_H