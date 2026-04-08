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
#ifndef STM32_COMM_H
#define STM32_COMM_H

#include <mutex>
#include <condition_variable>

#include "usb_driver.h"
#include "file_parse.h"
#include "power_config.h"

// 打包电压配置命令（直接返回指针和长度，由调用者保证缓冲区足够）
void Protocol_PackVoltageConfig(VoltageConfigPacket* pkt,uint8_t device_id ,uint8_t channel, uint16_t voltage_mv);
// 解析采样数据包
void Protocol_ParseSampleData(const uint8_t* data, int length, SampleDataPacket* out);
// 打包引脚配置命令
void Protocol_PackPinConfig(PinConfigPacket* pkt, uint8_t port, uint16_t pin, uint8_t level);
// 打包采样配置命令
void Protocol_PackSampleConfig(SampleConfigPacket* pkt, uint8_t state, uint8_t type);

void SendPowerOn(USBDriver& dev, PowersConfig* cfg);
void SendVoltageConfig(USBDriver& dev, PowerSupplyConfig* cfg);
void SendPinConfig(USBDriver& dev, int port, int pin, int level);
void SendStartSample(USBDriver& dev, int type);
void SendStopSample(USBDriver& dev, int type);
float GetActualVoltage(USBDriver& dev, int power_id);

#endif // STM32_COMM_H