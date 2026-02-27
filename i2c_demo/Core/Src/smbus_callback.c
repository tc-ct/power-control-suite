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

/*
 * smbus_callback.c
 * 全局 SMBUS 中断回调，供 INA238 和 INA260 驱动共用
 */

#include "stm32h5xx_hal.h"

/* 全局传输完成标志（volatile 供多个文件访问） */
volatile uint8_t smbus_tx_complete = 0;
volatile uint8_t smbus_rx_complete = 0;
volatile uint8_t smbus_error = 0;

void HAL_SMBUS_MasterTxCpltCallback(SMBUS_HandleTypeDef *hsmbus) {
    smbus_tx_complete = 1;
}

void HAL_SMBUS_MasterRxCpltCallback(SMBUS_HandleTypeDef *hsmbus) {
    smbus_rx_complete = 1;
}

void HAL_SMBUS_ErrorCallback(SMBUS_HandleTypeDef *hsmbus) {
    smbus_error = 1;
    smbus_tx_complete = 1;   /* 避免死等 */
    smbus_rx_complete = 1;
}