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

#ifndef PROTO_PKG_H
#define PROTO_PKG_H

#include <stdint.h>

/* 命令类型枚举 */
typedef enum {
    CMD_SET_VOLTAGE = 0x01,      // 设置电压以及上电时许
    CMD_GET_STATUS = 0x02,        // 查询状态
    CMD_START_SAMPLING = 0x03,    // 启动采样
    CMD_STOP_SAMPLING = 0x04,     // 停止采样
} CmdType_t;

// 电压配置命令（上位机 → 设备）
/* 上电时序配置结构 */
typedef struct __attribute__((packed)) {
    union {
        struct {
            uint8_t  cmd_id;        // 命令ID: 0x01 = dac设置
            uint8_t  device_id;     // dac 设备id
            uint16_t channel;       // 通道号
            uint16_t voltage_mv;    // 电压值(mV)
            uint16_t current_ma;    // 电流限制(mA)
            uint8_t  delay_ms;
            uint8_t  reserved[56];  // 填充到64字节
        };
        uint8_t bytes[64];           // 整个数据包的字节视图
    };
} VoltageConfigPacket_t;

/* 采样数据结构（STM32 -> PC） */
typedef struct {
    union {
        struct {
            uint8_t report_id;     // 报告ID（通常为0）
            uint8_t data_type;     // 数据类型
            uint32_t timestamp;           // 时间戳(ms)
            uint16_t channel_voltage_mv[28]; // 通道 电压
            uint16_t channel_current_ma[28]; // 通道 电流
        };
        uint8_t bytes[64];           // 整个数据包的字节视图
    };
} SampleData_t;


#endif // PROTO_PKG_H