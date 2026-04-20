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

#ifndef USER_CALLBACK_H
#define USER_CALLBACK_H

#include "stm32h5xx_hal.h"

/* 声明全局标志，供驱动文件使用 */
extern volatile uint8_t smbus_tx_complete;
extern volatile uint8_t smbus_rx_complete;
extern volatile uint8_t smbus_error;

/* 回调函数结构体（带参数） */
typedef struct {
	void (*TxCpltCallback)(void*);   // 传输完成回调，参数为设备句柄
	void *TxCpltArg;                  // 传输完成回调参数
	void (*ErrorCallback)(void*);     // 错误回调，参数为设备句柄
	void *ErrorArg;                    // 错误回调参数
} DAC_SPI_Callbacks_t;

/* 注册回调函数（由驱动调用） */
void DAC_SPI_Callback_Register(DAC_SPI_Callbacks_t* cb);

/* 回调函数声明（无需外部调用，但可包含） */
void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi);
void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi);

#endif

