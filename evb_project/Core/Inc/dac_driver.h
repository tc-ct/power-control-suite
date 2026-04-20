/*
 * Copyright (C) 2025- FlyingChip Technology (Shanghai) Co., Ltd.
 * All rights reserved.
 *
 * This file contains information that is proprietary to FlyingChip.
 * The holder of this file shall treat all information contained herein as
 * confidential, use the information only for its intended purpose, illustrate
 * the copyright of FlyingChip and not duplicate, disclose, modificate or
 * disseminate any of this information in any manner unless FlyingChip
 * has otherwise provided express written permission.
 * Use of the file may require a license of intellectual property from FlyingChip.
 * This file conveys no express or implied licenses to any intellectual property
 * rights belonging to FlyingChip.
 *
 * ALL INFORMATION CONTAINED IN THIS FILE IS FURNISHED "AS IS".
 * FlyingChip DISCLAIMS ANY AND ALL TYPES OF WARRANTIES, EXPRESS, IMPLIED, OR
 * STATUTORY, INCLUDING ANY IMPLIED WARRANTY OF MERCHANTABILITY OR FITNESS FOR
 * A PARTICULAR PURPOSE WITH RESPECT TO THE INFORMATION PROVIDE HEREUNDER.
 *
 * FlyingChip RESERVES ALL RIGHTS NOT EXPRESSLY GRANTED TO YOU HEREUNDER.
 */

#ifndef DAC_DRIVER_H
#define DAC_DRIVER_H

#include "stm32h5xx_hal.h"
#include "proto_pkg.h"

#define MAX_VOLTAGE_NUM 36

/* DAC芯片类型枚举 */
typedef enum {
	DAC_TYPE_7568,  // 8通道，32位帧
	DAC_TYPE_7563   // 双通道，24位帧
} DAC_Device_TypeDef;

/* DAC设备句柄结构体 */
typedef struct {
	DAC_Device_TypeDef type;            // 芯片类型
	GPIO_TypeDef *cs_port;       // 片选GPIO端口
	uint16_t cs_pin;             // 片选引脚号
	SPI_HandleTypeDef *hspi;     // 使用的SPI句柄
	volatile uint8_t tx_complete; // 传输完成标志
	uint8_t tx_buf[4];            // 发送缓冲区（最大4字节）
	uint8_t rx_buf[4];            // 接收缓冲区（最大4字节）
} DAC_HandleTypeDef;

/* DAC7568 通道枚举 A~H */
typedef enum {
	DAC7568_CH_A = 0,
	DAC7568_CH_B,
	DAC7568_CH_C,
	DAC7568_CH_D,
	DAC7568_CH_E,
	DAC7568_CH_F,
	DAC7568_CH_G,
	DAC7568_CH_H
} DAC7568_Channel_t;

/* DAC7563 通道枚举 A~B */
typedef enum {
	DAC7563_CH_A = 0,
	DAC7563_CH_B = 1
} DAC7563_Channel_t;

/* DAC7568 命令定义（写输入寄存器并更新所有DAC） */
#define DAC7568_CMD_WRITE_UPDATE_ALL   0x3
/* DAC7563 命令定义（写输入寄存器并更新所有DAC） */
#define DAC7563_CMD_WRITE_UPDATE_ALL   0x3
/* 传输超时 (ms) */
#define SPI_TIMEOUT          100

/* 函数声明 */
/* 显式禁用内部参考（使用外部参考） */
HAL_StatusTypeDef DAC_DisableInternalRef(DAC_HandleTypeDef* hdac);
/* 初始化DAC设备 */
HAL_StatusTypeDef DAC_Init(DAC_HandleTypeDef* hdac, DAC_Device_TypeDef type,
			   GPIO_TypeDef* cs_port, uint16_t cs_pin,
			   SPI_HandleTypeDef* hspi);

/* 设置DAC通道输出电压值（value为12位右对齐数据） */
HAL_StatusTypeDef DAC_SetVoltage(DAC_HandleTypeDef* hdac, uint8_t channel, uint16_t value);


HAL_StatusTypeDef DAC_VoltageConfig(VoltageConfigPacket_t *cfg);

HAL_StatusTypeDef DAC_PowerOnOff(uint8_t power);

#endif /* DAC_DRIVER_H */