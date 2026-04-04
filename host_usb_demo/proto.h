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

/* 发送数据类型枚举 */
typedef enum {
    I2C_DATA_VBUS=0,       
    I2C_DATA_CURRENT,                
} I2C_Data_Type;

// 电压配置命令（上位机 → 设备）
struct __attribute__((packed)) VoltageConfigPacket{ 
    uint8_t  cmd_id;        // 命令ID: 0x01 = dac设置
    uint8_t  device_id;     // dac 设备id
    uint8_t  channel;       // 通道号
    uint16_t voltage_mv;    // 电压值(mV)
    uint8_t  reserved[59];  // 填充到64字节
          // 整个数据包的字节视图
};

// 引脚配置命令（上位机 → 设备）
struct __attribute__((packed)) PinConfigPacket{
    uint8_t  cmd_id;      // 0x04
    uint8_t  port;        // 端口编号（0=GPIOB, 1=GPIOC, ...）
    uint16_t pin;         // 引脚编号（0~15）
    uint8_t  level;       // 电平：0=低电平，1=高电平
    uint8_t  reserved[59]; // 填充到64字节（可根据实际USB报告大小调整）
} ;

// 采样数据包（设备 → 上位机）
struct SampleDataPacket {
    uint8_t report_id;     // 报告ID（通常为0）
    uint8_t type;
    uint32_t timestamp;    // 时间戳
    float i2c_data[27];  // 电压电流数据
    uint8_t  reserved[14]; // 填充到64字节
};

#pragma pack(pop)

// 打包电压配置命令（直接返回指针和长度，由调用者保证缓冲区足够）
void Protocol_PackVoltageConfig(VoltageConfigPacket* pkt,uint8_t device_id ,uint8_t channel, uint16_t voltage_mv);
// 解析采样数据包
void Protocol_ParseSampleData(const uint8_t* data, int length, SampleDataPacket* out);
// 打包引脚配置命令
void Protocol_PackPinConfig(PinConfigPacket* pkt, uint8_t port, uint16_t pin, uint8_t level);

#endif // PROTO_H