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

#define USB_REPORT_SIZE                     128
#define DOWNLOAD_PACKET_SIZE                64
#define SAMPLE_DATA_COUNT                   27
#define POWER_SUPPLY_COUNT                  18

#pragma pack(push, 1)

/* 命令类型枚举 */
typedef enum {
    CMD_SET_VOLTAGE = 0x01,         // 设置电压
    CMD_SET_PIN = 0x02,             // 配置引脚
    CMD_START_SAMPLING = 0x03,      // 启动/停止采样
    CMD_GET_STATUS = 0x04,          // 查询状态   
    CMD_POWER_ON = 0x05,              // 上电命令并携带上电序列ID
    CMD_POWER_OFF = 0x06,             // 下电命令
    CMD_I2C_WRITE = 0x07,             // I2C写命令
    CMD_I2C_READ = 0x08,              // I2C读命令
    CMD_SPI_WRITE = 0x09,             // SPI写命令
    CMD_SPI_READ = 0x0A,              // SPI读命令
} CmdType_t;


/* 发送数据类型枚举 */
typedef enum {
    I2C_DATA_VBUS=0,       
    I2C_DATA_CURRENT,                
} I2C_Data_Type;

typedef enum {
    SAMPLE_TYPE_VOLTAGE = 4,
    SAMPLE_TYPE_CURRENT = 5,
} SampleType;

typedef enum {
    SAMPLE_STATE_STOP = 0,
    SAMPLE_STATE_START = 1,
} SampleState;


// 引脚配置命令（上位机 → 设备）
typedef struct PinConfigPacket{
union {
struct {
    uint8_t  cmd_id;      // 0x04
    uint8_t  port;        // 端口编号（0=GPIOB, 1=GPIOC, ...）
    uint8_t  pin;         // 引脚编号（0~15）
    uint8_t  level;       // 电平：0=低电平，1=高电平
};
uint8_t bytes[DOWNLOAD_PACKET_SIZE];           // 整个数据包的字节视图
};
} PinConfigPacket_t;

// 上电时序配置结构
typedef struct SequenceConfigPacket{
union {
struct {
    uint8_t     cmd_id;                                     // 命令ID: 0x05 = 上电命令
    uint8_t     sequence[POWER_SUPPLY_COUNT];               // 0~17的排列组合，表示上电顺序
    uint16_t    interval_ms[POWER_SUPPLY_COUNT];            // 上电间隔，单位ms
};
uint8_t bytes[DOWNLOAD_PACKET_SIZE];           // 整个数据包的字节视图
};
} SequenceConfigPacket_t;



// 引脚采样命令（上位机 → 设备）
typedef struct SampleConfigPacket{
union {
struct {
    uint8_t  cmd_id;      // 命令ID: CMD_START_SAMPLING
    uint8_t  state;       // 开启关闭采样
    uint8_t  type;        // 采样类型
    uint8_t  volt_en;     // 是否采样电压
    uint8_t  current_en;  // 是否采样电流
};
uint8_t bytes[DOWNLOAD_PACKET_SIZE];           // 整个数据包的字节视图
};
} SampleConfigPacket_t;


// 电压配置命令（上位机 → 设备）
/* 上电时序配置结构 */
typedef struct VoltageConfigPacket{
union {
struct {
    uint8_t     cmd_id;                 // 命令ID: 0x01 = volt设置
    uint8_t     device_id;              // dac 设备id
    uint16_t    channel;                // 通道号
    uint16_t    dac_value;              // DAC值
    uint16_t    current_ma_limit;       // 电流限制(mA)
    uint16_t    interval_ms;            // 可选的电压配置间隔，单位ms
    uint16_t    sequence_id;            // 可选的上电序列ID，0~POWER_SUPPLY_COUNT-1
    uint16_t    enable_pin;             // 使能引脚编号
    uint16_t    pin_port;               // 使能引脚端口
};
uint8_t bytes[DOWNLOAD_PACKET_SIZE];           // 整个数据包的字节视图
};
} VoltageConfigPacket_t;

/* 采样数据结构（STM32 -> PC） */
typedef struct SampleDataPacket{
union {
struct {
    uint8_t report_id;     // 报告ID（通常为0）
    uint8_t type;     // 数据类型
    uint32_t timestamp;           // 时间戳(ms)
    uint16_t channel_volt_mv[SAMPLE_DATA_COUNT]; // 通道 电压
    uint16_t channel_curr_ma[SAMPLE_DATA_COUNT]; // 通道 电流
};
uint8_t bytes[USB_REPORT_SIZE];           // 整个数据包的字节视图
};
} SampleDataPacket_t;


#pragma pack(pop)

#endif // PROTO_PKG_H